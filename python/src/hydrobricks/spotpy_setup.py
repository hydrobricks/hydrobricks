import importlib
from datetime import datetime

import HydroErr
import spotpy


class SpotpySetup:

    def __init__(self, model, params, forcing, obs, warmup=365, obj_func=None,
                 invert_obj_func=False, dump_outputs=False, dump_dir=''):
        self.model = model
        self.params = params
        self.params_spotpy = params.get_for_spotpy()
        self.random_forcing = params.needs_random_forcing()
        self.forcing = forcing
        self.forcing.apply_defined_spatialization(params)
        self.obs = obs.data_raw[0]
        self.warmup = warmup
        self.obj_func = obj_func
        self.invert_obj_func = invert_obj_func
        self.dump_outputs = dump_outputs
        self.dump_dir = dump_dir
        if not self.random_forcing:
            self.model.set_forcing(forcing=forcing)
        if not obj_func:
            print("Objective function: Non parametric Kling-Gupta Efficiency.")

    def parameters(self):
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
                raise RuntimeError('The parameter constraints could not be satisfied.')

        return x

    def simulation(self, x):
        params = self.params
        param_values = dict(zip(x.name, x.random))
        params.set_values(param_values)

        if not params.constraints_satisfied() or not params.range_satisfied():
            raise RuntimeError('The parameter constraints are not satisfied.')

        model = self.model
        if self.random_forcing:
            forcing = self.forcing
            forcing.apply_defined_spatialization(params, params.allow_changing)
            model.run(parameters=params, forcing=forcing)
        else:
            model.run(parameters=params)
        sim = model.get_outlet_discharge()

        if self.dump_outputs:
            now = datetime.now()
            date_time = now.strftime("%Y-%m-%d_%H%M%S")
            model.dump_outputs(self.dump_dir + date_time)

        return sim[self.warmup:]

    def evaluation(self):
        return self.obs[self.warmup:]

    def objectivefunction(self, simulation, evaluation, params=None):
        if not self.obj_func:
            like = spotpy.objectivefunctions.kge_non_parametric(evaluation, simulation)
        elif isinstance(self.obj_func, str):
            eval_fct = getattr(importlib.import_module('HydroErr'), self.obj_func)
            like = eval_fct(simulation, evaluation)
        else:
            like = self.obj_func(evaluation, simulation)

        if self.invert_obj_func:
            like = -like

        return like
