from datetime import datetime
from hydrobricks.preprocessing import catchment

import geopandas as gpd
import hydrobricks as hb
import hydrobricks.behaviours as behaviours
import hydrobricks.models as models
import matplotlib.pyplot as plt
import numpy as np
import pandas as pd
import rasterio
import rioxarray
import spotpy
import xarray as xa

print(hb.__file__)


def compute_the_elevation_interpolations_HR(tif_filename, tif_CRS, pet_nc_file, pr_nc_file, tas_nc_file, nc_CRS, 
                                            forcing, outline, outline_epsg, target_epsg, elevation_bands, var_bands, 
                                            meteo_data):
    start, end = forcing.initialize_from_netcdf_HR(pet_nc_file, 'pet', tif_filename, 
                                                   outline, outline_epsg, target_epsg, elevation_bands, tif_CRS, nc_CRS,
                                                   column_time='time', time_format='%Y-%m-%d',
                                                   content={'pet': 'pet_sim(mm/day)'})
    forcing.spatialize('pet','from_gridded_data')
    print('start, end = ', start, end)
    start, end = forcing.initialize_from_netcdf_HR(pr_nc_file, 'pr', tif_filename,
                                                   outline, outline_epsg, target_epsg, elevation_bands, tif_CRS, nc_CRS,
                                                   column_time='time', time_format='%Y-%m-%d',
                                                   content={'precipitation': 'precip(mm/day)'})
    forcing.spatialize('precipitation','from_gridded_data')
    print('start, end = ', start, end)
    start, end = forcing.initialize_from_netcdf_HR(tas_nc_file, 'tas', tif_filename, 
                                                   outline, outline_epsg, target_epsg, elevation_bands, tif_CRS, nc_CRS,
                                                   column_time='time', time_format='%Y-%m-%d',
                                                   content={'temperature': 'temp(C)'})
    forcing.spatialize('temperature','from_gridded_data')
    print('start, end = ', start, end)

    for name, data in zip(forcing.data_name, forcing.data_raw):
        np.savetxt(var_bands + name + f"_HR_{meteo_data}.csv", data.T, delimiter=",", header=name)
    for name, data_interp in zip(forcing.data_name, forcing.data_spatialized):
        np.savetxt(var_bands + name + f"_interp_HR_{meteo_data}.csv", data_interp, delimiter=",", header=name)
    np.savetxt(var_bands + f"time_HR_{meteo_data}.csv", forcing.time, delimiter=",", header='time', fmt="%s")

def compute_the_elevation_interpolations(forcing, elevation_thrs):
    start, end = forcing.initialize_from_netcdf('/home/anne-laure/Documents/Datasets/Outputs/CH2018_pet_Out.nc', 'pet',
                                   '/home/anne-laure/Documents/Datasets/Outputs/Swiss_reproj_resamp.nc', elevation_thrs,
                                   column_time='time', time_format='%Y-%m-%d', #ref_start_datetime=datetime(1900,1,1,0,0,0),
                                   content={'pet': 'pet_sim(mm/day)'})
    forcing.spatialize('pet','from_gridded_data')
    print('start, end = ', start, end)
    start, end = forcing.initialize_from_netcdf('/home/anne-laure/Documents/Datasets/Outputs/CH2018_pr_Out.nc', 'pr',
                               '/home/anne-laure/Documents/Datasets/Outputs/Swiss_reproj_resamp.nc', elevation_thrs,
                               column_time='time', time_format='%Y-%m-%d', #ref_start_datetime=datetime(1900,1,1,0,0,0),
                               content={'precipitation': 'precip(mm/day)'})
    forcing.spatialize('precipitation','from_gridded_data')
    print('start, end = ', start, end)
    start, end = forcing.initialize_from_netcdf('/home/anne-laure/Documents/Datasets/Outputs/CH2018_tas_Out.nc', 'tas',
                                   '/home/anne-laure/Documents/Datasets/Outputs/Swiss_reproj_resamp.nc', elevation_thrs,
                                   column_time='time', time_format='%Y-%m-%d', #ref_start_datetime=datetime(1900,1,1,0,0,0),
                                   content={'temperature': 'temp(C)'})
    forcing.spatialize('temperature','from_gridded_data')
    print('start, end = ', start, end)

    for name, data in zip(forcing.data_name, forcing.data_raw):
        np.savetxt(var_bands + name + ".csv", data.T, delimiter=",", header=name)
    for name, data_interp in zip(forcing.data_name, forcing.data_spatialized):
        np.savetxt(var_bands + name + "_interp.csv", data_interp, delimiter=",", header=name)
    np.savetxt(var_bands + "time.csv", forcing.time, delimiter=",", header='time', fmt="%s")

def preprocess_glacier_cover_change(debris_covered_glaciers, glacier_shapefile, catchment_shapefile, nc_topo_file, elevation_bands, outline_epsg, target_epsg):

    # Read file using gpd.read_file()
    all_glaciers = gpd.read_file(glacier_shapefile)
    catchment = gpd.read_file(catchment_shapefile)

    # Need to correct the EPSG of the outline (wrong metadata)
    catchment.set_crs(outline_epsg, allow_override=True, inplace=True)

    # Convert all shapefiles to the target CRS
    all_glaciers.to_crs(target_epsg, inplace=True)
    catchment.to_crs(target_epsg, inplace=True)

    # Clip the glaciated polygons by the catchment polygon, and the debris polygons by the glaciated polygons.
    glaciers = gpd.clip(all_glaciers, catchment)

    # Compute total area of glaciated area.
    glaciated_area = 0
    for _, row in glaciers.iterrows():
        poly_area = row['geometry'].area
        glaciated_area += poly_area

    # Compute total area of catchment. Loop through in case of remaining small polygons.
    catchment_area = 0
    for _, row in catchment.iterrows():
        poly_area = row['geometry'].area
        catchment_area += poly_area

    # Substract the glaciated area from the bare rock area.
    bare_rock_area = catchment_area - glaciated_area
    glaciated_percentage = glaciated_area / catchment_area * 100

    # If we have the debris-covered ice shapefiles only:
    if debris_covered_glaciers != None:
        all_debris_glaciers = gpd.read_file(debris_covered_glaciers)
        all_debris_glaciers.to_crs(target_epsg, inplace=True)
        debris_glaciers = gpd.clip(all_debris_glaciers, glaciers)

        # Compute total area of debris glaciated area.
        debris_glaciated_area = 0
        for _, row in debris_glaciers.iterrows():
            poly_area = row['geometry'].area
            debris_glaciated_area += poly_area

        bare_ice_area = glaciated_area - debris_glaciated_area
        bare_ice_percentage = bare_ice_area / glaciated_area * 100

    else:
        debris_glaciated_area = np.nan
        bare_ice_area = np.nan
        bare_ice_percentage = np.nan

    m2_to_km2 = 1/1000000
    print("The catchment has an area of {:.1f} km², "
          "from which {:.1f} km² are glaciated, and "
          "{:.1f} km² are bare rock.".format(catchment_area * m2_to_km2, glaciated_area * m2_to_km2, bare_rock_area * m2_to_km2))
    print(f"The catchment is thus {glaciated_percentage:.1f}% glaciated.")
    print("The glaciers have {:.1f} km² of bare ice, "
          "and {:.1f} km² of debris-covered ice, thus "
          "amounting to {:.1f}% of bare ice.".format(bare_ice_area * m2_to_km2, debris_glaciated_area * m2_to_km2, bare_ice_percentage))

    #Find pixel size
    ras = rasterio.open(nc_topo_file)
    a = ras.transform
    x, y = abs(a[0]), abs(a[4])
    pixel_area = x * y
    print('The dataset in', glaciers.crs, 'has a spatial resolution of',
          x, 'x', y, 'm thus giving pixel areas of', pixel_area, 'm².')
    
    thrs_min = elevation_bands.elevation_thrs_min.values
    thrs_max = elevation_bands.elevation_thrs_max.values

    band_nb = len(thrs_min) #len(self.hydro_units['elevation'])
    debris_area = np.zeros(band_nb)
    bare_ice_area = np.zeros(band_nb)
    bare_rock_area = np.zeros(band_nb)
    glacier_area = np.zeros(band_nb)
    xds = rioxarray.open_rasterio(nc_topo_file)

    catchment_clipped = xds.rio.clip(catchment.geometry.values, catchment.crs)
    glaciers_clipped = xds.rio.clip(glaciers.geometry.values, glaciers.crs)

    if debris_covered_glaciers != None:
        debris_clipped = xds.rio.clip(debris_glaciers.geometry.values, debris_glaciers.crs)

    for i, (elev_min, elev_max) in enumerate(zip(thrs_min, thrs_max)):
        topo_mask_catchm = xa.where((catchment_clipped > elev_min) & (catchment_clipped < elev_max), 1, 0) # Elevation band set to 1, rest to 0.
        topo_mask_glacie = xa.where((glaciers_clipped > elev_min) & (glaciers_clipped < elev_max), 1, 0)

        catchment_area = np.nansum(topo_mask_catchm) * pixel_area
        glacier_area[i] = np.nansum(topo_mask_glacie) * pixel_area
        bare_rock_area[i] = catchment_area - glacier_area[i]

        # If we have the debris-covered ice shapefiles only:
        if debris_covered_glaciers != None:
            topo_mask_debris = xa.where((debris_clipped > elev_min) & (debris_clipped < elev_max), 1, 0)
            debris_area[i] = np.nansum(topo_mask_debris) * pixel_area
            bare_ice_area[i] = glacier_area[i] - debris_area[i]
        else:
            debris_area[i] = np.nan
            bare_ice_area[i] = np.nan

    print("After shapefile to raster conversion, the glaciers have {:.1f} km² of bare ice, "
          "{:.1f} km² of debris-covered ice, and {:.1f} km² of bare rock."
          "".format(np.sum(bare_ice_area) * m2_to_km2, np.sum(debris_area) * m2_to_km2, np.sum(bare_rock_area) * m2_to_km2))

    return debris_area, bare_ice_area, bare_rock_area, glacier_area

def write_land_cover_evolution_files(varname, df, filename):
    
    header = pd.MultiIndex.from_product([[varname], list(df.columns.values)], names=['bands',''])
    df.columns = header
    df.to_csv(f'{filename}.csv')
    
    # The order is very important:
    dates = list(list(zip(*df.columns.values))[1])
    df.columns = dates
    df = df.transpose()
    df.index = pd.to_datetime(df.index)
    df_reindexed = df.reindex(pd.date_range(start=df.index.min(), end=df.index.max(), freq='YS'))
    df_reindexed = df_reindexed.interpolate(method='linear')
    df_reindexed.index = df_reindexed.index.strftime('%d/%m/%Y')
    df_reindexed = df_reindexed.transpose()
    header = pd.MultiIndex.from_product([[varname], list(df_reindexed.columns.values)], names=['bands',''])
    df_reindexed.columns = header
    df_reindexed.to_csv(f'{filename}_yearly_interp.csv')

def create_land_cover_evolution_files(elevation_bands, whole_glaciers, debris_glaciers, times, 
                                      outline, nc_topo_file, results, outline_epsg, target_epsg):
    # Creates files with land cover evolution as described here: https://hydrobricks.readthedocs.io/en/latest/doc/advanced.html
    
    # Creating Empty DataFrame and Storing it in variable df
    debris_df = pd.DataFrame(index = elevation_bands['elevation'].values.astype(int))
    ice_df = pd.DataFrame(index = elevation_bands['elevation'].values.astype(int))
    rock_df = pd.DataFrame(index = elevation_bands['elevation'].values.astype(int))
    glacier_df = pd.DataFrame(index = elevation_bands['elevation'].values.astype(int))
    
    for whole_glacier, debris_glacier, time in zip(whole_glaciers, debris_glaciers, times):
        debris, ice, rock, glacier = preprocess_glacier_cover_change(debris_glacier, whole_glacier, outline, nc_topo_file, 
                                                                     elevation_bands, outline_epsg, target_epsg)
        debris_df[time] = debris
        ice_df[time] = ice
        rock_df[time] = rock
        glacier_df[time] = glacier
        
    write_land_cover_evolution_files('glacier_debris', debris_df, f'{results}debris')
    write_land_cover_evolution_files('glacier_ice', ice_df, f'{results}ice')
    write_land_cover_evolution_files('ground', rock_df, f'{results}rock')
    write_land_cover_evolution_files('glacier', glacier_df, f'{results}glacier')
    
    first_debris = debris_df[debris_df.columns[0]].values
    first_ice = ice_df[ice_df.columns[0]].values
    first_rock = rock_df[rock_df.columns[0]].values
    first_glacier = glacier_df[glacier_df.columns[0]].values
    
    return first_debris, first_ice, first_rock, first_glacier


# This script takes the shapefile of a catchment, the associated DEM,
# and computes the mean elevation and the elevation bands, to finally convert them to CSV files.


def main_script(catchm, area):

    ####### INPUT FILES ################
    path = "/home/anne-laure/Documents/Datasets/"
    if area == 'Arolla':
        dem_path = f"{path}Swiss_Study_area/StudyAreas_EPSG21781.tif"
        outline = f"{path}Swiss_discharge/Arolla_discharge/Watersheds_on_dhm25/{catchm}_UpslopeArea_EPSG21781/out.shp" #f"{path}Swiss_Study_area/LaNavisence_Chippis/125595_EPSG21781.shp"
        outline_epsg = 21781
        method = 'set_isohypses' 
        min_val = 1900
        max_val = 3900
        distance = 40
    elif area == 'Solda':
        dem_path = f"{path}Italy_Study_area/dtm_25m_utm_st_whole_StudyArea_OriginalCRS_filled.tif"
        outline = f"{path}Italy_discharge/Watersheds/{catchm}_UpslopeArea_EPSG25832/out.shp"
        outline_epsg = 25832
        method = 'set_isohypses' 
        min_val = 1100
        max_val = 3900
        distance = 70
    
    ####### DISCHARGE DATASETS #########
    discharge_file = f"{path}Outputs/ObservedDischarges/{area}_hydrobricks_discharge_{catchm}.csv"
    column_time = 'Date'
    time_format = '%d/%m/%Y'
    content = {'discharge': 'Discharge (mm/d)'}
    
    ####### MODELISATION TIMESPAN ######
    if area == 'Arolla':
        start_date = '2009-01-01' #'1989-01-01'
        end_date = '2014-12-31'
    elif area == 'Solda':
        if catchm == 'PS':
            start_date = '2014-01-10'
            end_date = '2018-12-31' # Can't go later ('2021-12-22') since Crespi's data ends in 2018.
        elif catchm == 'SGF':
            start_date = '2017-06-29'
            end_date = '2020-05-25'
        elif catchm == 'TF':
            start_date = '2017-06-20'
            end_date = '2019-10-16'
        elif catchm == 'ZA':
            start_date = '2017-06-21'
            end_date = '2018-11-14'
    
    ####### MODELISATION PARAMETERS ####
    inversion = True
    with_debris = False
    compute_altitudinal_trends = True
    run_model = False
    meteo_data = "Crespi" # Choose between "CH2018", "CrespiEtAl", "MeteoSwiss"
    if area == 'Arolla':
        assert(meteo_data == "MeteoSwiss" or meteo_data == "CH2018")
    elif area == 'Solda':
        assert(meteo_data == "Crespi")
    if inversion:
        if with_debris:
            inversed_params = ['A', 'a_snow', 'k_slow_1', 'k_slow_2', 
                               'k_quick', 'percol',
                               'a_ice_1', 'a_ice_2', 'k_snow', 'k_ice'] # 'precip_corr_factor'
        else:
            inversed_params = ['A', 'a_snow', 'k_slow_1', 'k_slow_2', 
                               'k_quick', 'percol',
                               'a_ice', 'k_snow', 'k_ice'] # 'precip_corr_factor'
        # Select the number of maximum repetitions
        warmup = 365 # The first year is completely ignored, as it is just the water accumulating in the right places 
        # (it's usual in hydrological modeling), and all of the rest of the years is used for calibration (aka. evaluation).
        max_rep = 10000 #10 #10000
        algorithm = 'SCEUA' # 'SCEUA' 'MC' # The result filename depends on this algorithm name.
        # As SCE-UA is minizing the objective function we need a objective function that gets better with decreasing values.
        obj_func = 'kge_2012' #'kge_2012' #'nse' #'mse'
        invert_obj_func=True
    else:
        set_params = {'A': 458, 'a_snow': 1.8, 'k_slow_1': 0.9, 'k_slow_2': 0.8,
                      'k_quick': 1, 'percol': 9.8}
    
    ####### CHANGING GLACIER COVER #####
    if area == 'Arolla':
        glacier_path = f'{path}Swiss_GlaciersExtents/'
        nc_topo_file = f'{path}Outputs/{meteo_data}_Swiss_reproj.nc'
        whole_glaciers = [f'{glacier_path}inventory_sgi1850_r1992/SGI_1850.shp', f'{glacier_path}inventory_sgi1931_r2022/SGI_1931.shp',
                          f'{glacier_path}inventory_sgi1973_r1976/SGI_1973.shp', f'{glacier_path}inventory_sgi2010_r2010/SGI_2010.shp',
                          f'{glacier_path}inventory_sgi2016_r2020/SGI_2016_glaciers.shp']
        debris_glaciers = [None, None, None, None, f'{glacier_path}inventory_sgi2016_r2020/SGI_2016_debriscover.shp']
        times = ['01/01/1850', '01/01/1931', '01/01/1973', '01/01/2010', '01/01/2016']
        target_epsg = 21781
        # BUGS if EPSG:4326...
    elif area == 'Solda':
        glacier_path = f'{path}Italy_GlaciersExtents/'
        nc_topo_file = f'{path}Outputs/{meteo_data}_Italy_reproj.nc'
        whole_glaciers = [f'{glacier_path}SaraSavi/SuldenGlacier1985_fromOrthophoto_EPSG25832.shp', f'{glacier_path}GlaciersExtents/GlaciersExtents1997/glaciers_1997_EPSG25832.shp',
                          f'{glacier_path}GlaciersExtents/GlaciersExtents2005/glaciers_2015_EPSG25832.shp', f'{glacier_path}GlaciersExtents/GlaciersExtents2016_2017/glaciers_2016_2017_EPSG25832.shp',
                          f'{glacier_path}SaraSavi/SuldenGlacier2018_fromOrthophoto_EPSG25832.shp']
        debris_glaciers = [None, None, None, None, None]
        times = ['01/01/1985', '01/01/1997', '01/01/2005', '01/01/2016', '01/01/2018']
        target_epsg = 25832
    
    ####### ELEVATION PROFILES #########
    meteo_start_date = None
    meteo_end_date = None
    tif_filename = dem_path
    if area == 'Arolla':
        tif_CRS = "EPSG:21781"
        nc_CRS = "EPSG:21781"
    elif area == 'Solda':
        tif_CRS = "EPSG:25832"
        nc_CRS = "EPSG:25832"
    pet_nc_file = f'{path}Outputs/{meteo_data}_pet_Out.nc'
    pr_nc_file = f'{path}Outputs/{meteo_data}_pr_Out.nc'
    tas_nc_file = f'{path}Outputs/{meteo_data}_tas_Out.nc'
    
    ####### RESULT FILES ###############
    results = f"{path}Outputs/{area}/{catchm}/"
    figures = f"{path}OutputFigures/{area}/{catchm}/"
    elev_bands = f"{results}elevation_bands.csv"
    elev_thres_min = f"{results}elevation_threshold_mins.csv"
    elev_thres_max = f"{results}elevation_threshold_maxs.csv"
    var_bands = f"{results}spatialized_"
    output = f"{results}simulated_discharge.csv"
    output2 = f"{results}best_fit_simulated_discharge"
    stats = f"{results}simulation_stats.csv"
    stats2 = f"{results}best_fit_simulation_stats"
    
    #######################################################################
    
    study_area = catchment.Catchment(outline=outline)
    success = study_area.extract_dem(dem_path)
    print("DEM extracted:", success)
    mean_elev = study_area.get_mean_elevation()
    print("Mean elevation:", mean_elev)
    elevation_bands, null_areas = study_area.get_elevation_bands(method=method, min_val=min_val, max_val=max_val, distance=distance)
    print("Elevation bands:", elevation_bands)
    
    np.savetxt(elev_thres_min, elevation_bands.elevation_thrs_min.values, header='Elevation thresholds min')
    np.savetxt(elev_thres_max, elevation_bands.elevation_thrs_max.values, header='Elevation thresholds max')
    
    # FORMAT FOR THE CHANGING INITIALISATION
    debris, ice, rock, glacier = create_land_cover_evolution_files(elevation_bands, whole_glaciers, debris_glaciers, times, outline, nc_topo_file,
                                                                   results, outline_epsg=outline_epsg, target_epsg=target_epsg)
    
    # FORMAT FOR THE CONSTANT INITIALISATION
    if with_debris == True:
        elevation_bands['Area Debris'] = debris
        elevation_bands['Area Ice'] = ice
    else:
        elevation_bands['Area Glacier'] = glacier
    elevation_bands['Area Ground'] = rock
    
    elevation_bands.to_csv(elev_bands)
    
    if with_debris:
        land_cover_names = ['ground', 'glacier_ice', 'glacier_debris']
        land_cover_types = ['ground', 'glacier', 'glacier']
    else:
        land_cover_names = ['ground', 'glacier']
        land_cover_types = ['ground', 'glacier']
    
    # Model structure
    socont = models.Socont(soil_storage_nb=2, surface_runoff="linear_storage",
                           land_cover_names=land_cover_names,
                           land_cover_types=land_cover_types,
                           record_all=False)
    
    # Parameters
    parameters = socont.generate_parameters()
    if inversion:
        parameters.allow_changing = inversed_params
    else:
        parameters.set_values(set_params)
    
    # Hydro units
    hydro_units = hb.HydroUnits(land_cover_types, land_cover_names)
    if with_debris:
        hydro_units.load_from_csv(
           elev_bands, area_unit='m2', column_elevation='elevation',
           columns_areas={'ground': 'Area Ground',
                          'glacier_ice': 'Area Ice',
                          'glacier_debris': 'Area Debris'})
    else:
        hydro_units.load_from_csv(
           elev_bands, area_unit='m2', column_elevation='elevation',
           columns_areas={'ground': 'Area Ground',
                          'glacier': 'Area Glacier'})
    
    # Meteo data
    forcing = hb.Forcing(hydro_units)
    
    changes = behaviours.BehaviourLandCoverChange()
    changes.load_from_csv(f'{results}rock.csv', hydro_units, area_unit='m2', match_with='elevation')
    if with_debris:
        changes.load_from_csv(f'{results}debris.csv', hydro_units, area_unit='m2', match_with='elevation')
        changes.load_from_csv(f'{results}ice.csv', hydro_units, area_unit='m2', match_with='elevation')
    else:
        changes.load_from_csv(f'{results}glacier.csv', hydro_units, area_unit='m2', match_with='elevation')
    socont.add_behaviour(changes)
    
    
    if compute_altitudinal_trends:
        compute_the_elevation_interpolations_HR(tif_filename, tif_CRS, pet_nc_file, pr_nc_file, tas_nc_file, nc_CRS, forcing, 
                                                outline, outline_epsg, target_epsg, elevation_bands, var_bands, 
                                                meteo_data)
    else:
        forcing.load_spatialized_data_from_csv('precipitation', var_bands + f"precipitation_interp_HR_{meteo_data}.csv", var_bands + f"time_HR_{meteo_data}.csv", time_format='%Y-%m-%d', dropindex=null_areas)
        forcing.load_spatialized_data_from_csv('temperature', var_bands + f"temperature_interp_HR_{meteo_data}.csv", var_bands + f"time_HR_{meteo_data}.csv", time_format='%Y-%m-%d', dropindex=null_areas)
        forcing.load_spatialized_data_from_csv('pet', var_bands + f"pet_interp_HR_{meteo_data}.csv", var_bands + f"time_HR_{meteo_data}.csv", time_format='%Y-%m-%d', dropindex=null_areas)
    
    if run_model:
    
        # Obs data
        obs = hb.Observations()
        obs.load_from_csv(discharge_file, column_time=column_time, time_format=time_format,
                          content=content, start_date=start_date, end_date=end_date)
        
        
        
        print('Debug 1', flush=True)
        # Model setup
        socont.setup(spatial_structure=hydro_units, output_path=str(results),
                     start_date=start_date, end_date=end_date)
        
        if inversion:
            if obj_func == 'mse' or obj_func == 'kge_2012':
                spot_setup = hb.SpotpySetup(socont, parameters, forcing, obs, warmup=warmup, 
                                            obj_func=obj_func, 
                                            invert_obj_func=invert_obj_func)
            elif obj_func == 'nse':
                spot_setup = hb.SpotpySetup(socont, parameters, forcing, obs, warmup=warmup, 
                                            obj_func=spotpy.objectivefunctions.nashsutcliffe, 
                                            invert_obj_func=invert_obj_func)
            else:
                print('Error: Objective function not yet registered as either Invert or not.')
            
            # Run spotpy
            if algorithm == 'SCEUA':
                result_filename = f"{results}socont_SCEUA_{obj_func}"
                sampler = spotpy.algorithms.sceua(spot_setup, dbname=result_filename, dbformat='csv', save_sim=True)
            elif algorithm == 'MC':
                result_filename = f"{results}socont_MC_{obj_func}"
                sampler = spotpy.algorithms.mc(spot_setup, dbname=result_filename, dbformat='csv', save_sim=True)
            else:
                print('Error: No SPOTPY algorithm was defined!')
            sampler.sample(max_rep)
            
            # Load the results
            result = sampler.getdata()
            
            # Plot parameter interaction
            spotpy.analyser.plot_parameterInteraction(result)
            plt.savefig(f'{figures}parameter_interaction_{algorithm}_{obj_func}.svg', format="svg", bbox_inches='tight', dpi=30)
            plt.savefig(f'{figures}parameter_interaction_{algorithm}_{obj_func}.png', format="png", bbox_inches='tight', dpi=100)
            
            #######################################################################
        
            # Load the results
            result = spotpy.analyser.load_csv_results(result_filename)
            
            # Plot evolution
            fig_evolution = plt.figure(figsize=(9, 5))
            if invert_obj_func:
                plt.plot(-result['like1'])
            else:
                plt.plot(result['like1'])
            plt.ylabel(obj_func)
            plt.xlabel('Iteration')
            plt.savefig(f'{figures}{obj_func}_comparison_{algorithm}.svg', format="svg", bbox_inches='tight', dpi=30)
            plt.savefig(f'{figures}{obj_func}_comparison_{algorithm}.png', format="png", bbox_inches='tight', dpi=100)
            
            # Get best results
            # Each row is a simulation result. The first columns are the parameters used.
            # The 'simulation_x' columns are the discharges modeled for each day.
            best_index, best_obj_func = spotpy.analyser.get_minlikeindex(result)
            best_model_run = result[best_index]
            fields = [word for word in best_model_run.dtype.names if not word.startswith('sim')]
            best_params = list(best_model_run[fields])
            header = ['Minimal objective value'] + inversed_params
            with open(f"{stats2}_{algorithm}_{obj_func}.csv", 'w') as f:
                for name, param in zip(header, best_params):
                    f.write(f'{name}, {param:.8f}\n')
            fields = [word for word in best_model_run.dtype.names if word.startswith('sim')]
            best_simulation = list(best_model_run[fields])
            np.savetxt(f"{output2}_{algorithm}_{obj_func}.csv", best_simulation, header='Best fit simulated outlet discharge time series')
            
            # Plot simulation
            fig_simulation = plt.figure(figsize=(20, 9))
            ax = plt.subplot(1, 1, 1)
            ax.plot(best_simulation, color='black', linestyle='solid', label='Best obj. func.=' + str(best_obj_func))
            ax.plot(spot_setup.evaluation(), 'r:', markersize=3, label='Observation data')
            plt.xlabel('Number of Observation Points')
            plt.ylabel('Discharge [mm/d]')
            plt.legend(loc='upper right')
            plt.savefig(f'{figures}discharge_{algorithm}_{obj_func}.svg', format="svg", bbox_inches='tight', dpi=30)
            plt.savefig(f'{figures}discharge_{algorithm}_{obj_func}.png', format="png", bbox_inches='tight', dpi=100)
            
            #######################################################################
            
            # Plot posterior parameter distribution # ERROR RIGHT NOW: ValueError: `dataset` input should have multiple elements.
            posterior = spotpy.analyser.get_posterior(result, percentage=10)
            #spotpy.analyser.plot_parameterInteraction(posterior)
            #plt.tight_layout()
            #plt.show()
            print('End')
            
        
        else:
            print('Debug 2', flush=True)
            # Initialize and run the model
            socont.initialize_state_variables(parameters=parameters, forcing=forcing)
            print('Debug 3', flush=True)
            socont.run(parameters=parameters, forcing=forcing)
            print('Debug 4', flush=True)
            
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
            #assert balance == pytest.approx(0, abs=1e-8)
            
            # Get outlet discharge time series. WHAT IS THE DIFFERENCE WITH TOTAL OUTLET DISCHARGE???!!!
            sim_ts = socont.get_outlet_discharge()
            print('Debug 5', flush=True)
            np.savetxt(output, sim_ts, header='Simulated outlet discharge time series')
            
            # Evaluate
            obs_ts = obs.data_raw[0]
            print('Debug 6', flush=True)
            nse = socont.eval('nse', obs_ts)
            print('Debug 7', flush=True)
            kge_2012 = socont.eval('kge_2012', obs_ts)
            
            with open(stats, 'w') as f:
                f.write(f'nse, {nse:.3f}\n')
                f.write(f'kge_2012, {kge_2012:.3f}')
            
            print(f"nse = {nse:.3f}, kge_2012 = {kge_2012:.3f}")
            print('End')
        
        
        

catchments = ['BI', 'BS', 'DB', 'HGDA', 'PI', 'TN', 'VU']
catchments = ['TF', 'ZA'] #['PS', 'SGF', 'TF', 'ZA']
area = 'Solda' # 'Arolla', 'Solda'
for catchm in catchments:
    main_script(catchm, area)

# Load the results
inversed_params = ['A', 'a_snow', 'k_slow_1', 'k_slow_2', 
                           'k_quick', 'percol',
                           'a_ice', 'k_snow', 'k_ice']
obj_func = 'kge_2012'
algorithm = 'SCEUA'
results = f"/home/anne-laure/Documents/Datasets/Outputs/Arolla/{catchm}/"
figures = f"/home/anne-laure/Documents/Datasets/OutputFigures/Arolla/{catchm}/"
result_filename = f"{results}socont_{algorithm}_{obj_func}"
stats2 = f"{results}best_fit_simulation_stats"
result = spotpy.analyser.load_csv_results(result_filename)
print('result:', result)

# Plot evolution
fig_evolution = plt.figure(figsize=(9, 5))
plt.plot(-result['like1'])
plt.ylabel(obj_func)
plt.xlabel('Iteration')
plt.show()

# Get best results
# Each row is a simulation result. The first columns are the parameters used.
# The 'simulation_x' columns are the discharges modeled for each day.
best_index, best_obj_func = spotpy.analyser.get_maxlikeindex(result)
print('best_index, best_obj_func:', best_index, best_obj_func)
best_model_run = result[best_index][0]
print('best_model_run:', best_model_run)
fields = [word for word in best_model_run.dtype.names if not word.startswith('sim')]
best_params = list(best_model_run[fields])
header = ['Minimal objective value'] + inversed_params
#with open(f"{stats2}_{algorithm}_{obj_func}.csv", 'w') as f:
#    for name, param in zip(header, best_params):
#        f.write(f'{name}, {param:.8f}\n')
fields = [word for word in best_model_run.dtype.names if word.startswith('sim')]
best_simulation = list(best_model_run[fields])
print('best_simulation:', best_simulation)

# Plot simulation
fig_simulation = plt.figure(figsize=(20, 9))
ax = plt.subplot(1, 1, 1)
ax.plot(best_simulation, color='black', linestyle='solid',
        label='Best obj. func.=' + str(best_obj_func))
#ax.plot(spot_setup.evaluation(), 'r:', markersize=3, label='Observation data')
plt.xlabel('Number of Observation Points')
plt.ylabel('Discharge [mm/d]')
plt.legend(loc='upper right')
plt.show()

