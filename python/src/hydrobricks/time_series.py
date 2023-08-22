import warnings
from pathlib import Path

import numpy as np
import pandas as pd

import hydrobricks as hb
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

    def regrid_from_netcdf(self, path, file_pattern=None, data_crs=None, var_name=None,
                           dim_time='time', dim_x='x', dim_y='y',
                           raster_hydro_units=None):
        """
        Regrid time series data from netcdf files.
        The spatialization is done using a raster of hydro unit ids.
        The meteorological data is resampled to the DEM resolution using bilinear
        interpolation.

        Parameters
        ----------
        path : str|Path
            Path to a netcdf file containing the data or to a folder containing
            multiple files.
        file_pattern : str, optional
            Pattern of the files to read. If None, the path is considered to be
            a single file.
        data_crs : int, optional
            CRS (as EPSG id) of the netcdf file. If None, the CRS is read from the file.
        var_name : str
            Name of the variable to read.
        dim_time : str
            Name of the time dimension.
        dim_x : str
            Name of the x dimension.
        dim_y : str
            Name of the y dimension.
        raster_hydro_units : str|Path
            Path to a raster containing the hydro unit ids to use for the
            spatialization.
        """
        if not hb.has_rasterio:
            raise ImportError("rasterio is required for 'regrid_from_netcdf'.")
        if not hb.has_xarray:
            raise ImportError("rioxarray is required for 'regrid_from_netcdf'.")
        if not hb.has_rioxarray:
            raise ImportError("rioxarray is required for 'regrid_from_netcdf'.")
        if not hb.has_netcdf:
            raise ImportError("netCDF4 is required for 'regrid_from_netcdf'.")

        if raster_hydro_units is None:
            raise ValueError("You must provide a raster of the hydro units.")

        # Get unit ids
        with warnings.catch_warnings():
            warnings.simplefilter("ignore")
            unit_ids = hb.rxr.open_rasterio(raster_hydro_units)
            unit_ids = unit_ids.squeeze().drop_vars("band")

        # Get netCDF dataset
        print(f"Reading netcdf file(s) from {path}...")
        if file_pattern is None:
            nc_data = hb.xr.open_dataset(path)
        else:
            files = sorted(Path(path).glob(file_pattern))
            nc_data = hb.xr.open_mfdataset(files)

        # Get CRS of the netcdf file
        data_crs = self._parse_crs(nc_data, data_crs)

        # Get CRS of the unit ids raster
        unit_ids_crs = self._parse_crs(unit_ids, None)

        if data_crs != unit_ids_crs:
            print("The CRS of the netcdf file does not match the CRS of the "
                  "hydro unit ids raster. Reprojection will be done.")
            with warnings.catch_warnings():
                warnings.simplefilter("ignore")
                unit_ids = unit_ids.rio.reproject(f'epsg:{data_crs}')

        # Get list of hydro unit ids
        unit_ids_list = np.unique(unit_ids)
        unit_ids_list = unit_ids_list[unit_ids_list != 0]
        unit_ids_nb = len(unit_ids_list)

        # Get list of time steps
        time_nc = nc_data.variables[dim_time][:]
        if not self.time:
            self.time = pd.Series(time_nc)
        else:
            # Check if the time steps are the same
            if not np.array_equal(self.time, time_nc):
                raise ValueError("The time steps of the netcdf file do not match "
                                 "the time steps of the hydro units data.")

        # Extract the unit id masks
        unit_id_masks = []
        for unit_id in unit_ids_list:
            unit_id_mask = hb.xr.where(unit_ids == unit_id, 1, 0)
            unit_id_masks.append(unit_id_mask)

        # Initialize data array
        data = np.zeros((unit_ids_nb, len(self.time)))

        # Drop other variables
        other_coords = [v for v in nc_data.coords if v not in [dim_time, dim_x, dim_y]]
        nc_data = nc_data.drop_vars(other_coords)

        # Extract variable
        data_var = nc_data[var_name]

        # Specify the CRS if not specified
        if data_var.rio.crs is None:
            with warnings.catch_warnings():
                warnings.simplefilter("ignore")
                data_var.rio.write_crs(f'epsg:{data_crs}', inplace=True)

        # Rename spatial dimensions
        data_var = data_var.rename({dim_x: 'x', dim_y: 'y'})

        # Extract each time step
        for t, time in enumerate(self.time):
            print(f"Extracting {time}")

            # Reproject
            with warnings.catch_warnings():
                warnings.simplefilter("ignore")
                data_var_t = data_var[t].rio.reproject_match(
                    unit_ids, Resampling=hb.rasterio.enums.Resampling.bilinear)

            # Extract data for each unit
            for u in range(unit_ids_nb):
                # Mask the meteorological data with the hydro unit.
                val = hb.xr.where(unit_id_masks[u], data_var_t, np.nan).to_numpy()
                # Mean the meteorological data in the unit.
                data[u][t] = np.nanmean(val)

        self.data.append(data)

    @staticmethod
    def _parse_crs(data, file_crs):
        if file_crs is None:
            if 'crs' in data.attrs:
                # Try to get it from the global attributes
                return data.attrs['crs']
            elif data.rio.crs:
                # Try to get it from the rio crs
                return data.rio.crs.to_epsg()
            else:
                raise ValueError("No CRS found in the netcdf file."
                                 "Please provide a CRS (option 'file_crs').")
        return file_crs
