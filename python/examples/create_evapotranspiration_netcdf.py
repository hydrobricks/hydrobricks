# This is not a perennial way of importing PyETo:
import sys
from datetime import datetime

import numpy as np
import rasterio
import rioxarray  # Needed otherwise I get 'AttributeError: 'DataArray' object has no attribute 'rio''
import xarray as xa

sys.path.append('../PyETo/')
from pyeto import convert, fao


def print_info(verbose, filename, ds):
    if verbose is True:
        print(f"Input {filename}")
        print(ds)
        print(ds.__dict__)
        print(ds.variables)

def assert_same_extent(ds1, ds2):
    assert np.all(ds1['y'].to_numpy() == ds2['y'].to_numpy())
    assert np.all(ds1['x'].to_numpy() == ds2['x'].to_numpy())
    assert np.all(ds1['time'].to_numpy() == ds2['time'].to_numpy())

def compute_evapotranspiration(filename_mean, varname_mean, filename_min, varname_min, filename_max, varname_max, verbose):

    # Using the xarray library instead of the NetCDF4 preserves the Timestamps.
    ds = xa.open_dataset(filename_mean, decode_coords="all")
    d_mean_temp = ds[varname_mean].to_numpy()
    print_info(verbose, filename_mean, ds)
    time = ds['time'].to_numpy()
    lat_nc = ds['y'].to_numpy()
    lon_nc = ds['x'].to_numpy()

    ds_min = xa.open_dataset(filename_min, decode_coords="all")
    d_min_temp = ds_min[varname_min].to_numpy()
    print_info(verbose, filename_min, ds_min)
    assert_same_extent(ds, ds_min)

    ds_max = xa.open_dataset(filename_max, decode_coords="all")
    d_max_temp = ds_max[varname_max].to_numpy()
    print_info(verbose, filename_max, ds_max)
    assert_same_extent(ds, ds_max)

    eto = np.zeros(d_mean_temp.shape)
    # Functions that can handle matrices:
    lat_matrix = convert.deg2rad(lat_nc) # Convert latitude to radians

    # OPTIMIZE the called functions so that I can process the data quicker
    for d, t in enumerate(time):
        current_date = datetime.utcfromtimestamp(t.tolist()/1e9)
        day_of_year = current_date.timetuple().tm_yday
        sol_dec = fao.sol_dec(day_of_year)             # Solar declination
        ird = fao.inv_rel_dist_earth_sun(day_of_year)

        for i, l in enumerate(lat_matrix):
            sha = fao.sunset_hour_angle(l, sol_dec)
            et_rad = fao.et_rad(l, sol_dec, sha, ird)  # Extraterrestrial radiation
            eto[d][i] = fao.hargreaves(d_min_temp[d][i], d_max_temp[d][i], d_mean_temp[d][i], et_rad)

    ds.close()

    return eto, lon_nc, lat_nc, time

def saving_evapotranspiration_netcdf(eto, xs, ys, times, CRS, dem_CRS, output_filename, output_filename2, exact_coord):

    print ('Saving to', output_filename, CRS)

    ds = xa.DataArray(eto,
                      name='pet',
                      dims=['time', 'y', 'x'],
                      coords={
                          "x": xs,
                          "y": ys,
                          "time": times})
    ds.rio.write_crs(CRS, inplace=True)
    ds.to_netcdf(output_filename)
    print_info(verbose, output_filename, ds)

    # Reproject to the DEM CRS, in meters.
    ds_proj_crs = ds.rio.reproject(dem_CRS, Resampling=rasterio.enums.Resampling.bilinear)
    # Unfortunately, the projection in one way then another results in slight differences, so we need to assign the original corrdinates.
    ref = xa.open_dataset(exact_coord)
    assert(np.max(ds_proj_crs['x'] - ref.variables['x'][:]) < 0.000000001) # Assert the differences are minimal.
    ds_proj_crs['x'] = ref.variables['x'][:]
    ds_proj_crs['y'] = ref.variables['y'][:]

    ds_proj_crs.to_netcdf(output_filename2)

    ds.close()


verbose = False
path = '/home/anne-laure/Documents/Datasets/'
results = f'{path}Outputs/'
input_data = "CH2018" # Choose between "CH2018", "CrespiEtAl", "MeteoSwiss"

if input_data == "CH2018":
    exact_coord = f'{results}CH2018_tas_TestOut.nc'
    filename_mean = f'{results}CH2018_tas_TestOut_epsg4326.nc'
    filename_min = f'{results}CH2018_tasmin_TestOut_epsg4326.nc'
    filename_max = f'{results}CH2018_tasmax_TestOut_epsg4326.nc'
    varname_mean = 'tas'
    varname_min = 'tasmin'
    varname_max = 'tasmax'
    output_filename = f'{results}CH2018_pet_TestOut_epsg4326.nc'
    output_filename2 = f'{results}CH2018_pet_TestOut.nc'

elif input_data == "MeteoSwiss":
    filename_mean = f'{path}Swiss_Past_MeteoSwiss/TabsD_ch01r.swiss.lv95_202009280000_202009290000.nc' # RhiresD_ch02h.lonlat_200508210000_200508230000.nc'
    filename_min = f'{path}Swiss_Past_MeteoSwiss/TnormM9120_ch01r.swiss.lv95_000001010000_000012010000.nc'
    filename_max = f'{path}Swiss_Past_MeteoSwiss/TnormM9120_ch01r.swiss.lv95_000001010000_000012010000.nc'

    output_filename = f'{results}ETOHargreavesFrom{input_data}.nc'

# SLOW: change to matrix computations
eto, xs, ys, times = compute_evapotranspiration(filename_mean, varname_mean,
                                                filename_min, varname_min,
                                                filename_max, varname_max, verbose)
eto_CRS = 'EPSG:4326'
DEM_CRS = 'EPSG:21781'
saving_evapotranspiration_netcdf(eto, xs, ys, times, eto_CRS, DEM_CRS, output_filename, output_filename2, exact_coord)



