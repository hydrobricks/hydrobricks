from __future__ import annotations

import math
import warnings
from pathlib import Path
from typing import TYPE_CHECKING

import numpy as np

from hydrobricks import rxr, xrs, rasterio
from hydrobricks._exceptions import DependencyError, DataError
from hydrobricks._optional import HAS_RASTERIO, HAS_SHAPELY, HAS_PYARROW, HAS_XRSPATIAL

if TYPE_CHECKING:
    from hydrobricks.catchment import Catchment

if HAS_RASTERIO:
    from rasterio.enums import Resampling
    from rasterio.mask import mask

if HAS_SHAPELY:
    from shapely.geometry import mapping


class CatchmentTopography:
    """
    A class to represent the topography of a catchment.
    """

    def __init__(self, catchment: Catchment) -> None:
        """
        Initialize the Topography class.

        Parameters
        ----------
        catchment
            The catchment object.
        """
        self.catchment: Catchment = catchment
        self.slope: np.ndarray | None = None
        self.aspect: np.ndarray | None = None

    def get_mean_elevation(self) -> float:
        """
        Get the catchment mean elevation.

        Computes the mean elevation across all DEM cells within the catchment,
        ignoring NaN values.

        Returns
        -------
        float
            The catchment mean elevation in meters.
        """
        return np.nanmean(self.catchment.dem_data)

    def resample_dem_and_calculate_slope_aspect(
            self,
            resolution: float,
            output_path: str | Path | None = None
    ) -> tuple[rasterio.DatasetReader, np.ndarray, np.ndarray, np.ndarray]:
        """
        Resample the DEM and calculate the slope and aspect of the whole DEM.

        Downsamples the DEM to a specified resolution using bilinear interpolation,
        then computes slope and aspect for the resampled DEM. Results are saved to
        disk and can be used for coarser resolution calculations.

        Parameters
        ----------
        resolution
            Desired pixel resolution in meters. If None or matches current resolution,
            uses the original DEM without resampling.
        output_path
            Path of the directory to save the downsampled DEM to. Required if
            resolution differs from current DEM resolution.

        Returns
        -------
        tuple[rasterio.DatasetReader, np.ndarray, np.ndarray, np.ndarray]
            Tuple containing:
            - Resampled DEM dataset
            - Masked DEM data array
            - Slope array (in degrees)
            - Aspect array (in degrees, 0-360)
        """
        if not HAS_RASTERIO:
            raise DependencyError(
                "rasterio is required for DEM resampling and processing.",
                package_name='rasterio',
                operation='CatchmentTopography.resample_dem_and_calculate_slope_aspect',
                install_command='pip install rasterio'
            )
        if not HAS_PYARROW:
            raise DependencyError(
                "pyarrow is required for DEM resampling.",
                package_name='pyarrow',
                operation='CatchmentTopography.resample_dem_and_calculate_slope_aspect',
                install_command='pip install pyarrow'
            )
        if not HAS_XRSPATIAL:
            raise DependencyError(
                "xarray-spatial is required for slope and aspect calculations.",
                package_name='xarray-spatial',
                operation='CatchmentTopography.resample_dem_and_calculate_slope_aspect',
                install_command='pip install xarray-spatial'
            )

        # Only resample the DEM if the resolution is different from the original
        if resolution is None or resolution == self.catchment.get_dem_x_resolution():
            if self.slope is None or self.aspect is None:
                self.calculate_slope_aspect()
            return self.catchment.dem, self.catchment.dem_data, self.slope, self.aspect

        with warnings.catch_warnings():
            warnings.filterwarnings("ignore", category=UserWarning)  # pyproj
            dem_file = self.catchment.dem.files[0]
            xr_dem = rxr.open_rasterio(dem_file).drop_vars('band')[0]

        x_downscale_factor = self.catchment.get_dem_x_resolution() / resolution
        y_downscale_factor = self.catchment.get_dem_y_resolution() / resolution

        new_width = int(xr_dem.rio.width * x_downscale_factor)
        new_height = int(xr_dem.rio.height * y_downscale_factor)

        with warnings.catch_warnings():
            warnings.filterwarnings("ignore", category=UserWarning)  # pyproj
            xr_dem_downsampled = xr_dem.rio.reproject(
                xr_dem.rio.crs,
                shape=(new_height, new_width),
                resampling=Resampling.bilinear,
            )

        # Save the downsampled DEM to a file
        if isinstance(output_path, str):
            output_path = Path(output_path)
        filepath = output_path / 'downsampled_dem.tif'
        xr_dem_downsampled.rio.to_raster(filepath)

        # Reopen the downsampled DEM as a rasterio dataset
        new_dem = rasterio.open(filepath)
        if self.catchment.outline is not None:
            geoms = [mapping(polygon) for polygon in self.catchment.outline]
            new_dem_data, _ = mask(new_dem, geoms, crop=False)
        else:
            new_dem_data = new_dem.read(1)
        new_dem_data[new_dem_data == new_dem.nodata] = np.nan
        if len(new_dem_data.shape) == 3:
            new_dem_data = new_dem_data[0]
        new_slope = xrs.slope(xr_dem_downsampled, name='slope').to_numpy()
        new_aspect = xrs.aspect(xr_dem_downsampled, name='aspect').to_numpy()
        xr_dem_downsampled.close()

        return new_dem, new_dem_data, new_slope, new_aspect

    def calculate_slope_aspect(self) -> None:
        """
        Calculate the slope and aspect of the whole DEM.

        Computes slope in degrees and aspect (0-360°) from the DEM using
        spatial derivatives. Results stored in self.slope and self.aspect.
        """
        if not HAS_PYARROW:
            raise DependencyError(
                "pyarrow is required for slope and aspect calculations.",
                package_name='pyarrow',
                operation='CatchmentTopography.calculate_slope_aspect',
                install_command='pip install pyarrow'
            )
        if not HAS_XRSPATIAL:
            raise DependencyError(
                "xarray-spatial is required for slope and aspect calculations.",
                package_name='xarray-spatial',
                operation='CatchmentTopography.calculate_slope_aspect',
                install_command='pip install xarray-spatial'
            )

        with warnings.catch_warnings():
            warnings.filterwarnings("ignore", category=UserWarning)  # pyproj
            dem_file = self.catchment.dem.files[0]
            xr_dem = rxr.open_rasterio(dem_file).drop_vars('band')[0]
            self.slope = xrs.slope(xr_dem, name='slope').to_numpy()
            self.aspect = xrs.aspect(xr_dem, name='aspect').to_numpy()

    def get_hillshade(
            self,
            azimuth: float = 315,
            altitude: float = 45,
            z_factor: float = 1
    ) -> np.ndarray:
        """
        Create a hillshade from the DEM.

        Adapted from https://github.com/royalosyin/Work-with-DEM-data-using-Python-
        from-Simple-to-Complicated/blob/master/ex07-Hillshade%20from%20a%20Digital
        %20Elevation%20Model%20(DEM).ipynb

        Parameters
        ----------
        azimuth
            The desired azimuth for the hillshade.
        altitude
            The desired sun angle altitude for the hillshade.
        z_factor
            The z factor to amplify the relief.

        Returns
        -------
        np.ndarray
            2D array of hillshade values (0-255) representing shaded relief visualization.
        """
        x, y = np.gradient(self.catchment.dem.read(1))
        x_pixel_size = self.catchment.get_dem_x_resolution()
        y_pixel_size = self.catchment.get_dem_y_resolution()

        if azimuth > 360.0:
            raise ConfigurationError(
                "Azimuth value should be less than or equal to 360 degrees",
                parameter_name='azimuth',
                parameter_value=azimuth,
                reason='Value out of valid range'
            )

        if altitude > 90.0:
            raise ConfigurationError(
                "Altitude value should be less than or equal to 90 degrees",
                parameter_name='altitude',
                parameter_value=altitude,
                reason='Value out of valid range'
            )

        # Account for the pixel size
        x = z_factor * x / x_pixel_size
        y = z_factor * y / y_pixel_size

        azimuth = 360.0 - azimuth
        azimuth_rad = azimuth * np.pi / 180.0
        altitude_rad = altitude * np.pi / 180.0

        slope = np.pi / 2.0 - np.arctan(np.sqrt(x * x + y * y))
        aspect = np.arctan2(-x, y)

        shaded = (np.sin(altitude_rad) * np.sin(slope) +
                  np.cos(altitude_rad) * np.cos(slope) *
                  np.cos((azimuth_rad - np.pi / 2.0) - aspect))

        return 255 * (shaded + 1) / 2

    def extract_unit_mean_aspect(self, mask_unit: np.ndarray) -> float:
        """
        Extract the circular mean aspect for a hydro unit.

        Computes the circular mean of aspect values within a unit mask, accounting
        for the circular nature of aspect angles (0° and 360° are equivalent).

        Parameters
        ----------
        mask_unit
            Boolean mask array identifying the cells of the hydro unit.

        Returns
        -------
        float
            Mean aspect in degrees (0-360).
        """
        aspect_rad = np.radians(self.aspect[mask_unit])
        circular_mean_aspect_rad = math.atan2(np.mean(np.sin(aspect_rad)),
                                              np.mean(np.cos(aspect_rad)))
        circular_mean_aspect_deg = np.degrees(circular_mean_aspect_rad)
        if circular_mean_aspect_deg < 0:
            circular_mean_aspect_deg = circular_mean_aspect_deg + 360

        return circular_mean_aspect_deg

    def extract_unit_mean_slope(self, mask_unit: np.ndarray) -> float:
        """
        Extract the mean slope for a hydro unit.

        Parameters
        ----------
        mask_unit
            Boolean mask array identifying the cells of the hydro unit.

        Returns
        -------
        float
            Mean slope in degrees, rounded to 2 decimal places.
        """
        return round(float(np.nanmean(self.slope[mask_unit])), 2)

    def extract_unit_mean_elevation(self, mask_unit: np.ndarray) -> float:
        """
        Extract the mean elevation for a hydro unit.

        Parameters
        ----------
        mask_unit
            Boolean mask array identifying the cells of the hydro unit.

        Returns
        -------
        float
            Mean elevation in meters, rounded to 2 decimal places.
        """
        return round(float(np.nanmean(self.catchment.dem_data[mask_unit])), 2)

    def extract_unit_min_elevation(self, mask_unit: np.ndarray) -> float:
        """
        Extract the minimum elevation for a hydro unit.

        Parameters
        ----------
        mask_unit
            Boolean mask array identifying the cells of the hydro unit.

        Returns
        -------
        float
            Minimum elevation in meters, rounded to 2 decimal places.
        """
        return round(float(np.nanmin(self.catchment.dem_data[mask_unit])), 2)

    def extract_unit_max_elevation(self, mask_unit: np.ndarray) -> float:
        """
        Extract the maximum elevation for a hydro unit.

        Parameters
        ----------
        mask_unit
            Boolean mask array identifying the cells of the hydro unit.

        Returns
        -------
        float
            Maximum elevation in meters, rounded to 2 decimal places.
        """
        return round(float(np.nanmax(self.catchment.dem_data[mask_unit])), 2)
