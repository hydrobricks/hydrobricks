import spotpy


class SpotpySetup:

    def __init__(self, model, params, forcing, obs, warmup=365, obj_func=None,
                 invert_obj_func=False):
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
            params.set_values(param_values)

            if params.are_constraints_satisfied():
                break

            if i >= 1000:
                raise RuntimeError('The parameter constraints could not be satisfied.')

        return x

    def simulation(self, x):
        params = self.params
        param_values = dict(zip(x.name, x.random))
        params.set_values(param_values)
        model = self.model
        if self.random_forcing:
            forcing = self.forcing
            forcing.apply_defined_spatialization(params, params.allow_changing)
            model.run(parameters=params, forcing=forcing)
        else:
            model.run(parameters=params)
        sim = model.get_outlet_discharge()

        return sim[self.warmup + 1:]

    def evaluation(self):
        return self.obs[self.warmup + 1:]

    def objectivefunction(self, simulation, evaluation, params=None):
        if not self.obj_func:
            like = spotpy.objectivefunctions.kge_non_parametric(evaluation, simulation)
        else:
            like = self.obj_func(evaluation, simulation)
            if self.invert_obj_func:
                like = -like
        return like
