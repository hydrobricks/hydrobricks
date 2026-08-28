from datetime import timedelta
from hydrobricks.catchment import Catchment
from hydrobricks import HydroUnits, Forcing
from pathlib import Path

import hydrobricks as hb
import hydrobricks.actions as actions
from hydrobricks.models import Socont
import matplotlib.pyplot as plt
import numpy as np
import pandas as pd
import random
import spotpy

from file_paths_config import FilePathsConfig
from area_config import AreaConfig
from meteo_config import MeteoConfig


print("hb.__file__", hb.__file__)

import _hydrobricks
print(_hydrobricks.__file__)
        
def setting_hydro_units_and_changes(study_area: Catchment, melt_model: str, 
                                    computing_radiation: bool, area_config: AreaConfig,
                                    fp: FilePathsConfig, resolution: int,
                                    with_debris: bool, 
                                    linear_interpolation_of_land_cover_changes: bool,
                                    future: bool):
    """
    @brief Prepares hydro units and glacier change actions for hydrological simulations.

    This function initializes spatial discretization, radiation, elevation bands, glacier evolution, 
    and land cover change handling based on the selected melt model, glacier change strategy, 
    and scenario (historical or future). It outputs configured hydro units and glacier evolution 
    actions that can be used in the simulation engine.

    Depending on the configuration, the function:
      - Computes or loads radiation rasters.
      - Discretizes the study area by elevation and optionally radiation or aspect.
      - Applies different melt models: temperature index, degree-day, or degree-day with aspect.
      - Computes glacier evolution using delta-h methods or land cover change interpolation.
      - Initializes hydro units and optionally loads dynamic land cover changes.

    @param study_area The Catchment object with geometry and DEM.
    @param melt_model The snow/ice melt model: 'temperature_index', 'degree_day', or 'degree_day_aspect'.
    @param computing_radiation Boolean flag to compute (True) or load (False) potential radiation.
    @param area_config AreaConfig object with elevation/radiation discretization settings and glacier shapefiles.
    @param fp FilePathsConfig object with output and intermediate file locations.
    @param resolution Resolution for potential radiation computation (in meters).
    @param with_debris Boolean flag indicating if debris-covered glaciers are used.
    @param linear_interpolation_of_land_cover_changes Boolean flag to use interpolated land cover changes (True) or delta-h method (False).
    @param future Boolean flag to indicate if the simulation targets future projections.

    @return A tuple containing:
        - land_cover_names: List of land cover names (e.g., ['ground', 'glacier', 'glacier_debris']).
        - land_cover_types: Corresponding types for hydro units.
        - changes: A hydrobricks Action object handling glacier changes over time.
        - glacier_infinite_storage: Boolean indicating if glacier has infinite storage (used with land cover changes).
        - snow_ice_transformation: Boolean indicating if snow-to-ice transformation is enabled (used with delta-h).
        - hyd_units: The initialized hydro units ready for simulation.
    """
    
    if future:
        glacier_change = 'delta_h_method'
    elif linear_interpolation_of_land_cover_changes:
        glacier_change = 'linear_interpolation'
    
    match melt_model:
        case 'temperature_index':
            if computing_radiation:
                study_area.calculate_daily_potential_radiation(fp.results, resolution)
            else:
                study_area.load_mean_annual_radiation_raster(fp.results)
            other_columns = None
            study_area.discretize_by(['elevation', 'radiation'],
                                     elevation_method=area_config.elev_method, 
                                     elevation_distance=area_config.elev_distance,
                                     min_elevation=area_config.elev_min_val, max_elevation=area_config.elev_max_val, 
                                     radiation_method=area_config.rad_method, radiation_distance=area_config.rad_distance, 
                                     min_radiation=area_config.rad_min_val, max_radiation=area_config.rad_max_val)
        case 'degree_day_aspect':
            other_columns = {'slope': 'slope', 'aspect_class': 'aspect_class'}
            study_area.discretize_by(['elevation', 'aspect'], 
                                     elevation_method=area_config.elev_method, 
                                     elevation_distance=area_config.elev_distance,
                                     min_elevation=area_config.elev_min_val, max_elevation=area_config.elev_max_val, 
                                     )
        case 'degree_day':
            other_columns = None
            study_area.create_elevation_bands(method=area_config.elev_method, 
                                              min_elevation=area_config.elev_min_val, 
                                              max_elevation=area_config.elev_max_val, 
                                              distance=area_config.elev_distance)
        case _:
            raise ValueError(f"Invalid melt model option {melt_model}.")
    
    # Save the hydro units properties to a csv file
    study_area.save_hydro_units_to_csv(fp.hydro_units)
    
    # Compute the glacier changes through time.
    match glacier_change:
        case 'delta_h_method':
            # Glacier evolution
            glacier_evolution = hb.preprocessing.GlacierEvolutionDeltaH()
            glacier_df = glacier_evolution.compute_initial_ice_thickness(
                study_area, ice_thickness=area_config.glacier_thickness,
                elevation_bands_distance=area_config.elev_distance / 10)
                #study_area, area_config.glacier_shp, ice_thickness=area_config.glacier_thickness)
            
            # It can then optionally be saved as a csv file
            glacier_df.to_csv(fp.results / "glacier_profile.csv", index=False)
            
            # The lookup table can be computed and saved as a csv file
            glacier_evolution.compute_lookup_table()
            glacier_evolution.save_as_csv(fp.results)
    
            # Create the action glacier evolution object
            changes = actions.ActionGlacierEvolutionDeltaH()
            changes.load_from(glacier_evolution, land_cover='glacier',
                              update_month='October')
            
            glacier_infinite_storage = False
            snow_ice_transformation = True
        case 'linear_interpolation':
            # FORMAT FOR THE CHANGING INITIALISATION
            # The dataframe can be saved as csv (without column names)
            if with_debris:
                # Create the action land cover change object and the corresponding dataframe
                changes, changes_df = hb.actions.ActionLandCoverChange.create_action_for_glaciers(
                    study_area, area_config.times, area_config.clean_glaciers, area_config.debris_glaciers, 
                    with_debris=with_debris, method='raster', interpolate_yearly=True)
                
                changes_df[0].to_csv(fp.changes_glacier_ice, index=False)
                changes_df[1].to_csv(fp.changes_glacier_debris, index=False)
                changes_df[2].to_csv(fp.changes_ground, index=False)
            else:
                # Create the action land cover change object and the corresponding dataframe
                changes, changes_df = hb.actions.ActionLandCoverChange.create_action_for_glaciers(
                    study_area, area_config.times, area_config.whole_glaciers, area_config.debris_glaciers, 
                    with_debris=with_debris, method='raster', interpolate_yearly=True)
                
                changes_df[0].to_csv(fp.changes_glacier, index=False)
                changes_df[1].to_csv(fp.changes_ground, index=False)
            
            glacier_infinite_storage = True
            snow_ice_transformation = False
        case 'loading_change':
            glacier_infinite_storage = True
            snow_ice_transformation = False
        case _:
            raise ValueError(f"Invalid glacier change option {glacier_change}.")
    
    if with_debris:
        land_cover_names = ['ground', 'glacier_ice', 'glacier_debris']
        land_cover_types = ['ground', 'glacier', 'glacier']
    else:
        land_cover_names = ['ground', 'glacier']
        land_cover_types = ['ground', 'glacier']
    
    # Hydro units
    hyd_units = HydroUnits(land_cover_types, land_cover_names)
    hyd_units.load_from_csv(fp.hydro_units, column_area='area', column_elevation='elevation',
                            other_columns=other_columns)
    
    # Finally, initialize the HydroUnits cover with the first cover values of the
    # ActionLandCoverChange object (no need to do it for the ground land cover type).
    match glacier_change:
        case 'delta_h_method':
            pass
        case 'linear_interpolation':
            if with_debris:
                hyd_units.initialize_from_land_cover_change('glacier_ice', changes_df[0])
                hyd_units.initialize_from_land_cover_change('glacier_debris', changes_df[1])
            else:
                hyd_units.initialize_from_land_cover_change('glacier', changes_df[0])
        case 'loading_change':
            if with_debris:
                hyd_units.initialize_from_land_cover_change('glacier_ice', changes_df[0])
                hyd_units.initialize_from_land_cover_change('glacier_debris', changes_df[1])
            else:
                hyd_units.initialize_from_land_cover_change('glacier', changes_df[0])
                
            # And can be loaded again to be used in hydrobricks later
            changes = hb.actions.ActionLandCoverChange()
            changes.load_from_csv(
                fp.changes_ground.csv, hydro_units=study_area.hydro_units,
                land_cover='ground', area_unit='m2', match_with='id')
            if with_debris:
                changes.load_from_csv(
                    fp.changes_glacier_ice.csv, hydro_units=study_area.hydro_units,
                    land_cover='glacier_ice', area_unit='m2', match_with='id')
                changes.load_from_csv(
                    fp.changes_glacier_debris.csv, hydro_units=study_area.hydro_units,
                    land_cover='glacier_debris', area_unit='m2', match_with='id')
            else:
                changes.load_from_csv(
                    fp.changes_glacier.csv, hydro_units=study_area.hydro_units,
                    land_cover='glacier', area_unit='m2', match_with='id')
        case _:
            raise ValueError(f"Invalid glacier change option {glacier_change}.")
    
    # Save elevation bands to a raster
    study_area.save_unit_ids_raster(fp.results)

    return land_cover_names, land_cover_types, changes, glacier_infinite_storage, snow_ice_transformation, hyd_units

def setting_forcing(compute_altitudinal_trends: bool, hyd_units: HydroUnits, 
                    meteo: MeteoConfig, latitude: float, fp: FilePathsConfig,
                    melt_model: str, dem_crs: str):
    """
    @brief Prepares and loads the meteorological forcing data for hydrological simulations.

    This function either computes spatialized meteorological forcing fields using gridded input data 
    or loads precomputed forcing from file. It supports temperature, precipitation, PET, and optionally
    solar radiation fields depending on the selected melt model.

    When `compute_altitudinal_trends` is True, the function:
      - Spatializes precipitation and temperature data across hydro units.
      - Computes potential evapotranspiration (PET) using the Hamon method.
      - Optionally spatializes solar radiation data (if melt model is 'temperature_index').
      - Saves the result to a NetCDF forcing file.

    Regardless of compute flag, the function always reloads the saved forcing file to ensure compatibility 
    with HydroBricks simulation flow.

    @param compute_altitudinal_trends Boolean flag to indicate if forcing should be computed from gridded inputs.
    @param hyd_units HydroUnits object representing discretized simulation units.
    @param meteo MeteoConfig object containing meteorological variable metadata and file structure.
    @param latitude Latitude of the study area (required for PET computation).
    @param fp FilePathsConfig object with output and intermediate file paths.
    @param melt_model String identifier of the melt model (e.g., 'temperature_index').
    @param dem_crs Coordinate reference system of the DEM, used for spatializing radiation.

    @return forcing A Forcing object fully loaded and ready for use in the hydrological model.
    """
    
    # Meteo data - computing a meteo file
    if compute_altitudinal_trends:
        forcing = Forcing(hyd_units)
        forcing.spatialize_from_gridded_data(
            variable='precipitation', path=meteo.pr_path, file_pattern=meteo.pr_file_pattern,
            data_crs=meteo.nc_crs, var_name=meteo.pr_varname, dim_x=meteo.dim_x,
            dim_y=meteo.dim_y, dim_time=meteo.dim_time, raster_hydro_units=fp.unit_ids)
        forcing.spatialize_from_gridded_data(
            variable='temperature', path=meteo.tas_path, file_pattern=meteo.tas_file_pattern,
            data_crs=meteo.nc_crs, var_name=meteo.tas_varname, dim_x=meteo.dim_x, 
            dim_y=meteo.dim_y, dim_time=meteo.dim_time, raster_hydro_units=fp.unit_ids)
        forcing.compute_pet(method='Hamon', use=['t', 'lat'], lat=latitude)
            
        
        if melt_model == 'temperature_index':
            forcing.spatialize_from_gridded_data(
                variable='solar_radiation', path=fp.results, file_pattern=meteo.rad_file_pattern,
                data_crs=dem_crs, var_name=meteo.rad_varname, dim_x=meteo.rad_dim_x,
                dim_y=meteo.rad_dim_y, dim_time=meteo.rad_dim_time, 
                raster_hydro_units=fp.unit_ids)
        
        # Save forcing to a netcdf file
        forcing.save_as(fp.forcing_file)
        
    # Meteo data - loading the result file
    # Weirdly, I cannot directly use the forcing that was just computed to launch the simulations.
    # It only works if I save and reload it.
    forcing = Forcing(hyd_units)
    forcing.load_from(fp.forcing_file)
    return forcing

def setting_model(area_config: AreaConfig, meteo_config: MeteoConfig, 
                  file_paths: FilePathsConfig, melt_model: str, 
                  computing_radiation: bool, resolution: int, with_debris: bool, 
                  compute_action_land_cover_changes: bool, soil_storage_nb: int, 
                  modeling: str, params_to_calibrate: list[str], 
                  set_params: dict, compute_altitudinal_trends: bool, 
                  fp: FilePathsConfig, future: bool):
    """
    @brief Sets up the complete hydrological model, including catchment, discretization, forcing, and structure.

    This function integrates spatial discretization, glacier configuration, meteorological forcing,
    and model structure definition using the HydroBricks Socont class. It supports both calibration
    and simulation modes and handles changes in glacier cover and land use.

    Steps performed:
      - Loads the catchment from the area outline and extracts the DEM.
      - Configures glacier hydro units and land cover changes using selected melt model and glacier change mode.
      - Builds meteorological forcing from gridded datasets or loads existing files.
      - Creates a Socont model with user-defined melt process, land cover structure, and actions.
      - Prepares parameters for calibration or applies predefined values.
      - Finalizes the model setup with spatial structure and simulation time bounds.

    @param area_config AreaConfig object with DEM path, outline, discretization, and glacier config.
    @param meteo_config MeteoConfig object with meteorological file paths and metadata.
    @param file_paths FilePathsConfig object with paths to outputs and intermediate files.
    @param melt_model The melt model used: 'temperature_index', 'degree_day', or 'degree_day_aspect'.
    @param computing_radiation Boolean flag for whether to compute or load radiation raster.
    @param resolution Spatial resolution for radiation computation.
    @param with_debris Boolean flag for whether to include debris-covered glaciers.
    @param compute_action_land_cover_changes Boolean flag to use raster-based land cover interpolation.
    @param soil_storage_nb Number of soil storages in the model.
    @param modeling Mode of simulation: 'calibration', 'forcing', 'median_forcing', or 'top_forcing'.
    @param params_to_calibrate List of parameter names to calibrate (only used if modeling == 'calibration').
    @param set_params Dictionary of fixed parameter values (used for 'forcing' or median/top runs).
    @param compute_altitudinal_trends Whether to compute altitudinal meteorological trends from gridded data.
    @param fp Duplicate of file_paths for legacy compatibility.
    @param future Boolean flag indicating if simulation is for a future scenario.

    @return Tuple containing:
        - land_cover_names: List of land cover names used in the model.
        - land_cover_types: Corresponding types for hydrological simulation.
        - changes: HydroBricks Action object handling glacier evolution or land cover dynamics.
        - hyd_units: HydroUnits object defining spatial discretization.
        - forcing: Prepared meteorological Forcing object.
        - socont: The fully constructed Socont model object.
        - parameters: ModelParameters object (calibrated or fixed).
    """
    
    study_area = Catchment(outline=area_config.outline)
    success = study_area.extract_dem(area_config.dem_path)
    print("Extracted the DEM:", success)
    
    land_cover_names, \
    land_cover_types, \
    changes, \
    glacier_infinite_storage, \
    snow_ice_transformation, \
    hyd_units = setting_hydro_units_and_changes(study_area, melt_model, computing_radiation, 
                                                area_config, file_paths, resolution,
                                                with_debris, compute_action_land_cover_changes, 
                                                future)
    
    forcing = setting_forcing(compute_altitudinal_trends, hyd_units, meteo_config, area_config.latitude, 
                              file_paths, melt_model, area_config.outline_epsg)
    
    # Model structure
    socont = Socont(soil_storage_nb=soil_storage_nb, 
                    surface_runoff="linear_storage",
                    snow_melt_process=f"melt:{melt_model}",
                    land_cover_names=land_cover_names,
                    land_cover_types=land_cover_types,
                    glacier_infinite_storage=glacier_infinite_storage,
                    snow_ice_transformation=snow_ice_transformation,
                    record_all=True)

    # The 'changes' object can be directly used in hydrobricks:
    socont.add_action(changes)

    # Parameters
    parameters = socont.generate_parameters()
    if modeling == 'calibration':
        parameters.allow_changing = params_to_calibrate
    elif modeling == 'forcing' or modeling == 'median_forcing' or modeling == 'top_forcing':
        parameters.set_values(set_params)
        
    # Model setup
    socont.setup(spatial_structure=hyd_units, output_path=str(file_paths.results),
                 start_date=area_config.start_date, end_date=area_config.end_date)
    
    return land_cover_names, \
    land_cover_types, \
    changes, \
    hyd_units, forcing, socont, parameters

def determine_params_from_model_config(melt_model: str, with_debris: bool,
                                       soil_storage_nb: int):
    """
    @brief Determines the list of hydrological model parameters to calibrate.

    This function selects the appropriate set of calibration parameters based on the chosen
    melt model type, whether the glacier is debris-covered, and the number of soil storages.
    It returns a list of parameter names that will be passed to the calibration engine.

    Melt model-dependent parameters:
      - 'temperature_index': Includes melt factor (`mf`), and radiation factors (`r_snow`, `r_ice`, etc.).
      - 'degree_day': Includes degree-day factors (`a_snow`, `a_ice`, etc.).
      - 'degree_day_aspect': Includes separate snow/ice factors for aspect classes (north, south, east-west).

    Additional parameters:
      - `soil_storage_nb == 2` adds parameters for the second slow reservoir (`k_slow_2`, `percol`).
      - All models include core hydrological parameters: `'A'`, `'k_slow_1'`, `'k_quick'`, `'k_snow'`, `'k_ice'`.

    @param melt_model The snow/ice melt model: 'temperature_index', 'degree_day', or 'degree_day_aspect'.
    @param with_debris Boolean flag indicating if debris-covered glacier processes are enabled.
    @param soil_storage_nb Number of soil storages in the model (typically 1 or 2).

    @return List of parameter names (`List[str]`) that should be calibrated.
    """
    
    params_to_calibrate = ['A', 'k_slow_1', 'k_quick', 'k_snow', 'k_ice']
    if melt_model == 'temperature_index' and with_debris:
        params_to_calibrate.extend(['mf', 'r_snow', 'r_ice_0', 'r_ice_1'])
    elif melt_model == 'degree_day' and with_debris:
        params_to_calibrate.extend(['a_snow', 'a_ice_0', 'a_ice_1'])
    elif melt_model == 'degree_day_aspect' and with_debris:
        params_to_calibrate.extend(['a_snow_n', 'a_ice_n_0', 'a_ice_n_1', 
                                'a_snow_s', 'a_ice_s_0', 'a_ice_s_1', 
                                'a_snow_ew', 'a_ice_ew_0', 'a_ice_ew_1'])
    elif melt_model == 'temperature_index' and not with_debris:
        params_to_calibrate.extend(['mf', 'r_snow', 'r_ice'])
    elif melt_model == 'degree_day' and not with_debris:
        params_to_calibrate.extend(['a_snow', 'a_ice'])
    elif melt_model == 'degree_day_aspect' and not with_debris:
        params_to_calibrate.extend(['a_snow_n', 'a_ice_n',
                                'a_snow_s', 'a_ice_s',
                                'a_snow_ew', 'a_ice_ew'])
    if soil_storage_nb == 2:
        params_to_calibrate.extend(['k_slow_2', 'percol'])
    
    return params_to_calibrate

def determine_model_config_from_params(input_forcing_file: str, 
                                       forcing_type: str = "forcing", 
                                       catchment: str | None = None, 
                                       obj_func: str | None = None):
    """
    @brief Infers model configuration from a given parameter file.

    This function reads a forcing or statistical parameter CSV file and infers:
      - the melt model used (`temperature_index`, `degree_day`, or `degree_day_aspect`)
      - whether the glacier model includes debris-covered ice
      - the number of soil storages
      - the parameter values as a dictionary

    Supported forcing types:
      - `"forcing"`: reads a basic key-value parameter CSV (excluding header)
      - `"median_forcing"` or `"top_forcing"`: reads a DataFrame with multiple simulations,
         filters by catchment and objective function, and selects the first row

    @param input_forcing_file Path to the parameter CSV file.
    @param forcing_type Type of file content: "forcing", "median_forcing", or "top_forcing".
    @param catchment Optional catchment name (required for median/top forcing files).
    @param obj_func Optional objective function used for calibration (e.g., "nse", "kge_2012").

    @return Tuple:
        - melt_model: One of {'temperature_index', 'degree_day', 'degree_day_aspect'}.
        - with_debris: True if debris-covered glacier terms are present, else False.
        - soil_storage_nb: Number of soil storage units (1 or 2).
        - set_params: Dict[str, float] of parameter names and their calibrated values.
    """
    
    if forcing_type == "forcing":
        set_params = {}
        with open(input_forcing_file, 'r') as file:
            lines = file.readlines()[1:]  # Skip the first line
            for line in lines:
                key, value = line.strip().split(', ')
                set_params[key] = float(value)
    elif forcing_type == "median_forcing" or forcing_type == "top_forcing":
        df = pd.read_csv(input_forcing_file)
        if obj_func == 'nse':
            perfor_cr = "NSE"
        if obj_func == 'kge_2012':
            perfor_cr = "KGE"
        list_dict = df[(df["catchment"] == catchment) & (df["perfor_cr"] == perfor_cr)].to_dict('list')
        set_params = {key.removeprefix('par'):value[0] for (key, value) in list_dict.items()} # dictionary comprehension
        for key in ['Unnamed: 0', 'perfor_cr', 'catchment', 'level_1', 'simu_nb', 'like1']:
            set_params.pop(key, None)
    print("Parameter set", set_params)
    
    soil_storage_nb = 1
    if all(key in set_params for key in ['k_slow_2', 'percol']):
        soil_storage_nb = 2
    if all(key in set_params for key in ['mf', 'r_snow', 'r_ice_0', 'r_ice_1']):
        melt_model = 'temperature_index'
        with_debris = True
    elif all(key in set_params for key in ['a_snow', 'a_ice_0', 'a_ice_1']):
        melt_model = 'degree_day'
        with_debris = True
    elif all(key in set_params for key in ['a_snow_n', 'a_ice_n_0', 'a_ice_n_1', 
                                           'a_snow_s', 'a_ice_s_0', 'a_ice_s_1', 
                                           'a_snow_ew', 'a_ice_ew_0', 'a_ice_ew_1']):
        melt_model = 'degree_day_aspect'
        with_debris = True
    elif all(key in set_params for key in ['mf', 'r_snow', 'r_ice']):
        melt_model = 'temperature_index'
        with_debris = False
    elif all(key in set_params for key in ['a_snow', 'a_ice']):
        melt_model = 'degree_day'
        with_debris = False
    elif all(key in set_params for key in ['a_snow_n', 'a_ice_n',
                                           'a_snow_s', 'a_ice_s',
                                           'a_snow_ew', 'a_ice_ew']):
        melt_model = 'degree_day_aspect'
        with_debris = False
    else:
        raise ValueError(f"The parameter set is not consistent.")

    return melt_model, with_debris, soil_storage_nb, set_params

def forcing_run(socont: Socont, parameters, forcing: Forcing, start_date: str, end_date: str, fp: FilePathsConfig, 
                forcing_stats_and_output: str, warmup: int, set_params: dict, 
                snow_melt_process: str, with_debris: bool, metrics=True):
    """
    @brief Executes a hydrological simulation using a given parameter set and writes the results.

    This function initializes, runs, and evaluates a Socont model forced with a provided 
    Forcing object. The resulting discharge is saved as a time series along with evaluation metrics 
    (NSE, KGE) and the calibrated parameter set. Optionally compares with observed discharge.

    Operations:
      - Runs the Socont model from start_date to end_date.
      - Extracts simulated discharge at the outlet.
      - Optionally loads observed discharge and computes NSE and KGE_2012.
      - Writes results, metrics, and parameters to a CSV output file.

    @param socont The initialized Socont model object with land cover and processes configured.
    @param parameters ModelParameters object holding values to run the model.
    @param forcing Forcing object containing meteorological time series data.
    @param start_date Start date of the simulation period (format: 'YYYY-MM-DD').
    @param end_date End date of the simulation period (format: 'YYYY-MM-DD').
    @param fp FilePathsConfig object with required I/O paths (discharge file, time formatting, etc.).
    @param forcing_stats_and_output Path to the output file where simulation results and stats are saved.
    @param warmup Number of warm-up days to exclude from evaluation metrics.
    @param set_params Dictionary of model parameters used (to be logged).
    @param snow_melt_process Identifier of the snow/ice melt model (for traceability).
    @param with_debris Boolean indicating whether the glacier simulation includes debris cover.
    @param metrics Boolean flag (default: True) to compute NSE and KGE against observed data.

    @return None. Results are written directly to the specified output CSV.
    """
    
    # Initialize and run the model
    socont.initialize_state_variables(parameters=parameters, forcing=forcing)
    socont.run(parameters=parameters, forcing=forcing)
    
    # Get outlet discharge time series.
    sim_ts = socont.get_outlet_discharge()
    
    start_datetime = pd.to_datetime(start_date)
    end_datetime = pd.to_datetime(end_date)
    times = pd.date_range(start_datetime, end_datetime, freq='D')
    
    df = times.to_frame(index=True, name='Date')
    df['Simulated'] = sim_ts
    
    if metrics:
        # Get observed discharge time series.
        obs = hb.Observations()
        obs.load_from_csv(fp.discharge_file, column_time=fp.column_time, time_format=fp.time_format,
                          content=fp.content, start_date=start_date, end_date=end_date)
        # Evaluate
        obs_ts = obs.data[0]
        nse = socont.eval('nse', obs_ts, warmup=warmup)
        kge_2012 = socont.eval('kge_2012', obs_ts, warmup=warmup)
    else:
        nse = np.nan
        kge_2012 = np.nan
    
    with open(forcing_stats_and_output, "w") as f:
        f.write(f'nse, {nse:.8f}\n')
        f.write(f'kge_2012, {kge_2012:.8f}\n')
        for key, value in set_params.items():
            f.write('%s, %f\n' % (key, value))
        f.write(f'snow_melt_process, {snow_melt_process}\n')
        f.write(f'with_debris, {with_debris}\n')
        print(f"nse = {nse:.8f}, kge_2012 = {kge_2012:.8f}")
        df.to_csv(f, mode='a', index=False)
        #np.savetxt(f, sim_ts, header='Simulated outlet discharge time series')
    
    print('End of forcing run')
    

def main_script(catchment: str, area: str, meteo_data: str, obj_func: str = 'nse', 
                melt_model: str = 'degree_day', modeling: str = 'calibration', 
                subdirectory: str = '', computing_radiation: bool = False, 
                metrics: bool = True, multi_calibration: str | None = None,
                include_warmup: bool = False, algorithm: str = 'SCEUA',
                with_debris: bool = False, resolution: int | None = None, 
                compute_altitudinal_trends: bool = True,
                compute_action_land_cover_changes: bool = True,
                soil_storage_nb: int = 2, warmup: int = 365, max_rep: int = 10000):
    """
    @brief Main entry point for executing hydrological model workflows: calibration, forcing, or bootstrapping.

    This function manages the full pipeline for a hydrological simulation, including configuration loading, 
    model setup, parameter handling, execution of calibration or forcing, and result output (figures, stats, CSVs).

    Supported modes (`modeling`):
      - `"calibration"`: Performs calibration using a SPOTPY sampler (SCEUA, MC, or ROPE).
      - `"forcing"` / `"median_forcing"` / `"top_forcing"`: Runs model with pre-calibrated parameters.
      - `"bootstrapping"`: Computes bootstrap-based reference metrics from observed discharge data.

    Core responsibilities:
      - Resolves glacier and meteorological configurations.
      - Initializes model and forcing using elevation/radiation bands and melt model.
      - Runs model and stores time series and performance metrics (NSE, KGE).
      - Handles multi-catchment calibration if `multi_calibration` is provided.
      - Exports all results, diagnostics, and hydrograph plots.

    @param catchment Name of the sub-basin or outlet used for discharge comparison.
    @param area The larger study area name (e.g., "Arolla", "Solda").
    @param meteo_data Source of meteorological input: "MeteoSwiss", "CH2018_RCP26", etc.
    @param obj_func Objective function for calibration performance: "nse", "kge_2012", "mse", "rmse".
    @attention Algorithms minimize the objective function. If you add new metrics, ensure they are 
    inverted in this function if higher values mean better performance (e.g., "mse", "rmse").
    @param melt_model Melt model to use: "temperature_index", "degree_day", "degree_day_aspect".
    @param modeling Simulation mode: "calibration", "forcing", "median_forcing", "top_forcing", or "bootstrapping".
    @param subdirectory Optional custom path suffix for organizing output folders.
    @param computing_radiation If True, computes potential radiation; otherwise loads from disk.
    @param metrics If True, evaluates simulation with performance metrics (ignored for future projections).
    @param multi_calibration Optional second catchment for dual calibration.
    @param include_warmup If True, includes warmup period when computing bootstrap metrics.
    @param algorithm Calibration algorithm to use: "SCEUA", "MC", or "ROPE".
    @param with_debris Boolean indicating whether to model debris-covered glaciers separately.
    @param resolution Radiation resolution (in meters) if computing radiation. None to use the DEM resolution.
    @param compute_altitudinal_trends If True, generates meteorological forcing per elevation band.
    @param compute_action_land_cover_changes Whether to use raster-based glacier area change interpolation.
    @param soil_storage_nb Number of soil reservoirs in the model (1 or 2).
    @param warmup Warmup period (days) before performance evaluation begins.
    @param max_rep Maximum number of iterations (e.g., calibration repetitions).

    @return None. Results are saved directly to disk (CSV, PNG, SVG).
    """
    
    print(f'Now processing catchment {catchment} of area {area} in {modeling} with {melt_model} using {obj_func} with {meteo_data}.')
    if multi_calibration:
        print(f"With {multi_calibration} as second calibration catchment.")

    ####### CONFIGURATION ##########################################
    if with_debris:
        subdirectory += "with_debris/"
    subdirectory += meteo_data + "/"
    
    if meteo_data == "MeteoSwiss" or meteo_data == "Crespi":
        future = False
    elif meteo_data == "CH2018_RCP26" or meteo_data == "CH2018_RCP45" or meteo_data == "CH2018_RCP85":
        future = True
        metrics = False
    else:
        raise ValueError(f"Invalid meteorological dataset option {meteo_data}.")
    
    path = Path.home() / 'Documents' / 'Datasets'
    
    fp = FilePathsConfig(path, area, catchment, algorithm, melt_model, obj_func, multi_calibration, with_debris, subdirectory)
    area_config = AreaConfig(area, fp.path, catchment, future, with_debris)
    meteo_config = MeteoConfig(meteo_data, area, fp.path)
    
    if multi_calibration:
        assert modeling == 'calibration'
        fp2 = FilePathsConfig(area, multi_calibration, algorithm, melt_model, obj_func, multi_calibration, with_debris, subdirectory)
        area_config2 = AreaConfig(area, fp.path, multi_calibration)
    
    ####### PARAMETER SETTING ######################################
    set_params = None
    params_to_calibrate = None
    if modeling == 'calibration':
        params_to_calibrate = determine_params_from_model_config(melt_model, with_debris, soil_storage_nb)
        set_params = None
        if algorithm in ['SCEUA', 'NSGAII', 'PADDS']:
            invert_obj_func = True
            minimize = True
        else:
            invert_obj_func = False
            minimize = False
        if obj_func in ['mse', 'rmse']:
            invert_obj_func = not invert_obj_func
            minimize = not invert_obj_func
    elif modeling == 'forcing':
        print(fp.best_forcing_input_file, modeling)
        melt_model, with_debris, soil_storage_nb, set_params = determine_model_config_from_params(fp.best_forcing_input_file, modeling)
        forcing_stats_and_output = fp.best_forcing_stats_and_output
    elif modeling == 'median_forcing':
        melt_model, with_debris, soil_storage_nb, set_params = determine_model_config_from_params(fp.median_forcing_input_file, modeling, "BI", obj_func)
        forcing_stats_and_output = fp.median_forcing_stats_and_output
    elif modeling == 'top_forcing':
        melt_model, with_debris, soil_storage_nb, set_params = determine_model_config_from_params(fp.top_forcing_input_file, modeling, "BI", obj_func)
        forcing_stats_and_output = fp.top_forcing_stats_and_output
    # If a parameter is forgotten -> Get the error message: "A type error exception occurred."
    
    elif modeling == 'bootstrapping':
        if include_warmup:
            real_start_date = pd.to_datetime(area_config.start_date)
        else:
            real_start_date = pd.to_datetime(area_config.start_date) + timedelta(days=warmup)
        # Obs data
        obs = hb.Observations()
        obs.load_from_csv(fp.discharge_file, column_time=fp.column_time, time_format=fp.time_format,
                          content=fp.content, start_date=real_start_date, end_date=area_config.end_date)
        range = real_start_date.strftime("%Y-%m-%d") + "_" + area_config.end_date
        
        r_ref_nse = obs.compute_reference_metric('nse', all_combinations=True)
        r_ref_kge_2012 = obs.compute_reference_metric('kge_2012', all_combinations=True)
        r_ref_nse_excl = obs.compute_reference_metric('nse', with_exclusion=True, all_combinations=True)
        r_ref_kge_2012_excl = obs.compute_reference_metric('kge_2012', with_exclusion=True, all_combinations=True)
        r_ref_nse_mean = obs.compute_reference_metric('nse', mean_discharge=True)
        r_ref_kge_2012_mean = obs.compute_reference_metric('kge_2012', mean_discharge=True)
    
        obs_all_range = hb.Observations()
        obs_all_range.load_from_csv(fp.discharge_file, column_time=fp.column_time, time_format=fp.time_format,
                                    content=fp.content, start_date=None, end_date=None)
        
        # Take the same number of evaluations as the all_combinations in the 2010-2014 range (valid for Arolla).
        a_ref_nse = obs_all_range.compute_reference_metric('nse', start_date=real_start_date, 
                                                           end_date=area_config.end_date, n_evals=3125)
        a_ref_kge_2012 = obs_all_range.compute_reference_metric('kge_2012', start_date=real_start_date, 
                                                           end_date=area_config.end_date, n_evals=3125)
        a_ref_nse_excl = obs_all_range.compute_reference_metric('nse', start_date=real_start_date, 
                                                           end_date=area_config.end_date, with_exclusion=True,
                                                           n_evals=3125)
        a_ref_kge_2012_excl = obs_all_range.compute_reference_metric('kge_2012', start_date=real_start_date, 
                                                           end_date=area_config.end_date, with_exclusion=True,
                                                           n_evals=3125)
        a_ref_nse_mean = obs_all_range.compute_reference_metric('nse', start_date=real_start_date, 
                                                           end_date=area_config.end_date, mean_discharge=True)
        a_ref_kge_2012_mean = obs_all_range.compute_reference_metric('kge_2012', start_date=real_start_date, 
                                                           end_date=area_config.end_date, mean_discharge=True)
    
        with open(fp.bootstrapping_stats, 'w') as f:
            f.write(f'ref_nse_{range}_normal_block_bootstrap, {r_ref_nse:.3f}\n')
            f.write(f'ref_nse_{range}_excluding_block_bootstrap, {r_ref_nse_excl:.3f}\n')
            f.write(f'ref_nse_{range}_normal_average_discharge, {r_ref_nse_mean:.3f}\n')
            f.write(f'ref_nse_all_range_normal_block_bootstrap, {a_ref_nse:.3f}\n')
            f.write(f'ref_nse_all_range_excluding_block_bootstrap, {a_ref_nse_excl:.3f}\n')
            f.write(f'ref_nse_all_range_normal_average_discharge, {a_ref_nse_mean:.3f}\n')
            f.write(f'ref_kge_2012_{range}_normal_block_bootstrap, {r_ref_kge_2012:.3f}\n')
            f.write(f'ref_kge_2012_{range}_excluding_block_bootstrap, {r_ref_kge_2012_excl:.3f}\n')
            f.write(f'ref_kge_2012_{range}_normal_average_discharge, {r_ref_kge_2012_mean:.3f}\n')
            f.write(f'ref_kge_2012_all_range_normal_block_bootstrap, {a_ref_kge_2012:.3f}\n')
            f.write(f'ref_kge_2012_all_range_excluding_block_bootstrap, {a_ref_kge_2012_excl:.3f}\n')
            f.write(f'ref_kge_2012_all_range_normal_average_discharge, {a_ref_kge_2012_mean:.3f}')
        
        print('End of bootstrapping run')
        return
    
    #######################################################################
    land_cover_names, \
    land_cover_types, \
    changes, \
    hyd_units, forcing, socont, parameters = setting_model(area_config, meteo_config, fp, melt_model, computing_radiation, resolution, with_debris, 
                                    compute_action_land_cover_changes, soil_storage_nb, modeling, params_to_calibrate, 
                                    set_params, compute_altitudinal_trends, fp, future)

    if multi_calibration:
        land_cover_names2, \
        land_cover_types2, \
        changes2, \
        hyd_units2, forcing2, socont2, parameters2 = setting_model(area_config2, meteo_config, fp2, melt_model, computing_radiation, resolution, with_debris, 
                                          compute_action_land_cover_changes, soil_storage_nb, modeling, params_to_calibrate, 
                                          set_params, compute_altitudinal_trends, fp, future)
        ##### ENSURE THE DATES ARE THE SAME FOR MULTICALIBRATION + SAME WITH PARAMETERS
    
    if modeling == 'calibration':
        obs = hb.Observations()
        obs.load_from_csv(fp.discharge_file, column_time=fp.column_time, time_format=fp.time_format,
                          content=fp.content, start_date=area_config.start_date, end_date=area_config.end_date)
        if multi_calibration:
            obs2 = hb.Observations()
            obs2.load_from_csv(fp2.discharge_file, column_time=fp2.column_time, time_format=fp2.time_format,
                              content=fp2.content, start_date=area_config2.start_date, end_date=area_config2.end_date)
            catchments = [socont, socont2]
            forcings = [forcing, forcing2]
            obss = [obs, obs2]
        else:
            catchments = socont
            forcings = forcing
            obss = obs
    
        if obj_func == 'mse' or obj_func == 'kge_2012':
            spot_setup = hb.SpotpySetup(catchments, parameters, forcings, obss, warmup=warmup, 
                                        obj_func=obj_func, 
                                        invert_obj_func=invert_obj_func)
        elif obj_func == 'nse':
            spot_setup = hb.SpotpySetup(catchments, parameters, forcings, obss, warmup=warmup, 
                                        obj_func=spotpy.objectivefunctions.nashsutcliffe, 
                                        invert_obj_func=invert_obj_func)
        else:
            raise ValueError(f'Error: Objective function {obj_func} not yet registered.')

        # Run spotpy with a set random state
        seed = 8
        random.seed(seed)
        if algorithm == 'SCEUA':
            sampler = spotpy.algorithms.sceua(spot_setup, dbname=fp.calibration_all, dbformat='csv', save_sim=True, random_state=seed)
        elif algorithm == 'MC':
            sampler = spotpy.algorithms.mc(spot_setup, dbname=fp.calibration_all, dbformat='csv', save_sim=True, random_state=seed )
        elif algorithm == 'ROPE':
            sampler = spotpy.algorithms.rope(spot_setup, dbname=fp.calibration_all, dbformat='csv', save_sim=True, random_state=seed) #, parallel='mpi' #ModuleNotFoundError: No module named 'mpi4py'
        else:
            raise ValueError(f'Error: SPOTPY algorithm {algorithm} not yet registered.')
        sampler.sample(max_rep)
        
        # Load the results
        result = sampler.getdata()
        
        # Plot parameter interaction
        spotpy.analyser.plot_parameterInteraction(result)
        plt.savefig(fp.param_interac + '.svg', format="svg", bbox_inches='tight', dpi=30)
        plt.savefig(fp.param_interac + '.png', format="png", bbox_inches='tight', dpi=100)
        
        #######################################################################
    
        # Load the results
        result = spotpy.analyser.load_csv_results(fp.calibration_all)
        
        # Plot evolution
        fig_evolution = plt.figure(figsize=(9, 5))
        if invert_obj_func:
            plt.plot(-result['like1'])
        else:
            plt.plot(result['like1'])
        plt.ylabel(obj_func)
        plt.xlabel('Iteration')
        plt.savefig(fp.comparison + '.svg', format="svg", bbox_inches='tight', dpi=30)
        plt.savefig(fp.comparison + '.png', format="png", bbox_inches='tight', dpi=100)
        
        # Get best results
        # Each row is a simulation result. The first columns are the parameters used.
        # The 'simulation_x' columns are the discharges modeled for each day.
        if minimize:
            best_index, best_obj_func = spotpy.analyser.get_minlikeindex(result)
        else:
            best_index, best_obj_func = spotpy.analyser.get_maxlikeindex(result)
            best_index = best_index[0][0]
        best_model_run = result[best_index]
        fields = [word for word in best_model_run.dtype.names if not word.startswith('sim')]
        best_params = list(best_model_run[fields])
        header = ['Optimal objective value'] + params_to_calibrate
        with open(fp.calibration_stats, 'w') as f:
            for name, param in zip(header, best_params):
                f.write(f'{name}, {param:.8f}\n')
        fields = [word for word in best_model_run.dtype.names if word.startswith('sim')]
        best_simulation = list(best_model_run[fields])
        # Add a time column
        start_datetime = pd.to_datetime(area_config.start_date) + timedelta(days=warmup)
        end_datetime = pd.to_datetime(area_config.end_date)
        times = pd.date_range(start_datetime, end_datetime, freq='D')
        
        df = times.to_frame(index=True, name='Date')
        df.set_index('Date', inplace=True)
        if multi_calibration:
            half_index = len(best_simulation)//2
            df['Simulated1'] = best_simulation[:half_index]
            df['Simulated2'] = best_simulation[half_index:]
        else:
            df['Simulated'] = best_simulation
        df.to_csv(fp.calibration_output, header='Best fit simulated outlet discharge time series', 
                  date_format='%d/%m/%Y')
        df_obs = times.to_frame(index=True, name='Date')
        df_obs.set_index('Date', inplace=True)
        if multi_calibration:
            df_obs['Observed1'] = spot_setup.evaluation()[0]
            df_obs['Observed2'] = spot_setup.evaluation()[1]
        else:
            print(spot_setup.evaluation())
            df_obs['Observed'] = spot_setup.evaluation()[0]
        
        # Plot simulation
        fig_simulation = plt.figure(figsize=(20, 9))
        ax = plt.subplot(1, 1, 1)
        if multi_calibration:
            ax.plot(df['Simulated1'], color='black', linestyle='solid', label='Best obj. func.=' + str(best_obj_func))
            ax.plot(df_obs['Observed1'], 'r:', markersize=3, label='Observation data')
        else:
            ax.plot(df['Simulated'], color='black', linestyle='solid', label='Best obj. func.=' + str(best_obj_func))
            ax.plot(df_obs['Observed'], 'r:', markersize=3, label='Observation data')
        plt.xlabel('Years')
        plt.ylabel('Discharge [mm/d]')
        plt.legend(loc='upper right')
        plt.savefig(fp.discharge + '.svg', format="svg", bbox_inches='tight', dpi=30)
        plt.savefig(fp.discharge + '.png', format="png", bbox_inches='tight', dpi=100)

        # Dump all outputs
        socont.dump_outputs(str(fp.results))
        
        print('End of calibration run')
        return
    
    elif modeling == 'forcing' or modeling == 'median_forcing' or modeling == 'top_forcing':
        forcing_run(socont, parameters, forcing, area_config.start_date, area_config.end_date, 
                    fp, forcing_stats_and_output, warmup, set_params, 
                    f"melt:{melt_model}", with_debris, metrics=metrics)
    
         # Water balance
        precip_total = forcing.get_total_precipitation()
        discharge_total = socont.get_total_outlet_discharge()
        et_total = socont.get_total_et()
        storage_change = socont.get_total_water_storage_changes()
        snow_change = socont.get_total_snow_storage_changes()   # GET THE SNOW COVER??? HOW DO I INITIALIZE IT???
        balance = discharge_total + et_total + storage_change + snow_change - precip_total
        print('precip_total', precip_total)
        print('discharge_total', discharge_total)
        print('et_total', et_total)
        print('storage_change', storage_change)
        print('snow_change', snow_change)
        print('balance', balance)

        # Dump all outputs
        socont.dump_outputs(str(fp.results))
        print('End of (median/top) forcing run')
        return
        
RCP_modeling = False


area = 'Arolla'
catchments = ['BI', 'HGDA', 'TN', 'PI', 'BS', 'VU', 'DB'] # Ordered by area.
melt_models = ['degree_day', 'degree_day_aspect', 'temperature_index']
if RCP_modeling:
    meteo_models = ["CH2018_RCP26", "CH2018_RCP45", "CH2018_RCP85"]
else:
    meteo_models = ["MeteoSwiss"] # Choose between "CH2018_RCP26", "CH2018_RCP45", "CH2018_RCP85", "Crespi", "MeteoSwiss"
for meteo_data in meteo_models:
    for melt_model in melt_models:
        if melt_model == 'temperature_index':
            cr = True
        else:
            cr = False
        for catchment in catchments:
            main_script(catchment, area, meteo_data, obj_func='kge_2012', modeling='calibration', computing_radiation=cr, melt_model=melt_model) # multi_calibration='BI'
            main_script(catchment, area, meteo_data, obj_func='None', modeling='bootstrapping', include_warmup=False) #, metrics=False (for Solda nodes)
            main_script(catchment, area, meteo_data, obj_func='kge_2012', modeling='forcing', computing_radiation=False, melt_model=melt_model)

print("END OF MAIN LOOP")


