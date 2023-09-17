import concurrent.futures
import os
import time
import warnings
from concurrent.futures import ThreadPoolExecutor
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
                           raster_hydro_units=None, method='weights',
                           weights_block_size=100):
        """
        Regrid time series data from netcdf files. The spatialization is done using a
        raster of hydro unit ids. The meteorological data is resampled to the DEM
        resolution.

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
        method : str
            Method to use for the spatialization. Can be 'reproject' or 'weights'.
            It does not change the result but the 'weights' method is faster.
        weights_block_size : int
            Size of the block of time steps to use for the 'weights' method.
            Default: 100.
        """
        if not hb.has_rasterio:
            raise ImportError("rasterio is required for 'regrid_from_netcdf'.")
        if not hb.has_rioxarray:
            raise ImportError("rioxarray is required for 'regrid_from_netcdf'.")
        if not hb.has_netcdf:
            raise ImportError("netCDF4 is required for 'regrid_from_netcdf'.")

        if raster_hydro_units is None:
            raise ValueError("You must provide a raster of the hydro units.")

        # Get unit ids
        with warnings.catch_warnings():
            warnings.filterwarnings("ignore", category=UserWarning)  # pyproj
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
                warnings.filterwarnings("ignore", category=UserWarning)  # pyproj
                unit_ids = unit_ids.rio.reproject(f'epsg:{data_crs}')

        # Get list of hydro unit ids
        unit_ids_list = np.unique(unit_ids)
        unit_ids_list = unit_ids_list[unit_ids_list != 0]
        unit_ids_nb = len(unit_ids_list)

        # Get list of time steps
        time_nc = nc_data.variables[dim_time][:]
        if len(self.time) == 0:
            self.time = pd.Series(time_nc)
        else:
            # Check if the time steps are the same
            if len(self.time) != len(time_nc):
                raise ValueError(f"The length of the netcdf time series "
                                 f"({len(time_nc)}) does not match the one from the "
                                 f"hydro units data ({len(self.time)}).")
            if self.time[0] != time_nc[0]:
                raise ValueError(f"The first time step of the netcdf time series "
                                 f"({time_nc[0].data}) does not match the one from the "
                                 f"hydro units data ({self.time[0]}).")
            if self.time[len(self.time) - 1] != time_nc[len(time_nc) - 1]:
                raise ValueError(f"The last time step of the netcdf time series "
                                 f"({time_nc[len(time_nc) - 1].data}) does not match "
                                 f"the one from the hydro units data "
                                 f"({self.time[len(self.time) - 1]}).")

        # Extract the unit id masks
        unit_id_masks = []
        for unit_id in unit_ids_list:
            unit_id_mask = hb.xr.where(unit_ids == unit_id, 1, 0)
            unit_id_masks.append(unit_id_mask)

        # Initialize data array
        data = np.zeros((len(self.time), unit_ids_nb))
        self.data.append(data)

        # Drop other variables
        other_coords = [v for v in nc_data.coords if v not in [dim_time, dim_x, dim_y]]
        nc_data = nc_data.drop_vars(other_coords)

        # Extract variable
        data_var = nc_data[var_name]

        # Specify the CRS if not specified
        if data_var.rio.crs is None:
            with warnings.catch_warnings():
                warnings.filterwarnings("ignore", category=UserWarning)  # pyproj
                data_var.rio.write_crs(f'epsg:{data_crs}', inplace=True)

        # Rename spatial dimensions
        data_var = data_var.rename({dim_x: 'x', dim_y: 'y'})

        # Time the computation
        start_time = time.time()

        num_threads = os.cpu_count()

        if method == 'reproject':
            # Create a ThreadPoolExecutor with a specified number of threads
            with warnings.catch_warnings():
                warnings.filterwarnings("ignore", category=UserWarning)  # pyproj
                with ThreadPoolExecutor(max_workers=num_threads) as executor:
                    # Submit the tasks for each time step to the executor
                    futures = [executor.submit(self._extract_time_step_data_reproject,
                                               data_var, unit_id_masks, unit_ids,
                                               unit_ids_nb, t)
                               for t in range(len(self.time))]

                    # Wait for all tasks to complete
                    concurrent.futures.wait(futures)

        elif method == 'weights':
            # Create a xarray variable containing the data cell indices
            data_idx = data_var[0].copy()
            data_idx.values = np.arange(data_idx.size).reshape(data_idx.shape)
            data_idx = data_idx.astype(float)

            # Reproject the data cell indices to the hydro unit raster
            with warnings.catch_warnings():
                warnings.filterwarnings("ignore", category=UserWarning)  # pyproj
                data_idx_reproj = data_idx.rio.reproject_match(
                    unit_ids, Resampling=hb.rasterio.enums.Resampling.nearest)

            # Create the masks (with the original data shape) for each unit with the
            # weights to apply to the gridded data contributing to the unit
            unit_weights = []
            for u in range(unit_ids_nb):
                # Get the data indices contributing to the unit
                mask_unit_id = hb.xr.where(unit_id_masks[u], data_idx_reproj, -1)
                mask_unit_id = mask_unit_id.to_numpy().astype(int)
                # Get unique values and their counts
                data_idx_values, counts = np.unique(mask_unit_id[mask_unit_id >= 0],
                                                    return_counts=True)
                # Create a mask of the weights
                weights_mask = np.zeros(data_idx.shape)
                data_idx_values = np.unravel_index(data_idx_values, data_idx.shape)
                weights_mask[data_idx_values] = counts / np.sum(counts)

                assert np.isclose(np.sum(weights_mask), 1)

                # Add the mask to the list
                unit_weights.append(weights_mask)

            n_steps = 1 + np.ceil(len(self.time) / weights_block_size).astype(int)

            with ThreadPoolExecutor(max_workers=num_threads) as executor:
                # Submit the tasks for each time step to the executor
                futures = [executor.submit(self._extract_time_step_data_weights,
                                           data_var, unit_weights, t_block,
                                           weights_block_size)
                           for t_block in range(n_steps)]

                # Wait for all tasks to complete
                concurrent.futures.wait(futures)

        else:
            raise ValueError(f"Unknown method '{method}'.")

        # Print elapsed time
        elapsed_time = time.time() - start_time
        print(f"Elapsed time: {elapsed_time:.2f} seconds (using {num_threads} threads)")

    def _extract_time_step_data_reproject(self, data_var, unit_id_masks, unit_ids,
                                          unit_ids_nb, t):
        # Print message very 20 time steps
        if t % 20 == 0:
            print(f"Extracting {self.time[t]}")

        # Reproject
        data_var_t = data_var[t].rio.reproject_match(
            unit_ids, Resampling=hb.rasterio.enums.Resampling.average)

        # Extract data for each unit
        for u in range(unit_ids_nb):
            # Mask the meteorological data with the hydro unit.
            val = hb.xr.where(unit_id_masks[u], data_var_t, np.nan).to_numpy()
            # Average the meteorological data in the unit.
            self.data[-1][t, u] = np.nanmean(val)

    def _extract_time_step_data_weights(self, data_var, unit_weights, i_block,
                                        block_size):
        i_start = i_block * block_size
        i_end = min((i_block + 1) * block_size, len(self.time))
        if i_start >= len(self.time):
            return

        print(f"Extracting {self.time[i_start]}")

        # Extract data for each unit
        for u, unit_weight in enumerate(unit_weights):
            # Mask the meteorological data with the unit weights.
            self.data[-1][i_start:i_end, u] = np.nansum(
                data_var[i_start:i_end].to_numpy() * unit_weight,
                axis=(1, 2))

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
