from datetime import datetime

import netCDF4 as nc
import numpy as np
import pandas as pd
import rasterio
import rioxarray
import xarray as xa

# Homogenize my files (reproject and clip to the study area TIF CRS and extent)
# Warning: we do not homogenize the time format! Should I?
# The datasets do not have the same spatial resolution: Crespi: 250m; CH2018: 1612m; MeteoSwiss: 776m.

def interpolate(nc_filename, regrid_x, regrid_y, out_filename):
    # Opens an existing netCDF file
    print(len(regrid_x), len(regrid_y))

    da_input = xa.open_dataset(nc_filename) # the file the data will be loaded from
    dataset = da_input.interp(x=regrid_x, y=regrid_y) # specify calculation
    dataset.to_netcdf(out_filename) # save direct to file

# Doing da_input.load(), or da_output.compute(), for example, would cause all the data to be loaded into memory - which you want to avoid.


def assign_correct_EPSG(in_filename, in_CRS, out_filename):
    study_area = xa.open_dataset(in_filename)
    if study_area.rio.crs == in_CRS:
        print("CRS is indeed", study_area.rio.crs)
    study_area.rio.write_crs(in_CRS, inplace=True)
    del study_area.x.encoding["contiguous"] # Workaround to a weird bug that happens with Crespi's dataset (cf. https://github.com/pydata/xarray/issues/1849)
    del study_area.y.encoding["contiguous"]
    vars_list = list(study_area.data_vars)
    for var in vars_list:
        del study_area[var].attrs['grid_mapping']
    #study_area.load().to_netcdf(out_filename)


def create_elevation_mask(tif_filename, tif_CRS, nc_filename, target_CRS, reproj_filename, reproj_resamp_filename):
    study_area = xa.open_dataset(tif_filename)
    assign_EPSG_without_saving(study_area, tif_CRS)

    nc = xa.open_dataset(nc_filename, decode_coords="all")
    assign_EPSG_without_saving(nc, target_CRS)

    # Create a TIF to the NetCDF projection, and save it separately
    resam = study_area.rio.reproject(target_CRS, Resampling=rasterio.enums.Resampling.bilinear)
    resam.to_netcdf(f'{reproj_filename}.nc')
    resam['band_data'].rio.to_raster(f'{reproj_filename}.tif')

    # Create a TIF resampled to the NetCDF resolution and projection, and save it separately
    resam = study_area.rio.reproject_match(nc, Resampling=rasterio.enums.Resampling.bilinear)
    resam.to_netcdf(f'{reproj_resamp_filename}.nc')
    resam['band_data'].rio.to_raster(f'{reproj_resamp_filename}.tif')


def resample_netcdf_to_tif_resolution(tif_filename, tif_CRS, nc_filename, target_CRS, reproj_resamp_filename):
    study_area = xa.open_dataset(tif_filename)
    assign_EPSG_without_saving(study_area, tif_CRS)

    var = xa.open_dataset(nc_filename, decode_coords="all")
    assign_EPSG_without_saving(var, target_CRS)

    # Homogenize the datasets
    column_time = 'time'

    band_nb = 3
    times = len(var.variables[column_time][:])

    elevation_thrs = [1000, 1300, 1600, 2000]

    all_topo_masks = []
    for i, elev_min in enumerate(elevation_thrs[:-1]):
        elev_max = elevation_thrs[i+1]
        topo_mask = xa.where((study_area > elev_min) & (study_area < elev_max), 1, 0) # Elevation band set to 1, rest to 0.
        all_topo_masks.append(topo_mask)

    assert len(elevation_thrs) == band_nb + 1
    data = np.zeros((band_nb, times))
    for j, time in enumerate(var.variables[column_time][:]):
        print(time)
        var_uniq = var['pr'][j].rio.reproject_match(study_area, Resampling=rasterio.enums.Resampling.bilinear)
        # Create a NetCDF resampled to the TIF resolution and projection, and save it separately
        #resam = nc.rio.reproject_match(study_area, Resampling=rasterio.enums.Resampling.bilinear)
        for i, elev_min in enumerate(elevation_thrs[:-1]):
            #elev_max = elevation_thrs[i+1]
            #topo_mask = xa.where((study_area > elev_min) & (study_area < elev_max), 1, 0) # Elevation band set to 1, rest to 0.

            val = xa.where(all_topo_masks[i], var_uniq, np.nan).to_array() # Mask the meteorological data with the current elevation band.
            data[i][j] = np.nanmean(val) # Mean the meteorological data in the band.
            # We do not yet interpolate the holes.

    print(data)
    #resam.to_netcdf(f'{reproj_resamp_filename}.nc')

def assign_EPSG_without_saving(dataset, CRS):
    if dataset.rio.crs == CRS:
        print("CRS is indeed", dataset.rio.crs)
    else:
        print("CRS was", dataset.rio.crs, ", is now", CRS)
        dataset.rio.write_crs(CRS, inplace=True)

def reproject_netcdf_to_new_crs_and_save(nc_filename, nc_CRS, proj_CRS, out_filename):
    # Once the NetCDF file is clipped and small enough, we can reproject it.
    nc = xa.open_dataset(nc_filename, decode_coords="all")
    assign_EPSG_without_saving(nc, nc_CRS)

    # Reproject to the new CRS. I tried to use the reproject_match() function but it takes too much memory.
    nc_in_proj_crs = nc.rio.reproject(proj_CRS, Resampling=rasterio.enums.Resampling.bilinear)

    # Save the reprojected NetCDF to the proj CRS.
    nc_in_proj_crs.to_netcdf(out_filename)

def homogenize_the_time(nc_filename, start_datetime, ref_start_datetime):

    if ref_start_datetime != start_datetime:
        ds = nc.Dataset(nc_filename, 'a')
        diff_days = start_datetime - ref_start_datetime
        ds['time'][:] += diff_days.days
        ds.close() # file saved to disk

def reproject_tif_and_clip_netcdf_to_tif_extent(tif_filename, tif_CRS, nc_filename, nc_CRS,
                                                start_datetime, ref_start_datetime, out_filename, xy_lonlat_EN):
    study_area = xa.open_dataset(tif_filename)
    assign_EPSG_without_saving(study_area, tif_CRS)

    # Reproject the TIF to the NetCDF CRS, because the other way round is too RAM-expensive.
    study_area_in_nc_crs = study_area.rio.reproject(nc_CRS, Resampling=rasterio.enums.Resampling.bilinear)
    regrid_y = study_area_in_nc_crs.variables['y'][:]
    regrid_x = study_area_in_nc_crs.variables['x'][:]
    min_x = float(np.min(regrid_x))
    max_x = float(np.max(regrid_x))
    min_y = float(np.min(regrid_y))
    max_y = float(np.max(regrid_y))

    ncf = xa.open_dataset(nc_filename, decode_coords="all")
    assign_EPSG_without_saving(ncf, nc_CRS)

    # Homogenize the dataset variables (names and numbers)
    if xy_lonlat_EN == "xy":
        del ncf.x.encoding["contiguous"] # Workaround to a weird bug that happens with Crespi's dataset (cf. https://github.com/pydata/xarray/issues/1849)
        del ncf.y.encoding["contiguous"]
        ncf = ncf.rename({'DATE': 'time'})
    elif xy_lonlat_EN == "lonlat":
        ncf = ncf.rename({'lon': 'x', 'lat': 'y'})
    elif xy_lonlat_EN == "EN":
        ncf = ncf.rename({'E': 'x', 'N': 'y'})
        ncf = ncf.drop_vars(['lat', 'lon'])
    elif xy_lonlat_EN == "chxchy":
        ncf = ncf.rename({'chx': 'x', 'chy': 'y'})
        ncf = ncf.drop_vars(['dummy', 'lat', 'lon'])
    else:
        print("xy_or_lonlat should be either set to 'xy', 'lonlat' or 'EN'. Here it is set to", xy_lonlat_EN)

    # Clip the NetCDF to the TIF extent
    subset = ncf.sel(y=slice(min_y, max_y), x=slice(min_x, max_x))

    # Save the NetCDF clipped to the TIF study area in the NetCDF CRS.
    subset.to_netcdf(out_filename)

    homogenize_the_time(out_filename, start_datetime, ref_start_datetime)

    #interpolate(out_filename, regrid_x, regrid_y, f'/home/anne-laure/Documents/Datasets/Outputs/CrespiTestOut222.nc')


def main():

    path = '/home/anne-laure/Documents/Datasets/'
    results = f'{path}Outputs/'
    target_eto_CRS = "EPSG:4326" # Needs to be in degrees to compute the evapotranspiration
    target_dem_CRS = "EPSG:21781" # BUT NEEDS TO BE IN METERS TO COMPUTE AREAS FOR HYDROBRICKS
    target_start_datetime = datetime(1900,1,1,0,0,0)


    ##################################################################################################################################
    tif_filename = f'{path}Swiss_Study_area/ValDAnniviers_EPSG21781.tif'
    reproj_filename = f'{results}Swiss_reproj'
    reproj_resamp_filename = f'{results}Swiss_reproj_resamp'
    tif_CRS = "EPSG:21781" # Christian Kühni confirmed (email) that the DHM25 dataset is in the CRS LV03/CH1903 (EPSG:21781). SURE.
    # Check this page for the CRS, because the doc PDF is wrong!!! -> https://www.swisstopo.admin.ch/de/geodata/height/dhm25.html#technische_details
    start_datetime = datetime(1900,1,1,0,0,0)
    # CH2018 gdalinfo: time#units=days since 1900-1-1 00:00:00
    # Crespi gdalinfo: DATE#long_name=days since 1970-01-01 DATE#units=days since 1970-01-01 00:00:00 +00:00
    # TabsD (MeteoSwiss) gdalinfo: time#units=days since 1900-01-01 00:00:00

    ##################################################################################################################################

    #nc_filename = f'{path}Swiss_Past_MeteoSwiss/TEST_RhiresD_ch01h.swiss.lv95_200508210000_200508230000.nc'
    #nc_CRS = "EPSG:2056"  # CH1903+ convention introduced with LV95, EPSG:2056 (?)
    #clipped_filename = f'{results}MeteoSwissTestClipped.nc'
    #out_filename = f'{results}MeteoSwissTestOut.nc'
    #reproject_tif_and_clip_netcdf_to_tif_extent(tif_filename, tif_CRS, nc_filename, nc_CRS, start_datetime, target_start_datetime, clipped_filename, xy_lonlat_EN='EN')
    #reproject_netcdf_to_new_crs_and_save(clipped_filename, nc_CRS, target_CRS, out_filename)

    nc_filename = f'{path}Swiss_Past_MeteoSwiss/RhiresD_1961_2017_ch01r.swisscors/RhiresD_ch01r.swisscors_196101010000_196112310000.nc'
    nc_CRS = "EPSG:21781"  # CH1903+ convention introduced with LV95, EPSG:2056 (?) # NOT WORKING
    clipped_filename = f'{results}MeteoSwissClipped.nc'
    out_filename = f'{results}MeteoSwissOut.nc'
    reproject_tif_and_clip_netcdf_to_tif_extent(tif_filename, tif_CRS, nc_filename, nc_CRS, start_datetime, target_start_datetime, clipped_filename, xy_lonlat_EN='chxchy')
    reproject_netcdf_to_new_crs_and_save(clipped_filename, nc_CRS, target_dem_CRS, out_filename)

    ##################################################################################################################################

    nc_filename = f'{path}Swiss_Future_CH2018/CH2018_tasmin_SMHI-RCA_ECEARTH_EUR11_RCP26_QMgrid_1981-2099.nc'
    nc_CRS = "EPSG:4150" # CRS for the CH2018 is the CH1903+ EPSG:4150 -> cf CH2018_documentation_localized_v1.2.pdf
    # (Or the CH1903+ convention introduced with LV95?? EPSG:2056?? -> but does not seem to work)
    clipped_filename = f'{results}CH2018_tasmin_TestClipped.nc'
    out_filename = f'{results}CH2018_tasmin_TestOut_epsg4326.nc'
    reproject_tif_and_clip_netcdf_to_tif_extent(tif_filename, tif_CRS, nc_filename, nc_CRS, start_datetime, target_start_datetime, clipped_filename, xy_lonlat_EN='lonlat')
    reproject_netcdf_to_new_crs_and_save(clipped_filename, nc_CRS, target_eto_CRS, out_filename)
    #
    nc_filename = f'{path}Swiss_Future_CH2018/CH2018_tasmax_SMHI-RCA_ECEARTH_EUR11_RCP26_QMgrid_1981-2099.nc'
    clipped_filename = f'{results}CH2018_tasmax_TestClipped.nc'
    out_filename = f'{results}CH2018_tasmax_TestOut_epsg4326.nc'
    reproject_tif_and_clip_netcdf_to_tif_extent(tif_filename, tif_CRS, nc_filename, nc_CRS, start_datetime, target_start_datetime, clipped_filename, xy_lonlat_EN='lonlat')
    reproject_netcdf_to_new_crs_and_save(clipped_filename, nc_CRS, target_eto_CRS, out_filename)
    #
    nc_filename = f'{path}Swiss_Future_CH2018/CH2018_tas_SMHI-RCA_ECEARTH_EUR11_RCP26_QMgrid_1981-2099.nc'
    clipped_filename = f'{results}CH2018_tas_TestClipped.nc'
    out_filename1 = f'{results}CH2018_tas_TestOut_epsg4326.nc'
    out_filename2 = f'{results}CH2018_tas_TestOut.nc'
    reproject_tif_and_clip_netcdf_to_tif_extent(tif_filename, tif_CRS, nc_filename, nc_CRS, start_datetime, target_start_datetime, clipped_filename, xy_lonlat_EN='lonlat')
    reproject_netcdf_to_new_crs_and_save(clipped_filename, nc_CRS, target_eto_CRS, out_filename1)
    reproject_netcdf_to_new_crs_and_save(clipped_filename, nc_CRS, target_dem_CRS, out_filename2)

    create_elevation_mask(tif_filename, tif_CRS, f'{results}CH2018_tas_TestOut.nc', target_dem_CRS, reproj_filename, reproj_resamp_filename)
    #
    nc_filename = f'{path}Swiss_Future_CH2018/CH2018_pr_SMHI-RCA_ECEARTH_EUR11_RCP26_QMgrid_1981-2099.nc'
    clipped_filename = f'{results}CH2018_pr_TestClipped.nc'
    out_filename = f'{results}CH2018_pr_TestOut.nc'
    reproject_tif_and_clip_netcdf_to_tif_extent(tif_filename, tif_CRS, nc_filename, nc_CRS, start_datetime, target_start_datetime, clipped_filename, xy_lonlat_EN='lonlat')
    reproject_netcdf_to_new_crs_and_save(clipped_filename, nc_CRS, target_dem_CRS, out_filename)

    print('TEST 1')
    reproj_resamp_netcdf = f'{results}CH2018_pr_TestOut_HR'
    resample_netcdf_to_tif_resolution(tif_filename, tif_CRS, out_filename, target_dem_CRS, reproj_resamp_netcdf)
    print('TEST 2')

    ##################################################################################################################################

    tif_filename = f'{path}Italy_Study_area/Sulden_Saldur_EPSG4326.tif'
    tif_CRS = "EPSG:4326" # SURE
    start_datetime = datetime(1970,1,1,0,0,0)

    nc_filename = f'{path}Italy_Past_Crespi-etal_2020_allfiles/DailySeries_1980_2018_MeanTemp.nc'
    nc_CRS = "EPSG:32632" # A. C. confirmed that the data is in WGS 84 / UTM 32N. SURE.
    clipped_filename = f'{results}CrespiTestClipped.nc'
    out_filename = f'{results}CrespiTestOut.nc'
    reproject_tif_and_clip_netcdf_to_tif_extent(tif_filename, tif_CRS, nc_filename, nc_CRS, start_datetime, target_start_datetime, clipped_filename, xy_lonlat_EN='xy')
    reproject_netcdf_to_new_crs_and_save(clipped_filename, nc_CRS, target_dem_CRS, out_filename)

    ##################################################################################################################################



main()
