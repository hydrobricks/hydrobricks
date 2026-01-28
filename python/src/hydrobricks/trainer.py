from __future__ import annotations
import importlib
import logging
import os
from datetime import datetime
from typing import TYPE_CHECKING, Any, Callable

import numpy as np

from hydrobricks import spotpy
from hydrobricks._exceptions import DataError, ModelError

logger = logging.getLogger(__name__)

if TYPE_CHECKING:
    from hydrobricks.forcing import Forcing
    from hydrobricks.models.model import Model
    from hydrobricks.observations import Observations
    from hydrobricks.parameters import ParameterSet


class SpotpySetup:
    """Setup class for SPOTPY optimization framework integration."""

    def __init__(
            self,
            model: Model | list[Model],
            params: ParameterSet,
            forcing: Forcing | list[Forcing],
            obs: Observations | list[Observations],
            warmup: int = 365,
            obj_func: str | Callable[[np.ndarray, np.ndarray], float] | None = None,
            invert_obj_func: bool = False,
            dump_outputs: bool = False,
            dump_forcing: bool = False,
            dump_dir: str = ''
    ) -> None:
        """
        Initialize SPOTPY setup for model calibration.

        Parameters
        ----------
        model
            Single Model instance or list of Model instances for ensemble calibration.
        params
            ParameterSet defining the model parameters to calibrate.
        forcing
            Single Forcing instance or list of Forcing instances (one per model).
        obs
            Single Observations instance or list for observed data (one per model).
        warmup
            Number of days for model warmup period (skipped in evaluation).
            Default: 365
        obj_func
            Objective function for optimization. If None, uses non-parametric
            Kling-Gupta Efficiency. Can be a string (HydroErr function name) or
            callable that takes (observed, simulated) and returns a scalar.
            Default: None
        invert_obj_func
            If True, multiply objective function result by -1 (for minimizers).
            Default: False
        dump_outputs
            If True, save all simulation outputs to disk. Default: False
        dump_forcing
            If True, save forcing data used in simulations. Default: False
        dump_dir
            Directory path for saving dumped outputs. Default: '' (current directory)

        Raises
        ------
        ValueError
            If the number of models, forcing instances, or observations don't match.
        RuntimeError
            If parameter constraints cannot be satisfied.
        """
        self.model: list[Model] = [model] if not isinstance(model, list) else model
        self.params: ParameterSet = params
        self.params_spotpy: Any = params.get_for_spotpy()
        self.random_forcing: bool = params.needs_random_forcing()
        self.forcing: list[Forcing] = [forcing] if not isinstance(forcing, list) else forcing
        for f in self.forcing:
            f.apply_operations(params)
        if not isinstance(obs, list):
            self.obs: list[np.ndarray] = [obs.data[0]]
        else:
            self.obs: list[np.ndarray] = [o.data[0] for o in obs]
        self.warmup: int = warmup
        self.obj_func: str | Callable[[np.ndarray, np.ndarray], float] | None = obj_func
        self.invert_obj_func: bool = invert_obj_func
        self.dump_outputs: bool = dump_outputs
        self.dump_forcing: bool = dump_forcing
        self.dump_dir: str = dump_dir

        # Check that the models, forcing and the observations have the same length
        if len(self.model) != len(self.forcing) or len(self.model) != len(self.obs):
            raise ConfigurationError(
                'The number of models, forcing and observations must be the same.',
                reason='Mismatched ensemble sizes'
            )

        if not self.random_forcing:
            for m, f in zip(self.model, self.forcing):
                m.set_forcing(forcing=f)

        if not obj_func:
            logger.info("Objective function: Non parametric Kling-Gupta Efficiency.")

    def parameters(self) -> Any:
        """
        Generate random parameter sets that satisfy constraints.

        Returns
        -------
        Any
            SPOTPY parameter object with valid random parameter values.

        Raises
        ------
        RuntimeError
            If unable to generate valid parameters after 1000 attempts.
        """
        x = None
        for i in range(1000):
            x = spotpy.parameter.generate(self.params_spotpy)
            names = [row[1] for row in x]
            values = [row[0] for row in x]
            params = self.params
            param_values = dict(zip(names, values))
            params.set_values(param_values, check_range=False)

            if params.constraints_satisfied() and params.range_satisfied():
                break

            if i >= 1000:
                raise ModelError('The parameter constraints could not be satisfied.')

        return x

    def simulation(self, x: spotpy.parameter) -> list[np.ndarray] | None:
        """
        Run model simulation with given parameter set.

        Parameters
        ----------
        x
            SPOTPY parameter object containing parameter values.

        Returns
        -------
        list[np.ndarray] | None
            List of simulated discharge time series (one per model), or None
            if simulation failed or constraints were violated.
        """
        params = self.params
        param_values = dict(zip(x.name, x.random))
        params.set_values(param_values)

        if not params.constraints_satisfied() or not params.range_satisfied():
            return None

        all_sim = []
        for model, forcing, i in zip(self.model, self.forcing, range(len(self.model))):
            if self.random_forcing:
                forcing.apply_operations(params, apply_to_all=False)
                model.run(parameters=params, forcing=forcing)
            else:
                model.run(parameters=params)

            sim = model.get_outlet_discharge()
            if sim.size == 0:
                return None

            all_sim.append(sim[self.warmup:])

            if self.dump_outputs or self.dump_forcing:
                now = datetime.now()
                date_time = now.strftime("%Y-%m-%d_%H%M%S")
                path = os.path.join(self.dump_dir, date_time)
                os.makedirs(path, exist_ok=True)
                if self.dump_outputs:
                    model.dump_outputs(path)
                if self.dump_forcing:
                    forcing.save_as(os.path.join(path, f'forcing_{i}.nc'))

        return all_sim

    def evaluation(self) -> list[np.ndarray]:
        all_obs = []
        for obs in self.obs:
            all_obs.append(obs[self.warmup:])

        return all_obs

    def objectivefunction(
            self,
            simulation: list[np.ndarray],
            evaluation: list[np.ndarray],
            params: spotpy.parameter | None = None
    ) -> float:
        if simulation is None:
            logger.error(f'Simulation results empty (params: {params}).')
            if self.invert_obj_func:
                return np.inf
            return -np.inf

        all_like = []
        for sim, obs in zip(simulation, evaluation):
            if sim is None or sim.size == 0:
                logger.error(f'Simulation results empty (params: {params}).')
                like = -np.inf
            elif len(sim) != len(obs):
                logger.error(
                    f'Length of simulation and observation do not match '
                    f'(params: {params}).'
                )
                like = -np.inf
            else:
                if not self.obj_func:
                    like = spotpy.objectivefunctions.kge_non_parametric(obs, sim)
                elif isinstance(self.obj_func, str):
                    like = evaluate(sim, obs, self.obj_func)
                else:
                    like = self.obj_func(obs, sim)

            if self.invert_obj_func:
                like = -like

            all_like.append(like)

        # Return the mean of the objective function
        return np.mean(all_like)


def evaluate(simulation: np.array, observation: np.array, metric: str) -> float:
    """
    Evaluate the simulation using the provided metric (goodness of fit).

    Parameters
    ----------
    simulation
        The predicted time series.
    observation
        The time series of the observations with dates matching the simulated
        series.
    metric
        The abbreviation of the function as defined in HydroErr
        (https://hydroerr.readthedocs.io/en/stable/list_of_metrics.html)
        Examples: nse, kge_2012, ...

    Returns
    -------
    The value of the selected metric.
    """
    eval_fct = getattr(importlib.import_module('HydroErr'), metric)

    return eval_fct(simulation, observation)
