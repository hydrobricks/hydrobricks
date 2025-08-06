import concurrent.futures
import os
import time
import warnings
from concurrent.futures import ThreadPoolExecutor
from datetime import datetime
from pathlib import Path

import numpy as np
import pandas as pd

import hydrobricks as hb


class TimeSeries:
    """Class for generic time series data"""

    def __init__(self):
        self.time = []
        self.data = []
        self.data_name = []

    def get_dates_as_mjd(self):
        return hb.utils.date_as_mjd(self.time)


class TimeSeries1D(TimeSeries):
    """Class for generic 1D time series data"""

    def __init__(self):
        super().__init__()

    def load_from_csv(
            self,
            path: str | Path,
            column_time: str,
            time_format: str,
            content: dict,
            start_date: datetime | None = None,
            end_date: datetime | None = None
    ):
        """
        Read time series data from csv file.

        Parameters
        ----------
        path
            Path to the csv file containing hydro units data.
        column_time
            Column name containing the time.
        time_format
            Format of the time
        content
            Type of data and column name containing the data.
            Example: {'precipitation': 'Precipitation (mm)'}
        start_date
            Start date of the time series (used to select the period of interest).
            If None, the first date of the file is used.
        end_date
            End date of the time series (used to select the period of interest).
            If None, the last date of the file is used.
        """
        file_content = pd.read_csv(
            path, parse_dates=[column_time],
            date_format=time_format)

        if start_date and end_date:
            file_content = file_content.loc[
                (file_content[column_time] >= start_date) &
                (file_content[column_time] <= end_date)]

        self.time = file_content[column_time]

        for col in content:
            self.data_name.append(col)
            self.data.append(file_content[content[col]].to_numpy())


class TimeSeries2D(TimeSeries):
    """Class for generic 2D time series data"""

    def __init__(self):
        super().__init__()

    def regrid_from_netcdf(
            self,
            path: str | Path,
            file_pattern: str = None,
            data_crs: int | None = None,
            var_name: str | None = None,
            dim_time: str = 'time',
            dim_x: str = 'x',
            dim_y: str = 'y',
            hydro_units: HydroUnits | None = None,
            raster_hydro_units: str | Path | None = None,
            weights_block_size: int = 100,
            apply_data_gradient: bool = True,
            dem_path: str | Path | None = None
    ):
        """
        Regrid time series data from netcdf files. The spatialization is done using a
        raster of hydro unit ids. The meteorological data is resampled to the DEM
        resolution.

        Parameters
        ----------
        path
            Path to a netcdf file containing the data or to a folder containing
            multiple files.
        file_pattern
            Pattern of the files to read. If None, the path is considered to be
            a single file.
        data_crs
            CRS (as EPSG id) of the netcdf file. If None, the CRS is read from the file.
        var_name
            Name of the variable to read.
        dim_time
            Name of the time dimension.
        dim_x
            Name of the x dimension.
        dim_y
            Name of the y dimension.
        hydro_units
            HydroUnits object containing the hydro units to use for the spatialization.
            Needed if `apply_data_gradient` is True.
        raster_hydro_units
            Path to a raster containing the hydro unit ids to use for the
            spatialization.
        weights_block_size
            Size of the block of time steps to use for the 'weights' method.
            Default: 100.
        apply_data_gradient
            If True, elevation-based gradients will be retrieved from the data and
            applied to the hydro units (e.g., for temperature and precipitation).
            If False, the data will be regridded without applying any gradient.
        dem_path
            DEM to use for the spatialization (computation of the gradients).
            Needed if `apply_data_gradient` is True.
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
            nc_data = hb.xr.open_dataset(path, chunks={})
        else:
            files = sorted(Path(path).glob(file_pattern))
            nc_data = hb.xr.open_mfdataset(files, chunks={})

        # Get CRS of the netcdf file
        data_crs = self._parse_crs(nc_data, data_crs)

        # Get CRS of the unit ids raster
        unit_ids_crs = self._parse_crs(unit_ids, None)

        if data_crs != unit_ids_crs:
            print("The CRS of the netcdf file does not match the CRS of the "
                  "hydro unit ids raster. Reprojection will be done from "
                  f"{unit_ids_crs} to {data_crs}.")
            with warnings.catch_warnings():
                warnings.filterwarnings("ignore", category=UserWarning)  # pyproj
                unit_ids = unit_ids.rio.reproject(f'epsg:{data_crs}')

        # Get list of hydro unit ids
        unit_ids_list = np.unique(unit_ids)
        unit_ids_list = unit_ids_list[unit_ids_list != 0]
        unit_ids_nb = len(unit_ids_list)

        # Check if the file has the dimension 'day_of_year'
        time_method = None
        if 'day_of_year' in nc_data.dims:
            time_method = 'day_of_year'
            day_of_year = nc_data.variables['day_of_year'][:]
            if len(self.time) == 0:
                raise ValueError("Other forcing data with a full temporal array have "
                                 "to be loaded and spatialized before data based "
                                 "on 'day_of_year'.")
        else:
            time_method = 'full'
            time_nc = nc_data.variables[dim_time][:]
            if len(self.time) == 0:
                self.time = pd.Series(time_nc)

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
        other_coords = [v for v in nc_data.coords if v not in [
            dim_time, dim_x, dim_y, 'day_of_year']]
        nc_data = nc_data.drop_vars(other_coords)

        # Extract variable
        data_var = nc_data[var_name]

        # Specify the CRS if not specified
        if data_var.rio.crs is None:
            with warnings.catch_warnings():
                warnings.filterwarnings("ignore", category=UserWarning)  # pyproj
                data_var.rio.write_crs(f'epsg:{data_crs}', inplace=True)

        # Open the DEM if needed
        dem = None
        if apply_data_gradient:
            dem = hb.rxr.open_rasterio(dem_path)
            dem = dem.squeeze().drop_vars("band")

        # Get the spatial extent of interest
        ref_data = dem if apply_data_gradient else unit_ids
        data_var = self._select_relevant_extent(
            data_var, data_crs, dim_x, dim_y, ref_data)

        # Rename spatial dimensions
        if dim_x != 'x':
            data_var = data_var.rename({dim_x: 'x'})
        if dim_y != 'y':
            data_var = data_var.rename({dim_y: 'y'})

        # Time the computation
        start_time = time.time()

        num_threads = os.cpu_count()
        time_len = len(self.time)
        if time_method == 'day_of_year':
            time_len = len(day_of_year)
            if time_len != 366:
                raise ValueError("The time series based on 'day_of_year' must have "
                                 "a length of 366.")

        # Create a xarray variable containing the data cell indices
        data_idx = data_var[0].copy()
        data_idx.values = np.arange(data_idx.size).reshape(data_idx.shape)
        data_idx = data_idx.astype(float)

        # Reproject the data cell indices to the hydro unit raster
        with warnings.catch_warnings():
            warnings.filterwarnings("ignore", category=UserWarning)  # pyproj
            data_idx.rio.write_crs(f'epsg:{data_crs}', inplace=True)
            data_idx_reproj = data_idx.rio.reproject_match(
                unit_ids,
                Resampling=hb.rasterio.enums.Resampling.nearest
            )

        # Create the masks (with the original data shape) for each unit with the
        # weights to apply to the gridded data contributing to the unit
        unit_weights = []
        for u in range(unit_ids_nb):
            # Get the data indices contributing to the unit
            mask_unit_id = hb.xr.where(unit_id_masks[u], data_idx_reproj, -1)
            mask_unit_id = mask_unit_id.to_numpy().astype(int)
            # Get unique values and their counts
            data_idx_values, counts = np.unique(
                mask_unit_id[mask_unit_id >= 0],
                return_counts=True
            )
            # Create a mask of the weights
            weights_mask = np.zeros(data_idx.shape)
            data_idx_values = np.unravel_index(data_idx_values, data_idx.shape)
            weights_mask[data_idx_values] = counts / np.sum(counts)

            assert np.isclose(np.sum(weights_mask), 1)

            # Add the mask to the list
            unit_weights.append(weights_mask)

        n_steps = 1 + np.ceil(time_len / weights_block_size).astype(int)

        with ThreadPoolExecutor(max_workers=num_threads) as executor:
            # Submit the tasks for each time step to the executor
            futures = [
                executor.submit(self._extract_time_step_data_weights,
                                data_var, unit_weights, t_block,
                                weights_block_size)
                for t_block in range(n_steps)
            ]

            # Wait for all tasks to complete
            concurrent.futures.wait(futures)

        # If the time method is 'day_of_year', convert to the full time series
        if time_method == 'day_of_year':
            print("Converting to the full time series...")
            # Get the indices of jd_unique that match the values of jd
            jd = self.time.dt.strftime('%j').to_numpy().astype(int)
            indices = np.searchsorted(day_of_year, jd)
            whole_daily_pot_radiation = self.data[-1][indices, :]
            self.data[-1] = whole_daily_pot_radiation

        # Print elapsed time
        elapsed_time = time.time() - start_time
        print(f"Elapsed time: {elapsed_time:.2f} seconds (using {num_threads} threads)")

    def _extract_time_step_data_weights(
            self,
            data_var: hb.xr.DataArray,
            unit_weights: list,
            i_block: int,
            block_size: int
    ):
        i_start = i_block * block_size
        i_end = min((i_block + 1) * block_size, len(self.time))
        i_end = min(i_end, data_var.shape[0])
        if i_start >= len(self.time):
            return

        print(f"Extracting {self.time[i_start]}")

        # Extract data for each unit
        for u, unit_weight in enumerate(unit_weights):
            # Mask the meteorological data with the unit weights.
            self.data[-1][i_start:i_end, u] = np.nansum(
                data_var[i_start:i_end].to_numpy() * unit_weight,
                axis=(1, 2))

    def _select_relevant_extent(self, data_var, data_crs, dim_x, dim_y, ref_data):
        x_ref_min, x_ref_max, y_ref_min, y_ref_max = self._get_spatial_bounds(ref_data)

        # Convert the spatial extent to the data CRS
        src_crs = self._parse_crs(ref_data)
        if src_crs != data_crs:
            transformer = hb.pyproj.Transformer.from_crs(
                src_crs, data_crs, always_xy=True)
            x_min_dat, y_min_dat = transformer.transform(x_ref_min, y_ref_min)
            x_max_dat, y_max_dat = transformer.transform(x_ref_max, y_ref_max)
        else:
            x_min_dat, y_min_dat = x_ref_min, y_ref_min
            x_max_dat, y_max_dat = x_ref_max, y_ref_max

        # Find the coordinates that cover the extent
        x_coords = data_var[dim_x].values
        y_coords = data_var[dim_y].values

        x_start_idx = np.searchsorted(x_coords, x_min_dat, side='right') - 1
        x_end_idx = np.searchsorted(x_coords, x_max_dat, side='left')
        y_start_idx = np.searchsorted(y_coords, y_min_dat, side='right') - 1
        y_end_idx = np.searchsorted(y_coords, y_max_dat, side='left')

        x_start = x_coords[max(x_start_idx, 0)]
        x_end = x_coords[min(x_end_idx, len(x_coords) - 1)]
        y_start = y_coords[max(y_start_idx, 0)]
        y_end = y_coords[min(y_end_idx, len(y_coords) - 1)]

        x_sel = slice(min(x_start, x_end), max(x_start, x_end))
        y_sel = slice(min(y_start, y_end), max(y_start, y_end))

        data_var = data_var.sel({dim_x: x_sel, dim_y: y_sel})

        return data_var

    @staticmethod
    def _parse_crs(
            data: hb.xr.DataArray | hb.xr.Dataset,
            file_crs: int | None = None
    ) -> int:
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

    @staticmethod
    def _get_spatial_bounds(ref_data: hb.xr.DataArray | hb.xr.Dataset) -> tuple:
        # Possible names for spatial dimensions
        x_names = ['x', 'lon', 'longitude']
        y_names = ['y', 'lat', 'latitude']

        # Find the actual dimension names
        x_dim = next((name for name in x_names if name in ref_data.dims), None)
        y_dim = next((name for name in y_names if name in ref_data.dims), None)

        if x_dim is None or y_dim is None:
            raise ValueError("Could not find spatial dimensions in the reference data.")

        x_ref_min = ref_data[x_dim].min().item()
        x_ref_max = ref_data[x_dim].max().item()
        y_ref_min = ref_data[y_dim].min().item()
        y_ref_max = ref_data[y_dim].max().item()

        return x_ref_min, x_ref_max, y_ref_min, y_ref_max