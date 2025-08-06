from __future__ import annotations

import importlib
import os
from datetime import datetime
from typing import TYPE_CHECKING

import numpy as np

import hydrobricks as hb

if TYPE_CHECKING:
    from hydrobricks.forcing import Forcing
    from hydrobricks.models.model import Model
    from hydrobricks.observations import Observations
    from hydrobricks.parameters import ParameterSet


class SpotpySetup:

    def __init__(
            self,
            model: Model,
            params: ParameterSet,
            forcing: Forcing,
            obs: Observations,
            warmup: int = 365,
            obj_func: str | callable | None = None,
            invert_obj_func: bool = False,
            dump_outputs: bool = False,
            dump_forcing: bool = False,
            dump_dir: str = ''
    ):
        self.model = [model] if not isinstance(model, list) else model
        self.params = params
        self.params_spotpy = params.get_for_spotpy()
        self.random_forcing = params.needs_random_forcing()
        self.forcing = [forcing] if not isinstance(forcing, list) else forcing
        for f in self.forcing:
            f.apply_operations(params)
        if not isinstance(obs, list):
            self.obs = [obs.data[0]]
        else:
            self.obs = [o.data[0] for o in obs]
        self.warmup = warmup
        self.obj_func = obj_func
        self.invert_obj_func = invert_obj_func
        self.dump_outputs = dump_outputs
        self.dump_forcing = dump_forcing
        self.dump_dir = dump_dir

        # Check that the models, forcing and the observations have the same length
        if len(self.model) != len(self.forcing) or len(self.model) != len(self.obs):
            raise ValueError('The number of models, forcing and observations '
                             'must be the same.')

        if not self.random_forcing:
            for m, f in zip(self.model, self.forcing):
                m.set_forcing(forcing=f)

        if not obj_func:
            print("Objective function: Non parametric Kling-Gupta Efficiency.")

    def parameters(self) -> hb.spotpy.parameter:
        x = None
        for i in range(1000):
            x = hb.spotpy.parameter.generate(self.params_spotpy)
            names = [row[1] for row in x]
            values = [row[0] for row in x]
            params = self.params
            param_values = dict(zip(names, values))
            params.set_values(param_values, check_range=False)

            if params.constraints_satisfied() and params.range_satisfied():
                break

            if i >= 1000:
                raise RuntimeError('The parameter constraints could not be satisfied.')

        return x

    def simulation(self, x: hb.spotpy.parameter) -> list[np.ndarray] | None:
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
            params: hb.spotpy.parameter | None = None
    ) -> float:
        if simulation is None:
            print(f'Simulation results empty (params: {params}).')
            if self.invert_obj_func:
                return np.inf
            return -np.inf

        all_like = []
        for sim, obs in zip(simulation, evaluation):
            if sim is None or sim.size == 0:
                print(f'Simulation results empty (params: {params}).')
                like = -np.inf
            elif len(sim) != len(obs):
                print(f'Length of simulation and observation do not match '
                      f'(params: {params}).')
                like = -np.inf
            else:
                if not self.obj_func:
                    like = hb.spotpy.objectivefunctions.kge_non_parametric(obs, sim)
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
