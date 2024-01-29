import matplotlib.pyplot as plt
import spotpy
from examples._helpers.models_setup_helper import ModelSetupHelper

import hydrobricks as hb

# Select the methods to compare
methods = ['degree_day', 'degree_day_aspect']

# Select number of maximum repetitions for spotpy
max_rep = 4000

# Set up the case study options
helper = ModelSetupHelper('ch_sitter_appenzell', start_date='1981-01-01',
                          end_date='2020-12-31')
helper.create_hydro_units_from_csv_file(
    filename='elevation_bands_aspect.csv',
    other_columns={'slope': 'slope', 'aspect_class': 'aspect_class'})
forcing = helper.get_forcing_data_from_csv_file(
    ref_elevation=1250, use_precip_gradient=True)
obs = helper.get_obs_data_from_csv_file()

# Run spotpy for each method
for method in methods:
    # Set up the model
    socont, parameters = helper.get_model_and_params_socont(
        snow_melt_process=f'melt:{method}')

    # Select the parameters to optimize/analyze
    if method == 'degree_day':
        parameters.allow_changing = ['a_snow', 'k_quick', 'A', 'k_slow_1', 'percol',
                                     'k_slow_2', 'precip_corr_factor']
    elif method == 'degree_day_aspect':
        parameters.allow_changing = ['a_snow_n', 'a_snow_s', 'a_snow_ew', 'k_quick',
                                     'A', 'k_slow_1', 'percol', 'k_slow_2',
                                     'precip_corr_factor']
    else:
        raise RuntimeError(f"Method {method} not recognized.")

    # Setup SPOTPY
    spot_setup = hb.SpotpySetup(socont, parameters, forcing, obs, warmup=365,
                                obj_func='kge_2012')

    # Run spotpy
    sampler = spotpy.algorithms.sceua(spot_setup, dbformat='csv',
                                      dbname=f'spotpy_socont_sitter_{method}')
    sampler.optimization_direction = "maximize"
    sampler.sample(max_rep)

# Plot the results
for method in methods:
    # Load the results
    results = spotpy.analyser.load_csv_results(f'spotpy_socont_sitter_{method}')

    # Get best results
    best_index, best_obj_func = spotpy.analyser.get_maxlikeindex(results)
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
