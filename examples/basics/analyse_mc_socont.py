import matplotlib.pyplot as plt
import spotpy
from examples._helpers.models_setup_helper import ModelSetupHelper

import hydrobricks as hb
import hydrobricks.trainer as trainer

# Set up the model
helper = ModelSetupHelper(
    'ch_sitter_appenzell',
    start_date='1981-01-01',
    end_date='2020-12-31'
)
helper.create_hydro_units_from_csv_file(
    filename='hydro_units_elevation.csv'
)
forcing = helper.get_forcing_data_from_csv_file(
    ref_elevation=1253,
    use_precip_gradient=True
)
obs = helper.get_obs_data_from_csv_file()
socont, parameters = helper.get_model_and_params_socont()

# Select the parameters to optimize/analyze
parameters.allow_changing = [
    'a_snow',
    'k_quick',
    'A',
    'k_slow_1',
    'percol',
    'k_slow_2',
    'precip_corr_factor'
]

# Set a specific prior distribution instead of the default (Uniform)
parameters.set_prior(
    'a_snow',
    spotpy.parameter.Normal(mean=4, stddev=2)
)

# Setup SPOTPY
spot_setup = trainer.SpotpySetup(
    socont,
    parameters,
    forcing,
    obs,
    warmup=365,
    obj_func=spotpy.objectivefunctions.nashsutcliffe
)

# Select number of runs and run spotpy
nb_runs = 10000
sampler = spotpy.algorithms.mc(
    spot_setup,
    dbname='spotpy_socont_sitter_MC',
    dbformat='csv',
    save_sim=False
)
sampler.sample(nb_runs)

# Load the results
results = sampler.getdata()

# Plot parameter interaction
spotpy.analyser.plot_parameterInteraction(results)
plt.tight_layout()
plt.show()

# Plot posterior parameter distribution
posterior = spotpy.analyser.get_posterior(
    results,
    percentage=10
)
spotpy.analyser.plot_parameterInteraction(posterior)
plt.tight_layout()
plt.show()
