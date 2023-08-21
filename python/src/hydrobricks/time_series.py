import hydrobricks as hb
import numpy as np
import pandas as pd
from hydrobricks import utils


class TimeSeries:
    """Class for generic time series data"""

    def __init__(self):
        self.time = []
        self.data = []
        self.data_name = []

    def get_dates_as_mjd(self):
        return utils.date_as_mjd(self.time)


class TimeSeries1D(TimeSeries):
    """Class for generic 1D time series data"""

    def __init__(self):
        super().__init__()

    def load_from_csv(self, path, column_time, time_format, content):
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
            date_format=time_format)

        self.time = file_content[column_time]

        for col in content:
            self.data_name.append(col)
            self.data.append(file_content[content[col]].to_numpy())


class TimeSeries2D(TimeSeries):
    """Class for generic 2D time series data"""

    def __init__(self):
        super().__init__()

    def initialize_from_netcdf(self, path, varname, elev_mask_path, elevation_thrs,
                               column_time, time_format, content):
        """
        Initialize time series data from a netcdf file.
        Uses a DEM in netcdf format interpolated to the meteorological
        data resolution as input.
        Does not load the data in memory, as storing all data would be
        too RAM consuming.
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
            List of the threshold elevations defining the beginning and end of the
            elevation bands (hydrological units).
        column_time : str
            Column name containing the time.
        time_format : str
            Format of the time
        content : dict
            Type of data and column name containing the data.
            Example: {'precipitation': 'Precipitation (mm)'}
        """
        var = hb.xr.open_dataset(path)
        topo = hb.xr.open_dataset(elev_mask_path)

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
            elev_max = elevation_thrs[i + 1]
            topo_mask = hb.xr.where((topo > elev_min) & (topo < elev_max), 1,
                                    0)  # Elevation band set to 1, rest to 0.
            # Multiply by the depth of the meteorological time serie.
            topo_mask_3d = hb.xr.concat([topo_mask] * times,
                                        dim=column_time)
            topo_mask_3d = topo_mask_3d.assign_coords(
                time=var.variables[column_time][:])  # Assign the times to these sheets.
            # Mask the meteorological data with the current elevation band.
            val = hb.xr.where(topo_mask_3d, var,
                              np.nan)
            data[i] = hb.xr.DataArray.mean(val, dim=["x", "y"], skipna=True)[
                varname].to_numpy()  # Mean the meteorological data in the band.
            # We do not yet interpolate the holes.

        self.time = column
        for col in content:
            self.data_name.append(col)
            self.data.append(data)

        return var.variables[column_time][0], var.variables[column_time][-1]

    def initialize_from_netcdf_HR(self, path, varname, elev_mask_path, elevation_thrs,
                                  tif_CRS, nc_CRS, column_time, time_format, content):
        """
        Initialize time series data from a netcdf file. Uses a DEM in tif format
        at any resolution.
        The meteorological data is resampled to the DEM resolution using bilinear
        interpolation.
        Does not load the data in memory, as storing all data would be too RAM
        consuming.
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
            List of the threshold elevations defining the beginning and end of
            the elevation bands (hydrological units).
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
        var = hb.xr.open_dataset(path)
        topo = hb.xr.open_dataset(elev_mask_path)
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

        band_nb = len(self.hydro_units['elevation'])
        times = len(var.variables[column_time][:])

        column = pd.Series(var.variables[column_time][:])
        column = column.dt.strftime(time_format)

        assert len(elevation_thrs) == band_nb + 1
        all_topo_masks = []
        for i, elev_min in enumerate(elevation_thrs[:-1]):
            elev_max = elevation_thrs[i + 1]
            topo_mask = hb.xr.where((topo > elev_min) & (topo < elev_max), 1,
                                    0)  # Elevation band set to 1, rest to 0.
            all_topo_masks.append(topo_mask)

        data = np.zeros((band_nb, times))
        for j, time in enumerate(var.variables[column_time][:]):
            print(time)
            var_uniq = var[varname][j].rio.reproject_match(
                topo, Resampling=hb.rasterio.enums.Resampling.bilinear)
            for i, _ in enumerate(elevation_thrs[:-1]):
                # Mask the meteorological data with the current elevation band.
                val = hb.xr.where(all_topo_masks[i], var_uniq,
                                  np.nan).to_array()
                data[i][j] = np.nanmean(
                    val)  # Mean the meteorological data in the band.
                # We do not yet interpolate the holes.

        self.time = column
        for col in content:
            self.data_name.append(col)
            self.data.append(data)

        return var.variables[column_time][0], var.variables[column_time][-1]
