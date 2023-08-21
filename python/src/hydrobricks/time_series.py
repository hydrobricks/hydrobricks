from datetime import datetime

import geopandas as gpd
import numpy as np
import pandas as pd
import rasterio
import warnings
import xarray as xa
from hydrobricks import utils


class TimeSeries:
    """Class for generic time series data"""

    def __init__(self):
        self.time = []
        self.data_raw = []
        self.data_spatialized = []
        self.data_name = []

    def initialize_from_netcdf(self, path, varname, elev_mask_path, elevation_thrs, column_time, time_format, content):
        """
        Initialize time series data from a netcdf file.
        Uses a DEM in netcdf format interpolated to the meteorological data resolution as input.
        Does not load the data in memory, as storing all data would be too RAM consuming.
        Returns the start and end date of the dataset.

        Parameters
        ----------
        path : str|Path
            Path to the netcdf file containing meteorological data.
        varname : str
            Name of the variable in the netcdf file.
        elev_mask_path : str|Path
            Path to the DEM in netcdf format.
        elevation_thrs : list of float
            List of the threshold elevations defining the beginning and end of the elevation bands (hydrological units).
        column_time : str
            Column name containing the time.
        time_format : str
            Format of the time
        content : dict
            Type of data and column name containing the data.
            Example: {'precipitation': 'Precipitation (mm)'}
        """
        var = xa.open_dataset(path)
        topo = xa.open_dataset(elev_mask_path)

        # Homogenize the datasets
        var = var.drop_vars('spatial_ref')
        topo = topo.drop_vars('spatial_ref')
        topo = topo.rename({'band': 'time'})
        topo = topo.rename({'band_data': varname})

        band_nb = len(self.hydro_units['elevation'])
        times = len(var.variables[column_time][:])

        column = pd.Series(var.variables[column_time][:])
        column = column.dt.strftime(time_format)

        assert len(elevation_thrs) == band_nb + 1
        data = np.zeros((band_nb, times))
        for i, elev_min in enumerate(elevation_thrs[:-1]):
            elev_max = elevation_thrs[i+1]
            topo_mask = xa.where((topo > elev_min) & (topo < elev_max), 1, 0) # Elevation band set to 1, rest to 0.
            topo_mask_3d = xa.concat([topo_mask] * times, dim=column_time) # Multiply by the depth of the meteorological time serie.
            topo_mask_3d = topo_mask_3d.assign_coords(time=var.variables[column_time][:]) # Assign the times to these sheets.

            val = xa.where(topo_mask_3d, var, np.nan) # Mask the meteorological data with the current elevation band.
            data[i] = xa.DataArray.mean(val, dim=["x", "y"], skipna=True)[varname].to_numpy() # Mean the meteorological data in the band.
            # We do not yet interpolate the holes.

        self.time = column
        for col in content:
            self.data_name.append(col)
            self.data_raw.append(data)
            self.data_spatialized.append(None)

        return var.variables[column_time][0], var.variables[column_time][-1]

    def initialize_from_netcdf_HR(self, path, varname, elev_mask_path, outline, outline_epsg, target_epsg, 
                                  elevation_bands, tif_CRS, nc_CRS, column_time, time_format, content):
        """
        Initialize time series data from a netcdf file. Uses a DEM in tif format at any resolution.
        The meteorological data is resampled to the DEM resolution using bilinear interpolation.
        Does not load the data in memory, as storing all data would be too RAM consuming.
        Returns the start and end date of the dataset.

        Parameters
        ----------
        path : str|Path
            Path to the netcdf file containing meteorological data.
        varname : str
            Name of the variable in the netcdf file.
        elev_mask_path : str|Path
            Path to the DEM in tif format.
        elevation_thrs : list of float
            List of the threshold elevations defining the beginning and end of the elevation bands (hydrological units).
        tif_CRS : str
            CRS of the input tif file written in 'EPSG:4326' format.
        nc_CRS : str
            CRS of the input netcdf file written in 'EPSG:4326' format.
        column_time : str
            Column name containing the time.
        time_format : str
            Format of the time
        content : dict
            Type of data and column name containing the data.
            Example: {'precipitation': 'Precipitation (mm)'}
        """
        var = xa.open_dataset(path)
        topo = xa.open_dataset(elev_mask_path)
        if var.rio.crs == nc_CRS:
            print("CRS is indeed", var.rio.crs)
        else:
            print("CRS was", var.rio.crs, ", is now", nc_CRS)
            var.rio.write_crs(nc_CRS, inplace=True)
        if topo.rio.crs == tif_CRS:
            print("CRS is indeed", topo.rio.crs)
        else:
            print("CRS was", topo.rio.crs, ", is now", tif_CRS)
            topo.rio.write_crs(tif_CRS, inplace=True)

        # Homogenize the datasets
        topo = topo.rename({'band': 'time'})
        topo = topo.rename({'band_data': varname})
        
        # Read file using gpd.read_file()
        # Need to correct the EPSG of the outline (wrong metadata)
        # Convert shapefiles to the target CRS
        catchment = gpd.read_file(outline)
        catchment.set_crs(outline_epsg, allow_override=True, inplace=True)
        catchment.to_crs(target_epsg, inplace=True)
        
        # Clip the topographic data to our study catchment to discard outside meteorological values
        topo = topo.rio.clip(catchment.geometry.values, catchment.crs)

        band_nb = len(self.hydro_units['elevation'])
        times = len(var.variables[column_time][:])

        column = pd.Series(var.variables[column_time][:])
        column = column.dt.strftime(time_format)

        thrs_min = elevation_bands.elevation_thrs_min.values
        thrs_max = elevation_bands.elevation_thrs_max.values
        assert len(thrs_min) == band_nb + 1
        all_topo_masks = []
        for i, (elev_min, elev_max) in enumerate(zip(thrs_min, thrs_max)):
            elev_max = elevation_thrs[i+1]
            topo_mask = xa.where((topo > elev_min) & (topo < elev_max), 1, 0) # Elevation band set to 1, rest to 0.
            all_topo_masks.append(topo_mask)

        data = np.zeros((band_nb, times))
        for j, time in enumerate(var.variables[column_time][:]):
            print(varname, time.data)
            var_uniq = var[varname][j].rio.reproject_match(topo, Resampling=rasterio.enums.Resampling.bilinear)
            for i, elev_min in enumerate(elevation_thrs[:-1]):
                val = xa.where(all_topo_masks[i], var_uniq, np.nan).to_array() # Mask the meteorological data with the current elevation band.
                # I expect to see RuntimeWarnings in this block due to elevation bands without any data inside
                with warnings.catch_warnings():
                    warnings.simplefilter("ignore", category=RuntimeWarning)
                    data[i][j] = np.nanmean(val) # Mean the meteorological data in the band.
                    # We do not yet interpolate the holes.

        self.time = column
        for col in content:
            self.data_name.append(col)
            self.data_raw.append(data)
            self.data_spatialized.append(None)

        return var.variables[column_time][0], var.variables[column_time][-1]

    def load_from_csv(self, path, column_time, time_format, content, start_date=None, end_date=None):
        """
        Read time series data from csv file.

        Parameters
        ----------
        path : str|Path
            Path to the csv file containing hydro units data.
        column_time : str
            Column name containing the time.
        time_format : str
            Format of the time
        content : dict
            Type of data and column name containing the data.
            Example: {'precipitation': 'Precipitation (mm)'}
        """
        file_content = pd.read_csv(
            path, parse_dates=[column_time],
            date_parser=lambda x: datetime.strptime(x, time_format))
        
        if start_date and end_date:
            mask = (file_content[column_time] >= start_date) & (file_content[column_time] <= end_date)
            file_content = file_content.loc[mask]

        self.time = file_content[column_time]

        for col in content:
            self.data_name.append(col)
            self.data_raw.append(file_content[content[col]].to_numpy())
            self.data_spatialized.append(None)

    def load_spatialized_data_from_csv(self, column, path, time_path, time_format, dropindex):
        """
        Read already spatialized time series data from csv file.
        Useful for data that needs a long time to be processed with initialize_from_netcdf_HR()
        or initialize_from_netcdf() and can be stored as csv file.

        Parameters
        ----------
        column : str
            Name of the data to be loaded.
        path : str|Path
            Path to the csv file containing hydro units data.
        time_path : dict
            Type of data and column name containing the data.
            Example: {'precipitation': 'Precipitation (mm)'}
        time_format : str
            Format of the time
        """
        data = np.loadtxt(path, delimiter=',')

        # TEMPORARY - DROP WHERE AREA==0
        print('TEMP DEBUG', type(data))
        keepindex = np.delete(list(range(len(data[0]))), dropindex.values)
        print('TEMP DEBUG', keepindex)
        d = np.ndarray((len(data), len(keepindex)))
        for i, day in enumerate(data):
            d[i,:] = day[keepindex]
        # TEMPORARY - DROP WHERE AREA==0 - ENDOF

        file_content = pd.read_csv(time_path, converters={'time': lambda x: pd.to_datetime(x).strptime(time_format)})

        nb_days = len(d)
        assert len(file_content['# time']) == nb_days

        self.time = file_content['# time'] # time2 #file_content[column_time]
        self.data_name.append(column)
        self.data_raw.append(None)
        self.data_spatialized.append(d)

    def _date_as_mjd(self):
        return utils.date_as_mjd(self.time)
