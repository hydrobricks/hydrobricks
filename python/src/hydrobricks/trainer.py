from __future__ import annotations
import importlib
import logging
import os
from datetime import datetime
from typing import TYPE_CHECKING, Any, Callable

import numpy as np

from hydrobricks import spotpy
from hydrobricks._exceptions import DataError, ModelError, ConfigurationError

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
        ConfigurationError
            If invalid configuration parameters or mismatched ensemble sizes.
        DataError
            If observation data is invalid or missing.
        """
        # Validate and normalize inputs
        self.model = self._normalize_model_list(model)
        self.params = params
        self.forcing = self._normalize_forcing_list(forcing)
        self.obs = self._normalize_observations_list(obs)

        # Validate ensemble sizes match
        self._validate_ensemble_sizes()

        # Validate and set configuration parameters
        self._validate_and_set_parameters(warmup, dump_dir, invert_obj_func,
                                         dump_outputs, dump_forcing)

        # Initialize SPOTPY parameters
        self.params_spotpy = params.get_for_spotpy()
        self.random_forcing = params.needs_random_forcing()

        # Setup forcing and models
        self._setup_forcing_and_models()

        # Validate objective function
        self._validate_and_set_objective_function(obj_func)

    def _normalize_model_list(self, model: Model | list[Model]) -> list[Model]:
        """
        Validate and normalize model input to a list.

        Parameters
        ----------
        model
            Single Model instance or list of Model instances.

        Returns
        -------
        list[Model]
            Validated list of Model instances.

        Raises
        ------
        ConfigurationError
            If model is None or empty list.
        """
        if model is None:
            raise ConfigurationError(
                'Model cannot be None.',
                item_name='model',
                reason='Required parameter missing'
            )
        models = [model] if not isinstance(model, list) else model
        if len(models) == 0:
            raise ConfigurationError(
                'Model list cannot be empty.',
                item_name='model',
                reason='Empty model list'
            )
        return models

    def _normalize_forcing_list(
            self,
            forcing: Forcing | list[Forcing]
    ) -> list[Forcing]:
        """
        Validate and normalize forcing input to a list.

        Parameters
        ----------
        forcing
            Single Forcing instance or list of Forcing instances.

        Returns
        -------
        list[Forcing]
            Validated list of Forcing instances.

        Raises
        ------
        ConfigurationError
            If forcing is None or empty list.
        """
        if forcing is None:
            raise ConfigurationError(
                'Forcing cannot be None.',
                item_name='forcing',
                reason='Required parameter missing'
            )
        forcings = [forcing] if not isinstance(forcing, list) else forcing
        if len(forcings) == 0:
            raise ConfigurationError(
                'Forcing list cannot be empty.',
                item_name='forcing',
                reason='Empty forcing list'
            )
        return forcings

    def _normalize_observations_list(
            self,
            obs: Observations | list[Observations]
    ) -> list[np.ndarray]:
        """
        Validate and normalize observations to a list of numpy arrays.

        Parameters
        ----------
        obs
            Single Observations instance or list of Observations instances.

        Returns
        -------
        list[np.ndarray]
            Validated list of observation data arrays.

        Raises
        ------
        ConfigurationError
            If obs is None or empty list.
        DataError
            If observation data is missing, invalid, or empty.
        """
        if obs is None:
            raise ConfigurationError('Observations cannot be None.',
                                   item_name='obs', reason='Required parameter missing')
        obs_list = [obs] if not isinstance(obs, list) else obs
        if len(obs_list) == 0:
            raise ConfigurationError('Observations list cannot be empty.',
                                   item_name='obs', reason='Empty observations list')

        observations = []
        for idx, o in enumerate(obs_list):
            if not hasattr(o, 'data') or o.data is None:
                raise DataError(
                    f'Observations at index {idx} has no data attribute.',
                    data_type='observations',
                    reason='Missing or invalid observation data'
                )
            if len(o.data) == 0:
                raise DataError(
                    f'Observations at index {idx} has empty data.',
                    data_type='observations',
                    reason='Empty observation dataset'
                )
            observations.append(o.data[0])
        return observations

    def _validate_ensemble_sizes(self) -> None:
        """
        Validate that models, forcing, and observations have matching sizes.

        Checks that the number of models equals the number of forcing instances
        and the number of observation datasets. Raises specific errors if sizes
        don't match.

        Raises
        ------
        ConfigurationError
            If len(models) != len(forcing) or len(models) != len(observations),
            with details about which counts don't match.
        """
        num_models = len(self.model)
        num_forcing = len(self.forcing)
        num_obs = len(self.obs)

        if num_models != num_forcing:
            raise ConfigurationError(
                f'Number of models ({num_models}) does not match '
                f'number of forcing datasets ({num_forcing}).',
                reason='Mismatched ensemble sizes')
        if num_models != num_obs:
            raise ConfigurationError(
                f'Number of models ({num_models}) does not match '
                f'number of observation datasets ({num_obs}).',
                reason='Mismatched ensemble sizes')

    def _validate_and_set_parameters(
            self,
            warmup: int,
            dump_dir: str,
            invert_obj_func: bool,
            dump_outputs: bool,
            dump_forcing: bool
    ) -> None:
        """
        Validate and set configuration parameters.

        Validates the warmup period and dump directory parameters, then sets
        all configuration attributes on the instance.

        Parameters
        ----------
        warmup
            Number of days for model warmup period. Must be non-negative integer.
        dump_dir
            Directory path for saving dumped outputs. Only validated/created
            if dump_outputs or dump_forcing is True.
        invert_obj_func
            If True, multiply objective function result by -1 (for minimizers).
        dump_outputs
            If True, save all simulation outputs to disk.
        dump_forcing
            If True, save forcing data used in simulations.

        Raises
        ------
        ConfigurationError
            If warmup is not a non-negative integer or if dump_dir cannot be
            created/accessed.
        """
        if not isinstance(warmup, int) or warmup < 0:
            raise ConfigurationError(
                f'Warmup period must be non-negative integer, got {warmup}.',
                item_name='warmup',
                item_value=warmup,
                reason='Invalid warmup configuration'
            )
        self.warmup = warmup

        if (dump_outputs or dump_forcing) and dump_dir:
            try:
                dump_path = os.path.abspath(dump_dir)
                os.makedirs(dump_path, exist_ok=True)
                self.dump_dir = dump_path
            except (OSError, PermissionError) as e:
                raise ConfigurationError(
                    f'Cannot create dump directory "{dump_dir}": {e}',
                    item_name='dump_dir',
                    item_value=dump_dir,
                    reason='Directory access error'
                ) from e
        else:
            self.dump_dir = ''

        self.dump_outputs = dump_outputs
        self.dump_forcing = dump_forcing
        self.invert_obj_func = invert_obj_func

    def _setup_forcing_and_models(self) -> None:
        """
        Apply forcing operations and configure models.

        Applies parameter-dependent operations to all forcing datasets and
        configures models with forcing data if not using random forcing.

        Side Effects
        -----------
        - Modifies all forcing datasets via apply_operations()
        - Configures models with forcing via set_forcing() (if not random_forcing)

        Raises
        ------
        ModelError
            If forcing operations or model configuration fails. Original exception
            is preserved via exception chaining.
        """
        try:
            for f in self.forcing:
                f.apply_operations(self.params)
            if not self.random_forcing:
                for m, f in zip(self.model, self.forcing):
                    m.set_forcing(forcing=f)
        except Exception as e:
            raise ModelError(f'Failed to setup forcing and models: {e}',
                           is_initialized=False) from e

    def _validate_and_set_objective_function(
            self,
            obj_func: str | Callable[[np.ndarray, np.ndarray], float] | None
    ) -> None:
        """
        Validate and set the objective function.

        Parameters
        ----------
        obj_func
            Objective function specification. Can be:
            - None: Use default non-parametric Kling-Gupta Efficiency
            - str: Name of HydroErr metric (e.g., 'nse', 'kge_2012')
            - callable: User-defined function with signature (observed, simulated) -> float

        Raises
        ------
        ConfigurationError
            If obj_func is not None, string, or callable.
        """
        if obj_func is None:
            logger.info("Objective function: Non parametric Kling-Gupta Efficiency")
            self.obj_func = None
        elif isinstance(obj_func, str):
            self.obj_func = obj_func
        elif callable(obj_func):
            self.obj_func = obj_func
        else:
            raise ConfigurationError(
                f'Objective function must be None, string, or callable, '
                f'got {type(obj_func).__name__}.',
                item_name='obj_func',
                item_value=obj_func,
                reason='Invalid objective function type'
            )

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
        logger.debug(f"Setting {len(param_values)} parameter values for simulation")
        params.set_values(param_values)

        if not params.constraints_satisfied():
            logger.debug("Parameter constraints not satisfied, skipping simulation")
            return None

        if not params.range_satisfied():
            logger.debug("Parameter ranges not satisfied, skipping simulation")
            return None

        logger.debug(f"Running {len(self.model)} model(s)")
        all_sim = []
        for model_idx, (model, forcing) in enumerate(zip(self.model, self.forcing)):
            logger.debug(f"  Model {model_idx}: running simulation")

            if self.random_forcing:
                logger.debug(f"    Applying random forcing operations")
                forcing.apply_operations(params, apply_to_all=False)
                model.run(parameters=params, forcing=forcing)
            else:
                model.run(parameters=params)

            sim = model.get_outlet_discharge()
            if sim.size == 0:
                logger.warning(f"  Model {model_idx}: no outlet discharge data")
                return None

            logger.debug(f"  Model {model_idx}: extracted {len(sim)} discharge values")
            all_sim.append(sim[self.warmup:])

            if self.dump_outputs or self.dump_forcing:
                now = datetime.now()
                date_time = now.strftime("%Y-%m-%d_%H%M%S")
                path = os.path.join(self.dump_dir, date_time)
                os.makedirs(path, exist_ok=True)
                if self.dump_outputs:
                    logger.debug(f"    Dumping outputs to {path}")
                    model.dump_outputs(path)
                if self.dump_forcing:
                    logger.debug(f"    Saving forcing to {path}")
                    forcing.save_as(os.path.join(path, f'forcing_{i}.nc'))

        logger.debug(f"Simulation complete: {len(all_sim)} models processed")
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
