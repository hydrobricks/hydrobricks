import spotpy


class SpotpySetup:

    def __init__(self, model, params, forcing, obs, warmup=365, obj_func=None):
        self.model = model
        self.params = params
        self.params_spotpy = params.get_for_spotpy()
        self.random_forcing = params.needs_random_forcing()
        self.forcing = forcing
        self.forcing.apply_defined_spatialization(params)
        self.obs = obs.data_raw[0]
        self.warmup = warmup
        self.obj_func = obj_func
        if not self.random_forcing:
            self.model.set_forcing(forcing=forcing)

    def parameters(self):
        return spotpy.parameter.generate(self.params_spotpy)

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
            like = spotpy.objectivefunctions.nashsutcliffe(evaluation, simulation)
        else:
            like = self.obj_func(evaluation, simulation)
        return like
