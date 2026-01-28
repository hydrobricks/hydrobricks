import concurrent.futures
import os
import time
import warnings
from concurrent.futures import ThreadPoolExecutor
from datetime import datetime
from pathlib import Path
from typing import TYPE_CHECKING, Any

import numpy as np
import pandas as pd

from hydrobricks import rxr, xr, rasterio, pyproj
from hydrobricks._optional import HAS_RASTERIO, HAS_RIOXARRAY, HAS_NETCDF
from hydrobricks._exceptions import (DependencyError, DataError, ConfigurationError)
from hydrobricks.utils import date_as_mjd

if TYPE_CHECKING:
    from hydrobricks.hydro_units import HydroUnits


class TimeSeries:
    """Class for generic time series data"""

    def __init__(self) -> None:
        """Initialize TimeSeries with empty time and data containers."""
        self.time: list[Any] | pd.Series | pd.DatetimeIndex = []
        self.data: list[np.ndarray] = []
        self.data_name: list[str] = []

    def get_dates_as_mjd(self) -> float | np.ndarray:
        """
        Convert time series dates to modified Julian dates.

        Returns
        -------
        float | np.ndarray
            Modified Julian dates. Returns float if single date, array if multiple dates.
        """
        return date_as_mjd(self.time)


class TimeSeries1D(TimeSeries):
    """Class for generic 1D time series data"""

    def __init__(self) -> None:
        """Initialize 1D TimeSeries."""
        super().__init__()

    def load_from_csv(
            self,
            path: str | Path,
            column_time: str,
            time_format: str,
            content: dict[str, str],
            start_date: datetime | pd.Timestamp | None = None,
            end_date: datetime | pd.Timestamp | None = None
    ) -> None:
        """
        Read time series data from CSV file.

        Parameters
        ----------
        path
            Path to the CSV file containing hydro units data.
        column_time
            Column name containing the time values.
        time_format
            Format string for parsing time values (e.g., '%Y-%m-%d').
        content
            Dictionary mapping variable names/enums to column names in the CSV.
            Example: {'precipitation': 'Precipitation (mm)', 'temperature': 'Temperature (C)'}
        start_date
            Start date of the time series (used to select the period of interest).
            If None, the first date of the file is used.
        end_date
            End date of the time series (used to select the period of interest).
            If None, the last date of the file is used.

        Raises
        ------
        FileNotFoundError
            If the specified file does not exist.
        KeyError
            If required columns are not found in the CSV file.
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

    def __init__(self) -> None:
        """Initialize 2D TimeSeries."""
        super().__init__()

    def regrid_from_netcdf(
            self,
            path: str | Path,
            file_pattern: str | None = None,
            data_crs: int | None = None,
            var_name: str | None = None,
            dim_time: str = 'time',
            dim_x: str = 'x',
            dim_y: str = 'y',
            hydro_units: Any | None = None,
            raster_hydro_units: str | Path | None = None,
            weights_block_size: int = 100,
            apply_data_gradient: bool = True,
            gradient_type: str = 'additive',
            dem_path: str | Path | None = None
    ) -> None:
        """
        Regrid time series data from netCDF files. The spatialization is done using a
        raster of hydro unit IDs. The meteorological data is resampled to the DEM
        resolution.

        Parameters
        ----------
        path
            Path to a netCDF file containing the data or to a folder containing
            multiple files.
        file_pattern
            Glob pattern of the files to read (e.g., '*.nc'). If None, the path is
            considered to be a single file.
        data_crs
            CRS of the netCDF file (as EPSG code). If None, the CRS is read from the file.
        var_name
            Name of the variable to read from the netCDF file.
        dim_time
            Name of the time dimension. Default: 'time'
        dim_x
            Name of the x/longitude dimension. Default: 'x'
        dim_y
            Name of the y/latitude dimension. Default: 'y'
        hydro_units
            HydroUnits object containing the hydro units to use for the spatialization.
            Needed if apply_data_gradient is True.
        raster_hydro_units
            Path to a raster file containing the hydro unit IDs to use for the
            spatialization.
        weights_block_size
            Size of the block of time steps to use for weight computation.
            Default: 100
        apply_data_gradient
            If True, elevation-based gradients will be retrieved from the data and
            applied to the hydro units (e.g., for temperature and precipitation).
            If False, the data will be regridded without applying any gradient.
            Default: True
        gradient_type
            Type of gradient to apply: 'additive' or 'multiplicative'.
            Default: 'additive'
        dem_path
            Path to DEM raster file for spatialization (gradient computation).
            Needed if apply_data_gradient is True.

        Raises
        ------
        ImportError
            If required optional dependencies (rasterio, rioxarray, netCDF4) are not installed.
        ValueError
            If raster_hydro_units is not provided, or if time/spatial dimensions don't match.
        """
        if not HAS_RASTERIO:
            raise DependencyError(
                "rasterio is required for regridding from netCDF.",
                package_name='rasterio',
                operation='TimeSeries2D.regrid_from_netcdf',
                install_command='pip install rasterio'
            )
        if not HAS_RIOXARRAY:
            raise DependencyError(
                "rioxarray is required for regridding from netCDF.",
                package_name='rioxarray',
                operation='TimeSeries2D.regrid_from_netcdf',
                install_command='pip install rioxarray'
            )
        if not HAS_NETCDF:
            raise DependencyError(
                "netCDF4 is required for regridding from netCDF.",
                package_name='netCDF4',
                operation='TimeSeries2D.regrid_from_netcdf',
                install_command='pip install netCDF4'
            )

        if raster_hydro_units is None:
            raise DataError(
                "You must provide a raster of the hydro units.",
                data_type='raster hydro units',
                reason='Missing required data'
            )

        # Get unit ids
        with warnings.catch_warnings():
            warnings.filterwarnings("ignore", category=UserWarning)  # pyproj
            unit_ids = rxr.open_rasterio(raster_hydro_units)
            unit_ids = unit_ids.squeeze().drop_vars("band")

        # Get netCDF dataset
        print(f"Reading netcdf file(s) from {path}...")
        if file_pattern is None:
            nc_data = xr.open_dataset(path, chunks={})
        else:
            files = sorted(Path(path).glob(file_pattern))
            nc_data = xr.open_mfdataset(files, chunks={})

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
        unit_ids_list = hydro_units['id'].values.squeeze()
        unit_id_count = len(unit_ids_list)

        # Check if the file has the dimension 'day_of_year'
        time_method = None
        if 'day_of_year' in nc_data.dims:
            time_method = 'day_of_year'
            day_of_year = nc_data.variables['day_of_year'][:]
            if len(self.time) == 0:
                raise DataError(
                    "Other forcing data with a full temporal array have "
                    "to be loaded and spatialized before data based "
                    "on 'day_of_year'.",
                    data_type='time series',
                    reason='Missing preceding forcing data'
                )
        else:
            time_method = 'full'
            time_nc = nc_data.variables[dim_time][:]
            if len(self.time) == 0:
                self.time = pd.Series(time_nc)

            # Check if the time steps are the same
            if len(self.time) != len(time_nc):
                raise DataError(
                    f"The length of the netcdf time series ({len(time_nc)}) "
                    f"does not match the one from the hydro units data ({len(self.time)}).",
                    data_type='time series',
                    reason='Mismatched time series length'
                )
            if self.time[0] != time_nc[0]:
                raise DataError(
                    f"The first time step of the netcdf time series "
                    f"({time_nc[0].data}) does not match the one from the "
                    f"hydro units data ({self.time[0]}).",
                    data_type='time series',
                    reason='Mismatched start date'
                )
            if self.time[len(self.time) - 1] != time_nc[len(time_nc) - 1]:
                raise DataError(
                    f"The last time step of the netcdf time series "
                    f"({time_nc[len(time_nc) - 1].data}) does not match "
                    f"the one from the hydro units data "
                    f"({self.time[len(self.time) - 1]}).",
                    data_type='time series',
                    reason='Mismatched end date'
                )

        # Extract the unit id masks
        unit_id_masks = []
        for unit_id in unit_ids_list:
            unit_id_mask = xr.where(unit_ids == unit_id, 1, 0)
            unit_id_masks.append(unit_id_mask)

        # Initialize data array
        data = np.zeros((len(self.time), unit_id_count))
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
            dem = rxr.open_rasterio(dem_path)
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
                raise DataError(
                    f"The time series based on 'day_of_year' must have a length of 366 "
                    f"(got {time_len}).",
                    data_type='time series',
                    reason='Invalid time series length for day_of_year'
                )

        # If we are using the 'apply_data_gradient' option, we need to first compute
        # the reference elevation for each data grid cell.
        dem_data, dem_dx, dem_dy, hu_elevation = None, None, None, None
        if apply_data_gradient:
            data_grid = data_var[0].copy()
            dem_reproj = dem.rio.reproject_match(
                data_grid,
                Resampling=rasterio.enums.Resampling.average,
                nodata=np.nan,
            )
            dem_data = dem_reproj.values

            # Compute the gradient of the DEM along the x and y axes
            dem_dx = dem_reproj.diff('x')
            dem_dy = dem_reproj.diff('y')

            # Replace small values (<50m) with NaN to avoid irrelevant gradients
            dem_dx = xr.where(np.abs(dem_dx) < 50, np.nan, dem_dx).compute()
            dem_dy = xr.where(np.abs(dem_dy) < 50, np.nan, dem_dy).compute()

            # If both gradients contain more than 60% NaN values, raise a warning
            if (dem_dx.isnull().sum() / dem_dx.size > 0.6 and
                    dem_dy.isnull().sum() / dem_dy.size > 0.6):
                print("More than 60% of the DEM gradients are too small. "
                      "Defaulting to apply_data_gradient=False.")
                apply_data_gradient = False

            # Extract the elevation for each hydro unit
            hu_elevation = hydro_units['elevation'].to_numpy().squeeze()

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
                Resampling=rasterio.enums.Resampling.nearest
            )

        # Create the masks (with the original data shape) for each unit with the
        # weights to apply to the gridded data contributing to the unit
        unit_weights = []
        for u in range(unit_id_count):
            # Get the data indices contributing to the unit
            mask_unit_id = xr.where(unit_id_masks[u], data_idx_reproj, -1)
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
            if apply_data_gradient:
                futures = [
                    executor.submit(self._extract_time_step_data_weights_with_gradient,
                                    data_var, unit_weights, t_block,
                                    weights_block_size, gradient_type,
                                    dem_data, dem_dx, dem_dy, hu_elevation)
                    for t_block in range(n_steps)
                ]
            else:
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

    def _extract_time_step_data_weights_with_gradient(
            self,
            data_var: xr.DataArray,
            unit_weights: list,
            i_block: int,
            block_size: int,
            gradient_type: str,
            dem: np.ndarray,
            dem_dx: xr.DataArray,
            dem_dy: xr.DataArray,
            hu_elevation: np.ndarray
    ) -> None:
        """
        Extract time step data with elevation-based gradient corrections.

        Applies elevation-based gradients to gridded meteorological data to account
        for the effect of elevation differences on temperature, precipitation, etc.
        Processes data in blocks to optimize memory usage.

        Parameters
        ----------
        data_var
            3D xarray DataArray with dimensions (time, y, x) containing gridded data.
        unit_weights
            List of 2D weight arrays for each hydro unit, summing to 1.
        i_block
            Block index for this processing step.
        block_size
            Number of time steps to process in this block.
        gradient_type
            Type of gradient to apply: 'additive' or 'multiplicative'.
        dem
            2D array of digital elevation model data.
        dem_dx
            xarray DataArray with DEM gradients in x direction.
        dem_dy
            xarray DataArray with DEM gradients in y direction.
        hu_elevation
            1D array of elevation for each hydro unit.
        """
        i_start = i_block * block_size
        i_end = min((i_block + 1) * block_size, len(self.time))
        i_end = min(i_end, data_var.shape[0])
        if i_start >= len(self.time):
            return

        print(f"Extracting {self.time[i_start]}")

        if gradient_type == 'additive':
            dat_dx = data_var[i_start:i_end].diff('x')
            dat_dy = data_var[i_start:i_end].diff('y')
        elif gradient_type == 'multiplicative':
            data_xr = data_var[i_start:i_end]
            dat_dx = data_xr.diff('x')
            dat_dy = data_xr.diff('y')
            zero_mask = np.abs(data_xr) < 1e-10
            with np.errstate(invalid='ignore', divide='ignore'):
                dat_dx_np = np.where(
                    zero_mask.isel(x=slice(1, None)).values,
                    0,
                    dat_dx.values / data_xr.isel(x=slice(1, None)).values
                )
                dat_dy_np = np.where(
                    zero_mask.isel(y=slice(1, None)).values,
                    0,
                    dat_dy.values / data_xr.isel(y=slice(1, None)).values
                )

            dat_dx.data = dat_dx_np
            dat_dy.data = dat_dy_np

        else:
            raise ConfigurationError(
                f"Unknown gradient type: {gradient_type}. "
                "Use 'additive' or 'multiplicative'.",
                item_name='gradient_type',
                item_value=gradient_type
            )

        # Compute the gradient of the data along the x and y axes
        dat_dx = dat_dx / dem_dx
        dat_dy = dat_dy / dem_dy

        # Fill NaN values
        dat_dx = self._fill_nan_gradients(dat_dx)
        dat_dy = self._fill_nan_gradients(dat_dy)

        # Compute the rolling mean of the gradients
        dat_dx = dat_dx.rolling(x=3, y=3, center=True, min_periods=1).mean()
        dat_dy = dat_dy.rolling(x=3, y=3, center=True, min_periods=1).mean()

        # Combine the gradients into a single gradient
        dat_dxy = self._mean_xy_gradient(dat_dx, dat_dy).to_numpy()

        if dat_dxy.shape[1:] != data_var.shape[1:]:
            raise DataError(
                f"The shape of the data gradient ({dat_dxy.shape[1:]}) does not match "
                f"the shape of the data variable ({data_var.shape[1:]}).",
                data_type='time series',
                reason='Shape mismatch'
            )

        if dem.shape != data_var.shape[1:]:
            raise DataError(
                f"The shape of the DEM ({dem.shape}) does not match the "
                f"shape of the data variable ({data_var.shape[1:]}).",
                data_type='time series',
                reason='Shape mismatch'
            )

        # Extract data for each unit
        data_array = data_var[i_start:i_end].to_numpy()
        for u, unit_weight in enumerate(unit_weights):
            data = data_array[:, unit_weight > 0]
            weights = unit_weight[unit_weight > 0]
            dh = hu_elevation[u] - dem[unit_weight > 0]
            dh[np.isnan(dh)] = 0
            grads = dat_dxy[:, unit_weight > 0]
            if gradient_type == 'additive':
                data = data + grads * dh
            elif gradient_type == 'multiplicative':
                grads_dh = grads * dh
                grads_dh[grads_dh < -1] = -1
                data = data * (1 + grads_dh)
            else:
                raise ConfigurationError(
                    f"Unsupported gradient type: {gradient_type}",
                    item_name='gradient_type',
                    item_value=gradient_type,
                    reason='Invalid gradient type'
                )

            self.data[-1][i_start:i_end, u] = np.nansum(data * weights, axis=1)

    def _extract_time_step_data_weights(
            self,
            data_var: xr.DataArray,
            unit_weights: list,
            i_block: int,
            block_size: int
    ) -> None:
        """
        Extract time step data and apply spatial weights.

        Extracts meteorological data for a block of time steps and applies weighted
        averaging based on the spatial distribution of data cells within each
        hydro unit.

        Parameters
        ----------
        data_var
            3D xarray DataArray with dimensions (time, y, x) containing gridded data.
        unit_weights
            List of 2D weight arrays for each hydro unit, summing to 1.
        i_block
            Block index for this processing step.
        block_size
            Number of time steps to process in this block.
        """
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

    def _select_relevant_extent(
            self,
            data_var: xr.DataArray,
            data_crs: int,
            dim_x: str,
            dim_y: str,
            ref_data: xr.DataArray | xr.Dataset
    ) -> xr.DataArray:
        """
        Select the spatial extent of gridded data relevant to the reference data.

        Clips the input data to the bounding box of the reference data (DEM or hydro units),
        handling CRS transformations if necessary.

        Parameters
        ----------
        data_var
            The gridded data variable to clip.
        data_crs
            CRS of the gridded data (as EPSG code).
        dim_x
            Name of the x/longitude dimension in data_var.
        dim_y
            Name of the y/latitude dimension in data_var.
        ref_data
            Reference dataset (DEM or hydro units raster) to determine extent.

        Returns
        -------
        xr.DataArray
            Clipped data variable containing only the relevant spatial extent.
        """
        x_ref_min, x_ref_max, y_ref_min, y_ref_max = self._get_spatial_bounds(ref_data)

        # Convert the spatial extent to the data CRS
        src_crs = self._parse_crs(ref_data)
        if src_crs != data_crs:
            transformer = pyproj.Transformer.from_crs(
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
            data: xr.DataArray | xr.Dataset,
            file_crs: int | None = None
    ) -> int:
        """
        Extract CRS information from xarray data.

        Attempts to retrieve CRS from multiple sources: explicit parameter, data attributes,
        or rioxarray crs property. Raises error if CRS cannot be determined.

        Parameters
        ----------
        data
            xarray DataArray or Dataset to extract CRS from.
        file_crs
            Explicit CRS as EPSG code. If provided, this value is returned directly.

        Returns
        -------
        int
            CRS as EPSG code.

        Raises
        ------
        DataError
            If no CRS is found and file_crs is not provided.
        """
        if file_crs is None:
            if 'crs' in data.attrs:
                # Try to get it from the global attributes
                return data.attrs['crs']
            elif data.rio.crs:
                # Try to get it from the rio crs
                return data.rio.crs.to_epsg()
            else:
                raise DataError(
                    "Could not determine the CRS from the data."
                    "Please provide a CRS (option 'file_crs').",
                    data_type='spatial data',
                    reason='Missing CRS information'
                )
        return file_crs

    @staticmethod
    def _get_spatial_bounds(ref_data: xr.DataArray | xr.Dataset) -> tuple:
        """
        Extract spatial bounds from xarray data.

        Determines the minimum and maximum coordinates in x and y dimensions,
        automatically detecting dimension names (x/lon/longitude, y/lat/latitude).

        Parameters
        ----------
        ref_data
            xarray DataArray or Dataset containing spatial data.

        Returns
        -------
        tuple
            Tuple of (x_min, x_max, y_min, y_max) spatial bounds.

        Raises
        ------
        ValueError
            If spatial dimensions cannot be found in the data.
        """
        # Possible names for spatial dimensions
        x_names = ['x', 'lon', 'longitude']
        y_names = ['y', 'lat', 'latitude']

        # Find the actual dimension names
        x_dim = next((name for name in x_names if name in ref_data.dims), None)
        y_dim = next((name for name in y_names if name in ref_data.dims), None)

        if x_dim is None or y_dim is None:
            raise DataError(
                f"Could not find spatial dimensions in the reference data. "
                f"Available dimensions: {list(ref_data.dims)}",
                data_type='spatial data',
                reason='Missing x/y or lon/lat dimensions'
            )

        x_ref_min = ref_data[x_dim].min().item()
        x_ref_max = ref_data[x_dim].max().item()
        y_ref_min = ref_data[y_dim].min().item()
        y_ref_max = ref_data[y_dim].max().item()

        return x_ref_min, x_ref_max, y_ref_min, y_ref_max

    @staticmethod
    def _fill_nan_gradients(dat: xr.DataArray) -> xr.DataArray:
        """
        Fill NaN values in gradient arrays through interpolation and edge extension.

        Uses linear interpolation along x and y dimensions, then extends edge values
        to replace any remaining NaN values. Processes up to two iterations to ensure
        all NaN values are filled.

        Parameters
        ----------
        dat
            xarray DataArray containing gradient data with potential NaN values.

        Returns
        -------
        xr.DataArray
            DataArray with NaN values filled using interpolation and edge extension.

        Notes
        -----
        This function modifies the input array in-place for efficiency.
        """
        # Fill NaN values in the gradients (loop twice)
        for _ in range(2):
            if np.isnan(dat).any():
                # Interpolate NaN values in the gradients
                dat = dat.interpolate_na(dim="x").interpolate_na(dim="y")

                # Replace NaN values at the edges
                arr = dat.values
                x_ax = dat.get_axis_num('x')
                y_ax = dat.get_axis_num('y')

                def _replace_edge(axis, idx, neighbor_idx):
                    target = [slice(None)] * arr.ndim
                    neighbor = [slice(None)] * arr.ndim
                    target[axis] = idx
                    neighbor[axis] = neighbor_idx
                    m = np.isnan(arr[tuple(target)])
                    arr[tuple(target)][m] = arr[tuple(neighbor)][m]

                _replace_edge(x_ax, 0, 1)  # left
                _replace_edge(x_ax, -1, -2)  # right
                _replace_edge(y_ax, 0, 1)  # bottom
                _replace_edge(y_ax, -1, -2)  # top

                dat.data = arr

        return dat

    @staticmethod
    def _mean_xy_gradient(
            dat_dx: xr.DataArray,
            dat_dy: xr.DataArray
    ) -> xr.DataArray:
        """
        Compute the mean of x and y gradients on a common grid.

        Creates a new grid whose 'x' coordinate comes from dat_dy and 'y' coordinate
        comes from dat_dx. Reindexes both arrays to this grid using nearest neighbour
        for missing edges. Returns the mean.

        Parameters
        ----------
        dat_dx
            xarray DataArray containing gradients in x direction.
            Must have 'x' and 'y' dimensions.
        dat_dy
            xarray DataArray containing gradients in y direction.
            Must have 'x' and 'y' dimensions.

        Returns
        -------
        xr.DataArray
            Mean of the x and y gradients on the common grid.

        Raises
        ------
        ValueError
            If required 'x' and 'y' dimensions are missing from input arrays.
        """
        # Ensure both have x and y dims
        if "x" not in dat_dx.dims or "y" not in dat_dx.dims:
            raise DataError(
                f"dat_dx must have 'x' and 'y' dimensions. "
                f"Got dimensions: {dat_dx.dims}",
                data_type='time series',
                reason='Missing x or y dimension'
            )
        if "x" not in dat_dy.dims or "y" not in dat_dy.dims:
            raise DataError(
                f"dat_dy must have 'x' and 'y' dimensions. "
                f"Got dimensions: {dat_dy.dims}",
                data_type='time series',
                reason='Missing x or y dimension'
            )

        # Target coordinates
        coords = {}
        for dim in dat_dx.dims:
            if dim == "x":
                coords["x"] = dat_dy["x"]
            elif dim == "y":
                coords["y"] = dat_dx["y"]
            else:
                coords[dim] = dat_dx[dim] if dim in dat_dx.coords else dat_dy[dim]

        # Create target grid
        dat_dxy = xr.DataArray(
            dims=dat_dx.dims,
            coords=coords
        )

        # Reindex both arrays to this grid
        dat_dx_expanded = dat_dx.reindex_like(dat_dxy, method="nearest")
        dat_dy_expanded = dat_dy.reindex_like(dat_dxy, method="nearest")

        # Compute mean
        return (dat_dx_expanded + dat_dy_expanded) / 2
