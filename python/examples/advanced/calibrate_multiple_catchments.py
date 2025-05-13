import matplotlib.pyplot as plt
import spotpy
from examples._helpers.models_setup_helper import ModelSetupHelper

import hydrobricks as hb

# Set up the model for the Sitter
helper_appenzell = ModelSetupHelper('ch_sitter_appenzell', start_date='1981-01-01',
                                    end_date='2020-12-31')
helper_appenzell.create_hydro_units_from_csv_file()
forcing_appenzell = helper_appenzell.get_forcing_data_from_csv_file(
    ref_elevation=1253, use_precip_gradient=True)
obs_appenzell = helper_appenzell.get_obs_data_from_csv_file()
socont_appenzell, _ = helper_appenzell.get_model_and_params_socont()

# Set up the model for the Rhone at Gletsch
helper_stgallen = ModelSetupHelper('ch_sitter_stgallen', start_date='1981-01-01',
                                   end_date='2020-12-31')
helper_stgallen.create_hydro_units_from_csv_file()
forcing_stgallen = helper_stgallen.get_forcing_data_from_csv_file(
    ref_elevation=1045, use_precip_gradient=True)
obs_stgallen = helper_stgallen.get_obs_data_from_csv_file()
socont_stgallen, parameters = helper_stgallen.get_model_and_params_socont()

# Select the parameters to optimize/analyze
parameters.allow_changing = ['a_snow', 'k_quick', 'A', 'k_slow_1', 'percol', 'k_slow_2',
                             'precip_corr_factor']

# Setup SPOTPY (we need to invert the NSE score as SCE-UA minimizes it)
spot_setup = hb.SpotpySetup([socont_appenzell, socont_stgallen],
                            parameters,
                            [forcing_appenzell, forcing_stgallen],
                            [obs_appenzell, obs_stgallen],
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
