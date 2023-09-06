from datetime import datetime, timedelta

import netCDF4 as nc
import numpy as np
import pandas as pd
import rasterio
import rioxarray
import xarray as xa

# Homogenize my files (reproject and clip to the study area TIF CRS and extent)
# Warning: we do not homogenize the time format! Should I?
# The datasets do not have the same spatial resolution: Crespi: 250m; CH2018: 1612m; MeteoSwiss: 776m.

def homogenize_arolla_discharge_datasets(subcatchment_datasets, catchment_dataset, path, area_txt,
                                         output_file, output_daily_mean_file, output_hydrobricks_file):
    
    ########### The Bertol Inferieur catchment
    
    dateparse = lambda x: datetime.strptime(x, '%d.%m.%Y %H:%M')
    data_BI = pd.io.parsers.read_csv(catchment_dataset, sep=",", decimal=".", skiprows=0, header=0, na_values=np.nan, 
                                     parse_dates=['Horodatage'], date_parser=dateparse, index_col=0)
    data_BI.index.name = 'Date'
    data_BI.columns = ['BIrest'] # There is only the rest of water not taken from the HGDA, VU and BS intakes.
    #data_BI.iloc[:,0] = np.where(data_BI.iloc[:,0] < 0.08, np.nan, data_BI.iloc[:,0])
    
    ############ Then the subcatchments
    
    years_pd = pd.io.parsers.read_csv(subcatchment_datasets['Year'], sep=",", decimal=".", skiprows=0, header=None, na_values=np.nan)
    years = list(years_pd.values[0])
    
    merged_all = data_BI
    for key, subcatchment_dataset in subcatchment_datasets.items():
        if key != 'Year':
            df = pd.read_csv(subcatchment_dataset, sep=",", decimal=".", skiprows=0, header=None)
            df.to_csv(subcatchment_dataset + '_with_index.csv')
            
            dfs = []
            for i, year in enumerate(years):
                start_datetime = datetime(year,1,1,0,0,0)
                dateparse = lambda x: start_datetime + timedelta(minutes=int(x) * 15)
                yr_data = pd.io.parsers.read_csv(subcatchment_dataset + '_with_index.csv', sep=",", decimal=".", skiprows=1, names=['Date', key],
                                              na_values=np.nan, usecols=[0, i+1], index_col=None, parse_dates=[0], date_parser=dateparse)
                dfs.append(yr_data)

            data = pd.concat(dfs, ignore_index=True)
            data.set_index('Date', inplace=True)
            #data.iloc[:,0] = np.where(data.iloc[:,0] < 0.08, np.nan, data.iloc[:,0])
            
            ############ Merge all datasets but make sure to discard the dates of BI that are not contained in the others (31 Decembers of bissextile years)
            BI_but_not_key = data_BI.loc[data_BI.index.difference(data.index)]
            BI_but_not_key.to_csv(output_file + 'in_BIrest_but_not_in_' + key + '.csv')
            print ('Data present in BIrest, but not in ' + key, BI_but_not_key)
            key_but_not_BI = data.loc[data.index.difference(data_BI.index)]
            key_but_not_BI.to_csv(output_file + 'in_' + key + '_but_not_in_BIrest.csv')
            print('Data present in ' + key + ', but not in BIrest', key_but_not_BI)
            
            intersection_dates = merged_all.index.intersection(data.index)
            
            # Only select the data with the common dateindexes
            merged_all = merged_all.loc[intersection_dates]
            data = data.loc[intersection_dates]
            
            # Add all the data to the same dataframe
            merged_all = merged_all.merge(data, left_index=True, right_index=True, how='inner')
    
    ############ Correct the values of Vuibe (VU) for the late summer of 2011 (will be transmitted to BI as well...)
    merged_all["VU"].loc['2011-08-26':'2011-12-31'] = np.nan
    
    ############ Create a new column with BI plus the water coming from BS, HGDA or VU
    merged_all["BI"] = merged_all["BIrest"] + merged_all["BS"] + merged_all["HGDA"] + merged_all["VU"]
    print('merged_all', merged_all)
    
    ############ Then the daily mean of all discharge measurements
    means = merged_all.groupby(pd.Grouper(freq='1D')).mean()
    print('means_all', means)
    
    for key in merged_all:
        suffix = '_' + key + '.csv'
        
        merged_all.to_csv(output_file + suffix, columns=[key])
        means.to_csv(output_daily_mean_file + suffix, columns=[key], date_format='%d/%m/%Y')
    
        if key != "BIrest":
            filename = path + key + area_txt
            watershed_area = np.loadtxt(filename, skiprows=1)
            # Create new pandas DataFrame.
            subdataframe = means[[key]]
            m_to_mm = 1000
            persec_to_perday = 86400
            subdataframe = subdataframe / watershed_area * m_to_mm * persec_to_perday
            subdataframe.columns = ['Discharge (mm/d)']
            subdataframe.to_csv(output_hydrobricks_file + suffix, date_format='%d/%m/%Y')
    
def homogenize_stelvio_discharge_datasets(stelvio_dataset, path, area_txt,
                                          output_file, output_daily_mean_file, output_hydrobricks_file):
    
    dateparse = lambda x: datetime.strptime(x, '%d/%m/%Y %H:%M:%S')
    data = pd.io.parsers.read_csv(stelvio_dataset, sep=";", decimal=",", encoding='cp1252', skiprows=15, header=1, 
                                  na_values='---', parse_dates={'date': ['Date', 'Time']}, date_parser=dateparse,
                                  index_col=0)
    data.index.name = "Date" # Since cannot write directly parse_dates={'Date': ['Date', 'Time']} in the line above.
    key = 'PS'
    suffix = '_' + key + '.csv'
    
    means = data.groupby(pd.Grouper(freq='1D')).mean()
    data.to_csv(output_file + suffix)
    means.to_csv(output_daily_mean_file + suffix, date_format='%d/%m/%Y')
    filename = path + key + area_txt
    watershed_area = np.loadtxt(filename, skiprows=1)
    m_to_mm = 1000
    persec_to_perday = 86400
    subdataframe = means['Value [m³/s]'] / watershed_area * m_to_mm * persec_to_perday
    subdataframe.name = 'Discharge (mm/d)'
    subdataframe.to_csv(output_hydrobricks_file + suffix, date_format='%d/%m/%Y')
    
def homogenize_trafoi_discharge_datasets(trafoi_dataset, path, area_txt,
                                         output_file, output_daily_mean_file, output_hydrobricks_file):
    
    dateparse = lambda x: datetime.strptime(x, '%m/%d/%Y %H:%M')
    data = pd.io.parsers.read_csv(trafoi_dataset, sep=",", decimal=".", skiprows=0, header=0, 
                                  na_values='NaN', parse_dates=['date_DS5_Trafoi_solar'], date_parser=dateparse,
                                  index_col=0)
    key = 'TF'
    suffix = '_' + key + '.csv'
    
    means = data.groupby(pd.Grouper(freq='1D')).mean()
    data.to_csv(output_file + suffix)
    means.to_csv(output_daily_mean_file + suffix, date_format='%d/%m/%Y')
    filename = path + key + area_txt
    watershed_area = np.loadtxt(filename, skiprows=1)
    m_to_mm = 1000
    persec_to_perday = 86400
    subdataframe = means['discharge_Trafoi_calculated_m3s'] / watershed_area * m_to_mm * persec_to_perday
    subdataframe.name = 'Discharge (mm/d)'
    subdataframe.to_csv(output_hydrobricks_file + suffix, date_format='%d/%m/%Y')
    
def homogenize_solda_gf_discharge_datasets(solda_gf_dataset, path, area_txt,
                                           output_file, output_daily_mean_file, output_hydrobricks_file):
    
    dateparse = lambda x: datetime.strptime(x, '%m/%d/%Y %H:%M')
    data = pd.io.parsers.read_csv(solda_gf_dataset, sep=",", decimal=".", skiprows=0, header=0, 
                                  na_values='NaN', parse_dates=['solar time'], date_parser=dateparse,
                                  index_col=0)
    key = 'SGF'
    suffix = '_' + key + '.csv'
    
    means = data.groupby(pd.Grouper(freq='1D')).mean()
    data.to_csv(output_file + suffix)
    means.to_csv(output_daily_mean_file + suffix, date_format='%d/%m/%Y')
    filename = path + key + area_txt
    watershed_area = np.loadtxt(filename, skiprows=1)
    m_to_mm = 1000
    persec_to_perday = 86400
    subdataframe = means['discharge (m3/s)'] / watershed_area * m_to_mm * persec_to_perday
    subdataframe.name = 'Discharge (mm/d)'
    subdataframe.to_csv(output_hydrobricks_file + suffix, date_format='%d/%m/%Y')
    
def homogenize_zaybach_discharge_datasets(zaybach_dataset, path, area_txt,
                                           output_file, output_daily_mean_file, output_hydrobricks_file):
    
    dateparse = lambda x: datetime.strptime(x, '%m/%d/%Y %H:%M')
    data = pd.io.parsers.read_csv(zaybach_dataset, sep=",", decimal=".", skiprows=0, header=0, 
                                  na_values='', parse_dates=['solar time'], date_parser=dateparse,
                                  index_col=0)
    key = 'ZA'
    suffix = '_' + key + '.csv'
    
    means = data.groupby(pd.Grouper(freq='1D')).mean()
    data.to_csv(output_file + suffix)
    means.to_csv(output_daily_mean_file + suffix, date_format='%d/%m/%Y')
    filename = path + key + area_txt
    watershed_area = np.loadtxt(filename, skiprows=1)
    m_to_mm = 1000
    persec_to_perday = 86400
    subdataframe = means['discharge (m3/s)'] / watershed_area * m_to_mm * persec_to_perday
    subdataframe.name = 'Discharge (mm/d)'
    subdataframe.to_csv(output_hydrobricks_file + suffix, date_format='%d/%m/%Y')

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
    
    print(nc.variables['x'][:])

    # Reproject to the new CRS. I tried to use the reproject_match() function but it takes too much memory.
    nc_in_proj_crs = nc.rio.reproject(proj_CRS, Resampling=rasterio.enums.Resampling.bilinear)

    # Save the reprojected NetCDF to the proj CRS.
    nc_in_proj_crs.to_netcdf(out_filename)

def homogenize_the_time(nc_filename, start_datetime, ref_start_datetime):

    if ref_start_datetime != start_datetime:
        ds = nc.Dataset(nc_filename, 'a')
        diff_days = start_datetime - ref_start_datetime
        ds['time'][:] += diff_days.days
        print('ln', ds['time'].getncattr("long_name"))
        print(ds['time'].getncattr("units"))
        ds['time'].setncattr("long_name", "days since " + ref_start_datetime.strftime('%Y-%m-%d'))
        ds['time'].setncattr("units", "days since " + ref_start_datetime.strftime('%Y-%m-%d'))
        print('ln', ds['time'].getncattr("long_name"))
        print(ds['time'].getncattr("units"))
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
    
    print(ncf)

    # Homogenize the dataset variables (names and numbers)
    if xy_lonlat_EN == "xy":
        del ncf.x.encoding["contiguous"] # Workaround to a weird bug that happens with Crespi's dataset (cf. https://github.com/pydata/xarray/issues/1849)
        del ncf.y.encoding["contiguous"]
        ncf = ncf.rename({'DATE': 'time'})
        if 'tmin' in ncf.data_vars:
            ncf = ncf.rename({'tmin': 'tasmin'})
        elif 'tmax' in ncf.data_vars:
            ncf = ncf.rename({'tmax': 'tasmax'})
        elif 'temperature' in ncf.data_vars:
            ncf = ncf.rename({'temperature': 'tas'})
        elif 'precipitation' in ncf.data_vars:
            ncf = ncf.rename({'precipitation': 'pr'})
    elif xy_lonlat_EN == "lonlat":
        ncf = ncf.rename({'lon': 'x', 'lat': 'y'})
    elif xy_lonlat_EN == "EN":
        ncf = ncf.rename({'E': 'x', 'N': 'y'})
        ncf = ncf.drop_vars(['lat', 'lon'])
    elif xy_lonlat_EN == "chxchy":
        if 'TminD' in ncf.data_vars:
            ncf = ncf.rename({'TminD': 'tasmin'})
        elif 'TmaxD' in ncf.data_vars:
            ncf = ncf.rename({'TmaxD': 'tasmax'})
        elif 'TabsD' in ncf.data_vars:
            ncf = ncf.rename({'TabsD': 'tas'})
        elif 'RhiresD' in ncf.data_vars:
            ncf = ncf.rename({'RhiresD': 'pr'})
        ncf = ncf.rename({'chx': 'x', 'chy': 'y'})
        ncf = ncf.drop_vars(['lat', 'lon'])
    else:
        print("xy_or_lonlat should be either set to 'xy', 'lonlat' or 'EN'. Here it is set to", xy_lonlat_EN)

    # Clip the NetCDF to the TIF extent
    subset = ncf.sel(y=slice(min_y, max_y), x=slice(min_x, max_x))

    # Save the NetCDF clipped to the TIF study area in the NetCDF CRS.
    subset.to_netcdf(out_filename)

    homogenize_the_time(out_filename, start_datetime, ref_start_datetime)

    #interpolate(out_filename, regrid_x, regrid_y, f'/home/anne-laure/Documents/Datasets/Outputs/Crespi_Out222.nc')


def main():
    
    path = '/home/anne-laure/Documents/Datasets/'
    
    results = f'{path}Outputs/'
    target_start_datetime = datetime(1900,1,1,0,0,0)

    ##################################################################################################################################
    
    ########################## Harmonize the Stelvio discharge files #################################################################
    
    stelvio_discharge = f'{path}Italy_discharge/Original_discharge_data/PonteStelvio/Bonfrisco_20211222/07770PG_Suldenbach-Stilfserbrücke_RioSolda-PonteStelvio_Q_10m.Cmd.RunOff.csv'
    trafoi_discharge = f'{path}Italy_discharge/Extracted_discharge_data/data_station_Trafoi_stagePLS_turbidityDS5_all.csv'
    solda_gf_discharge = f'{path}Italy_discharge/Extracted_discharge_data/discharge_Solda_glacierfront_all_2017_2018_2019_2020.csv'
    zaybach_discharge = f'{path}Italy_discharge/Extracted_discharge_data/Discharge_Zaybach2017-18.csv'
    watershed_path = f'{path}Italy_discharge/Watersheds/'
    area_txt = '_UpslopeArea_EPSG25832.txt'
    output_file = f"{results}ObservedDischarges/Solda_discharge"
    output_daily_mean_file = f"{results}ObservedDischarges/Solda_daily_mean_discharge"
    output_hydrobricks_file = f"{results}ObservedDischarges/Solda_hydrobricks_discharge"
    homogenize_stelvio_discharge_datasets(stelvio_discharge, watershed_path, area_txt, output_file, output_daily_mean_file, output_hydrobricks_file)
    homogenize_trafoi_discharge_datasets(trafoi_discharge, watershed_path, area_txt, output_file, output_daily_mean_file, output_hydrobricks_file)
    homogenize_solda_gf_discharge_datasets(solda_gf_discharge, watershed_path, area_txt, output_file, output_daily_mean_file, output_hydrobricks_file)
    homogenize_zaybach_discharge_datasets(zaybach_discharge, watershed_path, area_txt, output_file, output_daily_mean_file, output_hydrobricks_file)
    
    ########################## Harmonize the Arolla discharge files ##################################################################
    
    csv_discharges = '/home/anne-laure/Documents/Datasets/Swiss_discharge/Arolla_discharge/Extracted_discharge_data/'
    subcatchment_discharges = {'BS':f'{csv_discharges}Q_Altroclima_BS.csv', 'DB':f'{csv_discharges}Q_Altroclima_DB.csv', 'HGDA':f'{csv_discharges}Q_Altroclima_HGDA.csv', 
                               'PI':f'{csv_discharges}Q_Altroclima_PI.csv', 'TN':f'{csv_discharges}Q_Altroclima_TN.csv', 'VU':f'{csv_discharges}Q_Altroclima_VU.csv', 
                               'Year':f'{csv_discharges}Q_Altroclima_year.csv'}
    main_catchment_discharge = f'{csv_discharges}Bertol_Inferieur_AllDebit.csv'
    watershed_path = f'{path}Swiss_discharge/Arolla_discharge/Watersheds_on_dhm25/'
    area_txt = '_UpslopeArea_EPSG21781.txt'
    output_file = f"{results}ObservedDischarges/Arolla_15min_discharge"
    output_daily_mean_file = f"{results}ObservedDischarges/Arolla_daily_mean_discharge"
    output_hydrobricks_file = f"{results}ObservedDischarges/Arolla_hydrobricks_discharge"
    homogenize_arolla_discharge_datasets(subcatchment_discharges, main_catchment_discharge, watershed_path, area_txt,
                                         output_file, output_daily_mean_file, output_hydrobricks_file)

    ########################## Harmonize Crespi datasets #############################################################################
    
    #gdalwarp -overwrite -s_srs "PROJCRS["""ETRF_1989_UTM_Zone_32N""",BASEGEOGCRS["""ETRF89""",DATUM["""European Terrestrial Reference Frame 1989""",ELLIPSOID["""GRS 1980""",6378137,298.257223563,LENGTHUNIT["""metre""",1]]],PRIMEM["""Greenwich""",0,ANGLEUNIT["""degree""",0.0174532925199433]],ID["""EPSG""",9059]],CONVERSION["""Transverse Mercator""",METHOD["""Transverse Mercator""",ID["""EPSG""",9807]],PARAMETER["""Latitude of natural origin""",0,ANGLEUNIT["""degree""",0.0174532925199433],ID["""EPSG""",8801]],PARAMETER["""Longitude of natural origin""",9,ANGLEUNIT["""degree""",0.0174532925199433],ID["""EPSG""",8802]],PARAMETER["""Scale factor at natural origin""",0.9996,SCALEUNIT["""unity""",1],ID["""EPSG""",8805]],PARAMETER["""False easting""",500000,LENGTHUNIT["""metre""",1],ID["""EPSG""",8806]],PARAMETER["""False northing""",0,LENGTHUNIT["""metre""",1],ID["""EPSG""",8807]]],CS[Cartesian,2],AXIS["""easting""",east,ORDER[1],LENGTHUNIT["""metre""",1,ID["""EPSG""",9001]]],AXIS["""northing""",north,ORDER[2],LENGTHUNIT["""metre""",1,ID["""EPSG""",9001]]]]" -t_srs "PROJCRS["""ETRF_1989_UTM_Zone_32N""",BASEGEOGCRS["""ETRF89""",DATUM["""European Terrestrial Reference Frame 1989""",ELLIPSOID["""GRS 1980""",6378137,298.257223563,LENGTHUNIT["""metre""",1]]],PRIMEM["""Greenwich""",0,ANGLEUNIT["""degree""",0.0174532925199433]],ID["""EPSG""",9059]],CONVERSION["""Transverse Mercator""",METHOD["""Transverse Mercator""",ID["""EPSG""",9807]],PARAMETER["""Latitude of natural origin""",0,ANGLEUNIT["""degree""",0.0174532925199433],ID["""EPSG""",8801]],PARAMETER["""Longitude of natural origin""",9,ANGLEUNIT["""degree""",0.0174532925199433],ID["""EPSG""",8802]],PARAMETER["""Scale factor at natural origin""",0.9996,SCALEUNIT["""unity""",1],ID["""EPSG""",8805]],PARAMETER["""False easting""",500000,LENGTHUNIT["""metre""",1],ID["""EPSG""",8806]],PARAMETER["""False northing""",0,LENGTHUNIT["""metre""",1],ID["""EPSG""",8807]]],CS[Cartesian,2],AXIS["""easting""",east,ORDER[1],LENGTHUNIT["""metre""",1,ID["""EPSG""",9001]]],AXIS["""northing""",north,ORDER[2],LENGTHUNIT["""metre""",1,ID["""EPSG""",9001]]]]" -tr 25.0 25.0 -r bilinear -of GTiff /home/anne-laure/Documents/Datasets/Italy_Study_area/dtm_2pt5m_utm_st_whole_StudyArea_OriginalCRS.tif /tmp/processing_vFpCqu/eb6aabf4e69346af9f3b7154880bf79f/OUTPUT.tif
    # gdalwarp -overwrite -s_srs EPSG:25832 -t_srs EPSG:25832 -tr 25.0 25.0 -r bilinear -of GTiff 
    # /home/anne-laure/Documents/Datasets/Italy_Study_area/dtm_2pt5m_utm_st_whole_StudyArea_OriginalCRS.tif 
    # /home/anne-laure/Documents/Datasets/Italy_Study_area/dtm_25m_utm_st_whole_StudyArea_OriginalCRS.tif
    
    tif_filename = f'{path}Italy_Study_area/dtm_25m_utm_st_whole_StudyArea_OriginalCRS_filled.tif'
    tif_CRS = "EPSG:25832"
    start_datetime = datetime(1970,1,1,0,0,0)
    # Crespi gdalinfo: DATE#long_name=days since 1970-01-01 DATE#units=days since 1970-01-01 00:00:00 +00:00
    
    nc_CRS = "EPSG:32632" # A. C. confirmed that the data is in WGS 84 / UTM 32N. SURE.
    target_eto_CRS = "EPSG:4326" # Needs to be in degrees to compute the evapotranspiration
    target_dem_CRS = "EPSG:25832" # BUT NEEDS TO BE IN METERS TO COMPUTE AREAS FOR HYDROBRICKS
    reproj_filename = f'{results}Crespi_Italy_reproj'
    reproj_resamp_filename = f'{results}Crespi_Italy_reproj_resamp'
    
    nc_filename = f'{path}Italy_Past_Crespi-etal_2020_allfiles/CATCHMENT_DAILYTMIN_1980_2022.nc'
    clipped_filename = f'{results}Crespi_tasmin_Clipped.nc'
    out_filename_tmp = f'{results}Crespi_tasmin_Out_epsg4326_tmp.nc'
    out_filename = f'{results}Crespi_tasmin_Out_epsg4326.nc'
    reproject_tif_and_clip_netcdf_to_tif_extent(tif_filename, tif_CRS, nc_filename, nc_CRS, start_datetime, target_start_datetime, clipped_filename, xy_lonlat_EN='xy')
    reproject_netcdf_to_new_crs_and_save(clipped_filename, nc_CRS, target_eto_CRS, out_filename_tmp)
    ds = xa.open_dataset(out_filename_tmp)
    sliced = ds.sel(time=slice('1980-01-01', '2018-12-31')) # Slice to the time extent of min and max for pet computations...
    sliced.to_netcdf(out_filename)

    nc_filename = f'{path}Italy_Past_Crespi-etal_2020_allfiles/CATCHMENT_DAILYTMAX_1980_2022.nc'
    clipped_filename = f'{results}Crespi_tasmax_Clipped.nc'
    out_filename_tmp = f'{results}Crespi_tasmax_Out_epsg4326_tmp.nc'
    out_filename = f'{results}Crespi_tasmax_Out_epsg4326.nc'
    reproject_tif_and_clip_netcdf_to_tif_extent(tif_filename, tif_CRS, nc_filename, nc_CRS, start_datetime, target_start_datetime, clipped_filename, xy_lonlat_EN='xy')
    reproject_netcdf_to_new_crs_and_save(clipped_filename, nc_CRS, target_eto_CRS, out_filename_tmp)
    ds = xa.open_dataset(out_filename_tmp)
    sliced = ds.sel(time=slice('1980-01-01', '2018-12-31')) # Slice to the time extent of min and max for pet computations...
    sliced.to_netcdf(out_filename)

    nc_filename = f'{path}Italy_Past_Crespi-etal_2020_allfiles/DailySeries_1980_2018_MeanTemp.nc'
    clipped_filename = f'{results}Crespi_tas_Clipped.nc'
    out_filename1 = f'{results}Crespi_tas_Out_epsg4326.nc'
    out_filename2 = f'{results}Crespi_tas_Out.nc'
    reproject_tif_and_clip_netcdf_to_tif_extent(tif_filename, tif_CRS, nc_filename, nc_CRS, start_datetime, target_start_datetime, clipped_filename, xy_lonlat_EN='xy')
    reproject_netcdf_to_new_crs_and_save(clipped_filename, nc_CRS, target_eto_CRS, out_filename1)
    reproject_netcdf_to_new_crs_and_save(out_filename1, target_eto_CRS, target_dem_CRS, out_filename2)

    create_elevation_mask(tif_filename, tif_CRS, out_filename2, target_dem_CRS, reproj_filename, reproj_resamp_filename)

    nc_filename = f'{path}Italy_Past_Crespi-etal_2020_allfiles/DailySeries_1980_2018_Prec.nc'
    clipped_filename = f'{results}Crespi_pr_Clipped.nc'
    out_filename1 = f'{results}Crespi_pr_Out_epsg4326.nc'
    out_filename2 = f'{results}Crespi_pr_Out.nc'
    reproject_tif_and_clip_netcdf_to_tif_extent(tif_filename, tif_CRS, nc_filename, nc_CRS, start_datetime, target_start_datetime, clipped_filename, xy_lonlat_EN='xy')
    reproject_netcdf_to_new_crs_and_save(clipped_filename, nc_CRS, target_eto_CRS, out_filename1)
    reproject_netcdf_to_new_crs_and_save(out_filename1, target_eto_CRS, target_dem_CRS, out_filename2)
    print(bvhua)

    ########################## Harmonize MeteoSwiss datasets #########################################################################

    #nc_filename = f'{path}Swiss_Past_MeteoSwiss/TEST_RhiresD_ch01h.swiss.lv95_200508210000_200508230000.nc'
    #nc_CRS = "EPSG:2056"  # CH1903+ convention introduced with LV95, EPSG:2056 (?)
    #clipped_filename = f'{results}MeteoSwiss_Clipped.nc'
    #out_filename = f'{results}MeteoSwiss_Out.nc'
    #reproject_tif_and_clip_netcdf_to_tif_extent(tif_filename, tif_CRS, nc_filename, nc_CRS, start_datetime, target_start_datetime, clipped_filename, xy_lonlat_EN='EN')
    #reproject_netcdf_to_new_crs_and_save(clipped_filename, nc_CRS, target_CRS, out_filename)

    # NEED TO MERGE FIRST THE NETCDF FILES WITH:
    # cdo mergetime RhiresD_ch01r.swisscors_????01010000_????12310000.nc RhiresD_ch01r.swisscors_all.nc

    tif_filename = f'{path}Swiss_Study_area/StudyAreas_EPSG21781.tif'
    tif_CRS = "EPSG:21781" # Christian Kühni confirmed (email) that the DHM25 dataset is in the CRS LV03/CH1903 (EPSG:21781). SURE.
    # Check this page for the CRS, because the doc PDF is wrong!!! -> https://www.swisstopo.admin.ch/de/geodata/height/dhm25.html#technische_details
    start_datetime = datetime(1900,1,1,0,0,0)
    # CH2018 gdalinfo: time#units=days since 1900-1-1 00:00:00
    # TabsD (MeteoSwiss) gdalinfo: time#units=days since 1900-01-01 00:00:00
    nc_CRS = "EPSG:21781"  # CH1903+ convention introduced with LV95, EPSG:2056 (?) # NOT WORKING
    target_eto_CRS = "EPSG:4326" # Needs to be in degrees to compute the evapotranspiration
    target_dem_CRS = "EPSG:21781" # BUT NEEDS TO BE IN METERS TO COMPUTE AREAS FOR HYDROBRICKS
    reproj_filename = f'{results}MeteoSwiss_Swiss_reproj'
    reproj_resamp_filename = f'{results}MeteoSwiss_Swiss_reproj_resamp'
    
    nc_filename = f'{path}Swiss_Past_MeteoSwiss/OldVersion/TminD_1971_2017_ch01r.swisscors/TminD_ch01r.swisscors_all.nc'
    clipped_filename = f'{results}MeteoSwiss_tasmin_Clipped.nc'
    out_filename = f'{results}MeteoSwiss_tasmin_Out_epsg4326.nc'
    reproject_tif_and_clip_netcdf_to_tif_extent(tif_filename, tif_CRS, nc_filename, nc_CRS, start_datetime, target_start_datetime, clipped_filename, xy_lonlat_EN='chxchy')
    reproject_netcdf_to_new_crs_and_save(clipped_filename, nc_CRS, target_eto_CRS, out_filename)

    nc_filename = f'{path}Swiss_Past_MeteoSwiss/OldVersion/TmaxD_1971_2017_ch01r.swisscors/TmaxD_ch01r.swisscors_all.nc'
    clipped_filename = f'{results}MeteoSwiss_tasmax_Clipped.nc'
    out_filename = f'{results}MeteoSwiss_tasmax_Out_epsg4326.nc'
    reproject_tif_and_clip_netcdf_to_tif_extent(tif_filename, tif_CRS, nc_filename, nc_CRS, start_datetime, target_start_datetime, clipped_filename, xy_lonlat_EN='chxchy')
    reproject_netcdf_to_new_crs_and_save(clipped_filename, nc_CRS, target_eto_CRS, out_filename)

    nc_filename = f'{path}Swiss_Past_MeteoSwiss/OldVersion/TabsD_1961_2017_ch01r.swisscors/TabsD_ch01r.swisscors_all.nc'
    clipped_filename_tmp = f'{results}MeteoSwiss_tas_Clipped_temp.nc'
    clipped_filename = f'{results}MeteoSwiss_tas_Clipped.nc'
    out_filename1 = f'{results}MeteoSwiss_tas_Out_epsg4326.nc'
    out_filename2 = f'{results}MeteoSwiss_tas_Out.nc'
    reproject_tif_and_clip_netcdf_to_tif_extent(tif_filename, tif_CRS, nc_filename, nc_CRS, start_datetime, target_start_datetime, clipped_filename_tmp, xy_lonlat_EN='chxchy')
    # Slice to the time extent of min and max for pet computations, and fill the missing year...
    ds = xa.open_dataset(clipped_filename_tmp)
    full_time_index = pd.date_range(start=ds.time.values[0], end=ds.time.values[-1], freq="D")
    ds = ds.reindex({"time": full_time_index}, fill_value=0)
    sliced = ds.sel(time=slice('1971-01-01', '2017-12-31'))
    sliced.to_netcdf(clipped_filename)
    reproject_netcdf_to_new_crs_and_save(clipped_filename, nc_CRS, target_eto_CRS, out_filename1)
    reproject_netcdf_to_new_crs_and_save(clipped_filename, nc_CRS, target_dem_CRS, out_filename2)

    create_elevation_mask(tif_filename, tif_CRS, out_filename2, target_dem_CRS, reproj_filename, reproj_resamp_filename)

    nc_filename = f'{path}Swiss_Past_MeteoSwiss/OldVersion/RhiresD_1961_2017_ch01r.swisscors/RhiresD_ch01r.swisscors_all.nc'
    clipped_filename_tmp = f'{results}MeteoSwiss_pr_Clipped_tmp.nc'
    clipped_filename = f'{results}MeteoSwiss_pr_Clipped.nc'
    out_filename = f'{results}MeteoSwiss_pr_Out.nc'
    reproject_tif_and_clip_netcdf_to_tif_extent(tif_filename, tif_CRS, nc_filename, nc_CRS, start_datetime, target_start_datetime, clipped_filename_tmp, xy_lonlat_EN='chxchy')
    # Slice to the time extent of min and max.
    ds = xa.open_dataset(clipped_filename_tmp)
    sliced = ds.sel(time=slice('1971-01-01', '2017-12-31'))
    sliced.to_netcdf(clipped_filename)
    reproject_netcdf_to_new_crs_and_save(clipped_filename, nc_CRS, target_dem_CRS, out_filename)

    ########################## Harmonize CH2018 datasets #############################################################################
    
    nc_CRS = "EPSG:4150" # CRS for the CH2018 is the CH1903+ EPSG:4150 -> cf CH2018_documentation_localized_v1.2.pdf
    # (Or the CH1903+ convention introduced with LV95?? EPSG:2056?? -> but does not seem to work)
    reproj_filename = f'{results}CH2018_Swiss_reproj'
    reproj_resamp_filename = f'{results}CH2018_Swiss_reproj_resamp'

    nc_filename = f'{path}Swiss_Future_CH2018/CH2018_tasmin_SMHI-RCA_ECEARTH_EUR11_RCP26_QMgrid_1981-2099.nc'
    clipped_filename = f'{results}CH2018_tasmin_Clipped.nc'
    out_filename = f'{results}CH2018_tasmin_Out_epsg4326.nc'
    reproject_tif_and_clip_netcdf_to_tif_extent(tif_filename, tif_CRS, nc_filename, nc_CRS, start_datetime, target_start_datetime, clipped_filename, xy_lonlat_EN='lonlat')
    reproject_netcdf_to_new_crs_and_save(clipped_filename, nc_CRS, target_eto_CRS, out_filename)
    #
    nc_filename = f'{path}Swiss_Future_CH2018/CH2018_tasmax_SMHI-RCA_ECEARTH_EUR11_RCP26_QMgrid_1981-2099.nc'
    clipped_filename = f'{results}CH2018_tasmax_Clipped.nc'
    out_filename = f'{results}CH2018_tasmax_Out_epsg4326.nc'
    reproject_tif_and_clip_netcdf_to_tif_extent(tif_filename, tif_CRS, nc_filename, nc_CRS, start_datetime, target_start_datetime, clipped_filename, xy_lonlat_EN='lonlat')
    reproject_netcdf_to_new_crs_and_save(clipped_filename, nc_CRS, target_eto_CRS, out_filename)
    #
    nc_filename = f'{path}Swiss_Future_CH2018/CH2018_tas_SMHI-RCA_ECEARTH_EUR11_RCP26_QMgrid_1981-2099.nc'
    clipped_filename = f'{results}CH2018_tas_Clipped.nc'
    out_filename1 = f'{results}CH2018_tas_Out_epsg4326.nc'
    out_filename2 = f'{results}CH2018_tas_Out.nc'
    reproject_tif_and_clip_netcdf_to_tif_extent(tif_filename, tif_CRS, nc_filename, nc_CRS, start_datetime, target_start_datetime, clipped_filename, xy_lonlat_EN='lonlat')
    reproject_netcdf_to_new_crs_and_save(clipped_filename, nc_CRS, target_eto_CRS, out_filename1)
    reproject_netcdf_to_new_crs_and_save(clipped_filename, nc_CRS, target_dem_CRS, out_filename2)

    create_elevation_mask(tif_filename, tif_CRS, out_filename2, target_dem_CRS, reproj_filename, reproj_resamp_filename)
    #
    nc_filename = f'{path}Swiss_Future_CH2018/CH2018_pr_SMHI-RCA_ECEARTH_EUR11_RCP26_QMgrid_1981-2099.nc'
    clipped_filename = f'{results}CH2018_pr_Clipped.nc'
    out_filename = f'{results}CH2018_pr_Out.nc'
    reproject_tif_and_clip_netcdf_to_tif_extent(tif_filename, tif_CRS, nc_filename, nc_CRS, start_datetime, target_start_datetime, clipped_filename, xy_lonlat_EN='lonlat')
    reproject_netcdf_to_new_crs_and_save(clipped_filename, nc_CRS, target_dem_CRS, out_filename)

    ##################################################################################################################################



main()
