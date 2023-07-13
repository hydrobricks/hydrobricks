from datetime import datetime

import hydrobricks as hb

print(hb.__file__)
import geopandas as gpd
import hydrobricks.models as models
import numpy as np
import rasterio
import rioxarray
import xarray as xa
from hydrobricks.preprocessing import catchment


def compute_the_elevation_interpolations_HR(forcing, elevation_thrs):

    tif_filename = f'/home/anne-laure/Documents/Datasets/Swiss_Study_area/ValDAnniviers_EPSG21781.tif'
    tif_CRS = "EPSG:21781"
    nc_CRS = "EPSG:21781"
    start, end = forcing.initialize_from_netcdf_HR('/home/anne-laure/Documents/Datasets/Outputs/CH2018_pet_TestOut.nc', 'pet',
                                   tif_filename, elevation_thrs, tif_CRS, nc_CRS,
                                   column_time='time', time_format='%Y-%m-%d', #ref_start_datetime=datetime(1900,1,1,0,0,0),
                                   content={'pet': 'pet_sim(mm/day)'})
    forcing.spatialize('pet','from_gridded_data')
    print('start, end = ', start, end)
    start, end = forcing.initialize_from_netcdf_HR('/home/anne-laure/Documents/Datasets/Outputs/CH2018_pr_TestOut.nc', 'pr',
                               tif_filename, elevation_thrs, tif_CRS, nc_CRS,
                               column_time='time', time_format='%Y-%m-%d', #ref_start_datetime=datetime(1900,1,1,0,0,0),
                               content={'precipitation': 'precip(mm/day)'})
    forcing.spatialize('precipitation','from_gridded_data')
    print('start, end = ', start, end)
    start, end = forcing.initialize_from_netcdf_HR('/home/anne-laure/Documents/Datasets/Outputs/CH2018_tas_TestOut.nc', 'tas',
                                   tif_filename, elevation_thrs, tif_CRS, nc_CRS,
                                   column_time='time', time_format='%Y-%m-%d', #ref_start_datetime=datetime(1900,1,1,0,0,0),
                                   content={'temperature': 'temp(C)'})
    forcing.spatialize('temperature','from_gridded_data')
    print('start, end = ', start, end)

    for name, data in zip(forcing.data_name, forcing.data_raw):
        np.savetxt(var_bands + name + "_HR.csv", data.T, delimiter=",", header=name)
    for name, data_interp in zip(forcing.data_name, forcing.data_spatialized):
        np.savetxt(var_bands + name + "_interp_HR.csv", data_interp, delimiter=",", header=name)
    np.savetxt(var_bands + "time_HR.csv", forcing.time, delimiter=",", header='time', fmt="%s")

def compute_the_elevation_interpolations(forcing, elevation_thrs):
    start, end = forcing.initialize_from_netcdf('/home/anne-laure/Documents/Datasets/Outputs/CH2018_pet_TestOut.nc', 'pet',
                                   '/home/anne-laure/Documents/Datasets/Outputs/Swiss_reproj_resamp.nc', elevation_thrs,
                                   column_time='time', time_format='%Y-%m-%d', #ref_start_datetime=datetime(1900,1,1,0,0,0),
                                   content={'pet': 'pet_sim(mm/day)'})
    forcing.spatialize('pet','from_gridded_data')
    print('start, end = ', start, end)
    start, end = forcing.initialize_from_netcdf('/home/anne-laure/Documents/Datasets/Outputs/CH2018_pr_TestOut.nc', 'pr',
                               '/home/anne-laure/Documents/Datasets/Outputs/Swiss_reproj_resamp.nc', elevation_thrs,
                               column_time='time', time_format='%Y-%m-%d', #ref_start_datetime=datetime(1900,1,1,0,0,0),
                               content={'precipitation': 'precip(mm/day)'})
    forcing.spatialize('precipitation','from_gridded_data')
    print('start, end = ', start, end)
    start, end = forcing.initialize_from_netcdf('/home/anne-laure/Documents/Datasets/Outputs/CH2018_tas_TestOut.nc', 'tas',
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

def preprocess_glacier_cover_change(debris_covered_glaciers, glacier_shapefile, catchment_shapefile, elevation_thrs, outline_epsg, target_epsg):

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
    ras = rasterio.open('/home/anne-laure/Documents/Datasets/Outputs/Swiss_reproj.nc')
    a = ras.transform
    x, y = abs(a[0]), abs(a[4])
    pixel_area = x * y
    print('The dataset in', glaciers.crs, 'has a spatial resolution of',
          x, 'x', y, 'm thus giving pixel areas of', pixel_area, 'm².')

    band_nb = len(elevation_thrs[:-1]) #len(self.hydro_units['elevation'])
    if debris_covered_glaciers != None:
        debris_area = np.zeros(band_nb)
        bare_ice_area = np.zeros(band_nb)
    else:
        debris_area = np.nan
        bare_ice_area = np.nan

    bare_rock_area = np.zeros(band_nb)
    glacier_area = np.zeros(band_nb)
    xds = rioxarray.open_rasterio('/home/anne-laure/Documents/Datasets/Outputs/Swiss_reproj.nc')

    catchment_clipped = xds.rio.clip(catchment.geometry.values, catchment.crs)
    glaciers_clipped = xds.rio.clip(glaciers.geometry.values, glaciers.crs)

    if debris_covered_glaciers != None:
        debris_clipped = xds.rio.clip(debris_glaciers.geometry.values, debris_glaciers.crs)

    for i, elev_min in enumerate(elevation_thrs[:-1]):
        elev_max = elevation_thrs[i+1]
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

    print("After shapefile to raster conversion, the glaciers have {:.1f} km² of bare ice, "
          "{:.1f} km² of debris-covered ice, and {:.1f} km² of bare rock."
          "".format(np.sum(bare_ice_area) * m2_to_km2, np.sum(debris_area) * m2_to_km2, np.sum(bare_rock_area) * m2_to_km2))

    return debris_area, bare_ice_area, bare_rock_area, glacier_area


# This script takes the shapefile of a catchment, the associated DEM,
# and computes the mean elevation and the elevation bands, to finally convert them to CSV files.

path1 = "/home/anne-laure/eclipse-workspace/eclipse-workspace/GSM-SOCONT/tests/files/catchments/Val_d_Anniviers/"
path2 = "/home/anne-laure/eclipse-workspace/eclipse-workspace/GSM-SOCONT/tests/files/catchments/ch_sitter_appenzell/"
results = "/home/anne-laure/Documents/Datasets/Outputs/"

date_input_format = '%Y%m%d'
date_output_format = '%d/%m/%Y'

dem_path = f"{path1}dem_EPSG21781.tif"
outline = f"{path1}outline.shp"
elev_bands = f"{path1}elevation_bands.csv"
elev_thres = f"{path1}elevation_thresholds.csv"
var_bands = f"{results}spatialized_"
output = f"{results}simulated_discharge.csv"

anniviers = catchment.Catchment(outline=outline)
success = anniviers.extract_dem(dem_path)
print("DEM extracted:", success)
mean_elev = anniviers.get_mean_elevation()
print("Mean elevation:", mean_elev)
elevation_bands, elevation_thrs = anniviers.get_elevation_bands(method='set_isohypses', min_val=1000, max_val=4000, distance=30)
print("Elevation bands:", elevation_bands)
np.savetxt(elev_thres, elevation_thrs, header='Elevation thresholds')

glacier_path = '/home/anne-laure/Documents/Datasets/Swiss_GlaciersExtent/'
whole_glaciers = f'{glacier_path}inventory_sgi1850_r1992/SGI_1850.shp'
debris, ice, rock, glacier = preprocess_glacier_cover_change(None, whole_glaciers, outline, elevation_thrs,
                                                    outline_epsg=21781, target_epsg=21781)
whole_glaciers = f'{glacier_path}inventory_sgi1931_r2022/SGI_1931.shp'
debris, ice, rock, glacier = preprocess_glacier_cover_change(None, whole_glaciers, outline, elevation_thrs,
                                                    outline_epsg=21781, target_epsg=21781)
whole_glaciers = f'{glacier_path}inventory_sgi1973_r1976/SGI_1973.shp'
debris, ice, rock, glacier = preprocess_glacier_cover_change(None, whole_glaciers, outline, elevation_thrs,
                                                    outline_epsg=21781, target_epsg=21781)
whole_glaciers = f'{glacier_path}inventory_sgi2010_r2010/SGI_2010.shp'
debris, ice, rock, glacier = preprocess_glacier_cover_change(None, whole_glaciers, outline, elevation_thrs,
                                                    outline_epsg=21781, target_epsg=21781)
debris_glaciers = f'{glacier_path}inventory_sgi2016_r2020/SGI_2016_debriscover.shp'
whole_glaciers = f'{glacier_path}inventory_sgi2016_r2020/SGI_2016_glaciers.shp'
debris, ice, rock, glacier = preprocess_glacier_cover_change(debris_glaciers, whole_glaciers, outline, elevation_thrs,
                                                    outline_epsg=21781, target_epsg=21781)
elevation_bands['Area Debris'] = debris
elevation_bands['Area Ice'] = ice
elevation_bands['Area Non Glacier'] = rock
elevation_bands.to_csv(elev_bands)

# Model structure
socont = models.Socont(soil_storage_nb=2, surface_runoff="linear_storage",
                       record_all=False)

# Parameters
parameters = socont.generate_parameters()
parameters.set_values({'A': 458, 'a_snow': 1.8, 'k_slow_1': 0.9, 'k_slow_2': 0.8,
                       'k_quick': 1, 'percol': 9.8})

land_cover_names = ['ground', 'glacier_ice', 'glacier_debris']
land_cover_types = ['ground', 'glacier', 'glacier']

# Hydro units
#hydro_units = hb.HydroUnits(land_cover_types, land_cover_names)
#hydro_units.load_from_csv(
#   elev_bands, area_unit='m2', column_elevation='elevation',
#   columns_areas={'ground': 'Area Non Glacier',
#                  'glacier_ice': 'Area Ice',
#                  'glacier_debris': 'Area Debris'})
hydro_units = hb.HydroUnits()
hydro_units.load_from_csv(elev_bands, area_unit='m2', column_elevation='elevation', column_area='area')

# Meteo data
forcing = hb.Forcing(hydro_units)

compute_altitudinal_trends = False
if compute_altitudinal_trends:
    compute_the_elevation_interpolations_HR(forcing, elevation_thrs)
else:
    forcing.load_spatialized_data_from_csv('precipitation', var_bands + "precipitation_interp_HR.csv", var_bands + "time.csv", time_format='%Y-%m-%d')
    forcing.load_spatialized_data_from_csv('temperature', var_bands + "temperature_interp_HR.csv", var_bands + "time.csv", time_format='%Y-%m-%d')
    forcing.load_spatialized_data_from_csv('pet', var_bands + "pet_interp_HR.csv", var_bands + "time.csv", time_format='%Y-%m-%d')

#forcing.load_from_csv(
#    f'{path}meteo.csv', column_time='Date', time_format='%d/%m/%Y',
#    content={'precipitation': 'precip(mm/day)', 'temperature': 'temp(C)',
#             'pet': 'pet_sim(mm/day)'})

# GERER LE FORMAT DES DATES !!! (dans initialize_from_netcdf)
#ref_elevation = 1250  # Reference altitude for the meteo data
#forcing.spatialize_temperature(ref_elevation, -0.6)
#forcing.spatialize_pet()
#forcing.spatialize_precipitation(ref_elevation=ref_elevation, gradient=0.05,
#                                 correction_factor=0.75)

# Obs data
obs = hb.Observations()
obs.load_from_csv(f'{path2}discharge.csv', column_time='Date', time_format='%d/%m/%Y',
                  content={'discharge': 'Discharge (mm/d)'})

print('Debug 1', flush=True)
# Model setup
socont.setup(spatial_structure=hydro_units, output_path=str(f'{path1}outputs'),
             start_date='1981-01-01', end_date='2020-12-31')

print('Debug 2', flush=True)
# Initialize and run the model
socont.initialize_state_variables(parameters=parameters, forcing=forcing)
print('Debug 3', flush=True)
socont.run(parameters=parameters, forcing=forcing)
print('Debug 4', flush=True)

# Get outlet discharge time series
sim_ts = socont.get_outlet_discharge()
print('Debug 5', flush=True)
np.savetxt(output, sim_ts, header='Simulated outlet discharge time series')

# Evaluate
obs_ts = obs.data_raw[0]
print('Debug 6', flush=True)
nse = socont.eval('nse', obs_ts)
print('Debug 7', flush=True)
kge_2012 = socont.eval('kge_2012', obs_ts)

print(f"nse = {nse:.3f}, kge_2012 = {kge_2012:.3f}")
print('End')