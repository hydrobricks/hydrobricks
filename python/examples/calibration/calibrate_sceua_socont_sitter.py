import matplotlib.pyplot as plt
import spotpy
from examples._helpers.models_setup_helper import ModelSetupHelper

import hydrobricks as hb

# Set up the model
helper = ModelSetupHelper('ch_sitter_appenzell', start_date='1981-01-01',
                          end_date='2020-12-31')
helper.create_hydro_units_from_csv_file()
forcing = helper.get_forcing_data_from_csv_file(ref_elevation=1250)
obs = helper.get_obs_data_from_csv_file()
socont, parameters = helper.get_model_and_params_socont()

# Select the parameters to optimize/analyze
parameters.allow_changing = ['a_snow', 'k_quick', 'A', 'k_slow_1', 'percol', 'k_slow_2',
                             'precip_corr_factor']

# Setup SPOTPY (we need to invert the NSE score as SCE-UA minimizes it)
spot_setup = hb.SpotpySetup(socont, parameters, forcing, obs, warmup=365,
                            obj_func='kge_2012', invert_obj_func=True)

# Select number of maximum repetitions and run spotpy
max_rep = 4000
sampler = spotpy.algorithms.sceua(spot_setup, dbname='socont_sitter_SCEUA',
                                  dbformat='csv')
sampler.sample(max_rep)

# Load the results
results = spotpy.analyser.load_csv_results('socont_sitter_SCEUA')

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

# Plot simulation
fig_simulation = plt.figure(figsize=(16, 9))
ax = plt.subplot(1, 1, 1)
ax.plot(best_simulation, color='black', linestyle='solid',
        label='Best obj. func.=' + str(best_obj_func))
ax.plot(spot_setup.evaluation(), 'r.', markersize=3, label='Observation data')
plt.xlabel('Number of Observation Points')
plt.ylabel('Discharge [l s-1]')
plt.legend(loc='upper right')
plt.tight_layout()
plt.show()

# Cleanup
socont.cleanup()
