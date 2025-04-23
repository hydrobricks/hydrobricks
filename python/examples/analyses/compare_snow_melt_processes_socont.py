import matplotlib.pyplot as plt
import spotpy
from examples._helpers.models_setup_helper import ModelSetupHelper

import hydrobricks as hb

# Select the methods to compare
methods = ['temperature_index', 'degree_day', 'degree_day_aspect']

# Select number of maximum repetitions for spotpy
max_rep = 4000

# Optimize data-related parameters
optimize_data_params = False

# Select the catchment
catchment_name = 'ch_rhone_gletsch'
ref_elevation = 2702

# Run spotpy for each method
for method in methods:
    # Set up the case study options
    helper = ModelSetupHelper(catchment_name,
                              start_date='1981-01-01',
                              end_date='2020-12-31')

    if method == 'degree_day':
        helper.create_hydro_units_from_csv_file(
            filename='hydro_units_elevation.csv')
    elif method == 'degree_day_aspect':
        helper.create_hydro_units_from_csv_file(
            filename='hydro_units_elevation_aspect.csv',
            other_columns={'slope': 'slope', 'aspect_class': 'aspect_class'})
    elif method == 'temperature_index':
        helper.create_hydro_units_from_csv_file(
            filename='hydro_units_elevation_radiation.csv',
            other_columns={'slope': 'slope', 'radiation': 'radiation'})
    else:
        raise RuntimeError(f"Method {method} not recognized.")

    forcing = helper.get_forcing_data_from_csv_file(
        ref_elevation=ref_elevation, use_precip_gradient=True)
    obs = helper.get_obs_data_from_csv_file()

    if method == 'temperature_index':
        catchment = hb.Catchment(helper.get_catchment_dir() / 'outline.shp')
        if not (helper.working_dir / 'daily_potential_radiation.nc').exists():
            catchment.extract_raster(helper.get_catchment_dir() / 'dem.tif')
            catchment.calculate_daily_potential_radiation(
                str(helper.working_dir),  resolution=25)

        catchment.load_unit_ids_from_raster(
            helper.get_catchment_dir(), 'unit_ids_radiation.tif')
        forcing.spatialize_from_gridded_data(
            variable='solar_radiation', var_name='radiation', path=helper.working_dir,
            file_pattern='daily_potential_radiation.nc',
            data_crs=2056, dim_x='x', dim_y='y',
            raster_hydro_units=helper.get_catchment_dir() / 'unit_ids_radiation.tif')

    # Set up the model
    socont, parameters = helper.get_model_and_params_socont(
        snow_melt_process=f'melt:{method}')

    # Select the parameters to optimize/analyze
    if method == 'degree_day':
        parameters.allow_changing = ['a_snow', 'k_quick', 'A', 'k_slow_1', 'percol',
                                     'k_slow_2']
    elif method == 'degree_day_aspect':
        parameters.allow_changing = ['a_snow_n', 'a_snow_s', 'a_snow_ew', 'k_quick',
                                     'A', 'k_slow_1', 'percol', 'k_slow_2']
    elif method == 'temperature_index':
        parameters.allow_changing = ['melt_factor', 'r_snow', 'k_quick', 'A',
                                     'k_slow_1', 'percol', 'k_slow_2']
    else:
        raise RuntimeError(f"Method {method} not recognized.")

    if optimize_data_params:
        parameters.allow_changing += ['temp_gradients', 'precip_corr_factor',
                                      'precip_gradient']

    # Setup SPOTPY
    spot_setup = hb.SpotpySetup(socont, parameters, forcing, obs, warmup=365,
                                obj_func='kge_2012')

    # Run spotpy
    sampler = spotpy.algorithms.sceua(spot_setup, dbformat='csv',
                                      dbname=f'spotpy_{catchment_name}_{method}')
    sampler.optimization_direction = "maximize"
    sampler.sample(max_rep)

# Plot the results
for method in methods:
    # Load the results
    results = spotpy.analyser.load_csv_results(f'spotpy_{catchment_name}_{method}')

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
