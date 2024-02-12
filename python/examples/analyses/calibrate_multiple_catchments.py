import matplotlib.pyplot as plt
import spotpy
from examples._helpers.models_setup_helper import ModelSetupHelper

import hydrobricks as hb

# Set up the model for the Sitter
helper_sitter = ModelSetupHelper('ch_sitter_appenzell', start_date='1981-01-01',
                                 end_date='2020-12-31')
helper_sitter.create_hydro_units_from_csv_file()
forcing_sitter = helper_sitter.get_forcing_data_from_csv_file(
    ref_elevation=1250, use_precip_gradient=True)
obs_sitter = helper_sitter.get_obs_data_from_csv_file()
socont_sitter, _ = helper_sitter.get_model_and_params_socont()

# Set up the model for the Rhone at Gletsch
helper_rhone = ModelSetupHelper('ch_sitter_appenzell', start_date='1981-01-01',
                                end_date='2020-12-31')
helper_rhone.create_hydro_units_from_csv_file()
forcing_rhone = helper_rhone.get_forcing_data_from_csv_file(
    ref_elevation=1250, use_precip_gradient=True)
obs_rhone = helper_rhone.get_obs_data_from_csv_file()
socont_rhone, parameters = helper_rhone.get_model_and_params_socont()

# Select the parameters to optimize/analyze
parameters.allow_changing = ['a_snow', 'k_quick', 'A', 'k_slow_1', 'percol', 'k_slow_2',
                             'precip_corr_factor']

# Setup SPOTPY (we need to invert the NSE score as SCE-UA minimizes it)
spot_setup = hb.SpotpySetup([socont_sitter, socont_rhone],
                            parameters,
                            [forcing_sitter, forcing_rhone],
                            [obs_sitter, obs_rhone],
                            warmup=365,
                            obj_func='kge_2012',
                            invert_obj_func=True)

# Select number of maximum repetitions and run spotpy
max_rep = 4000
sampler = spotpy.algorithms.sceua(spot_setup, dbname='spotpy_socont_sitter_SCEUA',
                                  dbformat='csv')
sampler.sample(max_rep)

# Load the results
results = spotpy.analyser.load_csv_results('spotpy_socont_sitter_SCEUA')

# Plot evolution
fig_evolution = plt.figure(figsize=(9, 5))
plt.plot(-results['like1'])
plt.ylabel('NSE')
plt.xlabel('Iteration')
plt.tight_layout()
plt.show()

# Get best results
best_index, best_obj_func = spotpy.analyser.get_minlikeindex(results)
best_model_run = results[best_index]
fields = [word for word in best_model_run.dtype.names if word.startswith('sim')]
best_simulation = list(best_model_run[fields])
