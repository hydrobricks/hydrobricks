from __future__ import annotations

from pathlib import Path

import numpy as np
import pandas as pd

import hydrobricks as hb
from hydrobricks.hydro_units import HydroUnits
from hydrobricks.preprocessing.catchment_connectivity import CatchmentConnectivity
from hydrobricks.preprocessing.catchment_discretization import CatchmentDiscretization
from hydrobricks.preprocessing.catchment_topography import CatchmentTopography
from hydrobricks.preprocessing.potential_solar_radiation import PotentialSolarRadiation

if hb.has_shapely:
    from shapely.geometry import mapping

if hb.has_rasterio:
    from rasterio.mask import mask


class Catchment:
    """
    Creation of catchment-related data

    Parameters
    ----------
    outline
        Path to the outline of the catchment.
    land_cover_types
        The land cover types of the catchment.
    land_cover_names
        The land cover names of the catchment.
    hydro_units_data
        The hydro units data of the catchment.

    Attributes
    ----------
    area : float
        The area of the catchment.
    crs : str
        The crs of the catchment outline.
    outline : shapely.geometry.Polygon
        The outline of the catchment.
    dem : rasterio.DatasetReader
        The DEM of the catchment [m].
    dem_data : np.ndarray
        The masked DEM data of the catchment.
    slope : np.ndarray
        The slope map of the catchment [degrees].
    aspect : np.ndarray
        The aspect map of the catchment.
    map_unit_ids : np.ndarray
        The unit ids as a numpy array matching the DEM extent.
    hydro_units : HydroUnits
        The hydro units of the catchment.
    """

    def __init__(
            self,
            outline: str | Path | None = None,
            land_cover_types: list[str] | None = None,
            land_cover_names: list[str] | None = None,
            hydro_units_data: pd.DataFrame | None = None):
        # Check that the required packages are installed
        if not hb.has_rasterio:
            raise ImportError("rasterio is required to do this.")
        if not hb.has_geopandas:
            raise ImportError("geopandas is required to do this.")
        if not hb.has_shapely:
            raise ImportError("shapely is required to do this.")

        # Check that the outline file exists if provided
        if outline is not None and not Path(outline).is_file():
            raise FileNotFoundError(f"File {outline} does not exist.")

        self.area = None
        self.crs = None
        self.outline = None
        self.dem = None
        self.dem_data = None
        self.attributes = {}
        self.map_unit_ids = None
        self.hydro_units = HydroUnits(
            land_cover_types,
            land_cover_names,
            hydro_units_data
        )

        self._extract_outline(outline)
        self._extract_area(outline)

        self._topography = None
        self._discretization = None
        self._connectivity = None
        self._solar_radiation = None

    def __del__(self):
        if self.dem is not None:
            self.dem.close()

    @property
    def topography(self) -> CatchmentTopography:
        if self._topography is None:
            self._topography = CatchmentTopography(self)
        return self._topography

    @property
    def discretization(self) -> CatchmentDiscretization:
        if self._discretization is None:
            self._discretization = CatchmentDiscretization(self)
        return self._discretization

    @property
    def connectivity(self) -> CatchmentConnectivity:
        if self._connectivity is None:
            self._connectivity = CatchmentConnectivity(self)
        return self._connectivity

    @property
    def solar_radiation(self) -> PotentialSolarRadiation:
        if self._solar_radiation is None:
            self._solar_radiation = PotentialSolarRadiation(self)
        return self._solar_radiation

    def extract_dem(self, raster_path: str | Path) -> bool:
        """
        Extract the DEM data for the catchment. Does not handle change in coordinates.

        Parameters
        ----------
        raster_path
            Path of the DEM raster file.

        Returns
        -------
        True if successful, False otherwise.
        """
        self.dem, self.dem_data = self._extract_raster(raster_path)
        if self.dem is None:
            return False

        return True

    def extract_attribute_raster(
            self,
            raster_path: str | Path,
            attr_name: str,
            resample_to_dem_resolution: bool = True,
            resampling: str = 'average',
            replace_nans_by_zeros: bool = True
    ) -> bool:
        """
        Extract spatial attributes (raster) for the catchment.
        Does not handle change in coordinates.

        Parameters
        ----------
        raster_path
            Path of the DEM file.
        attr_name
            Name of the attribute.
        resample_to_dem_resolution
            If True, resample the attribute to the DEM resolution.
        resampling
            Resampling method to use. Default is 'average'. Options are listed under
            https://rasterio.readthedocs.io/en/stable/api/rasterio.enums.html#rasterio.enums.Resampling
        replace_nans_by_zeros
            If True, replace NaN values by 0. Default is True.

        Returns
        -------
        True if successful, False otherwise.
        """
        src, data = self._extract_raster(raster_path)
        if src is None:
            return False

        if replace_nans_by_zeros:
            data[np.isnan(data)] = 0

        if resample_to_dem_resolution:
            if resampling == 'nearest':
                resampling_id = 0
            elif resampling == 'bilinear':
                resampling_id = 1
            elif resampling == 'cubic':
                resampling_id = 2
            elif resampling == 'cubic_spline':
                resampling_id = 3
            elif resampling == 'lanczos':
                resampling_id = 4
            elif resampling == 'average':
                resampling_id = 5
            elif resampling == 'mode':
                resampling_id = 6
            elif resampling == 'gauss':
                resampling_id = 7
            elif resampling == 'max':
                resampling_id = 8
            elif resampling == 'min':
                resampling_id = 9
            elif resampling == 'med':
                resampling_id = 10
            elif resampling == 'q1':
                resampling_id = 11
            elif resampling == 'q3':
                resampling_id = 12
            elif resampling == 'sum':
                resampling_id = 13
            elif resampling == 'rms':
                resampling_id = 14
            else:
                raise ValueError(f"Unknown resampling method: {resampling}")

            # Resample the attribute to the DEM resolution
            data = hb.rasterio.warp.reproject(
                data,
                destination=self.dem_data,
                src_transform=src.transform,
                dst_transform=self.dem.transform,
                src_crs=src.crs,
                dst_crs=self.dem.crs,
                resampling=resampling_id
            )[0]

            # Create a MemoryFile to store the reprojected data
            with hb.rasterio.io.MemoryFile() as memfile:
                with memfile.open(
                        driver='GTiff',
                        height=data.shape[0],
                        width=data.shape[1],
                        count=1,
                        dtype=data.dtype,
                        crs=self.dem.crs,
                        transform=self.dem.transform
                ) as dataset:
                    dataset.write(data, 1)

                # Open the dataset as a rasterio object
                src = memfile.open()

        self.attributes[attr_name] = {"src": src, "data": data}

        return True

    def get_hydro_units_nb(self) -> int:
        """
        Get the number of hydro units.

        Returns
        -------
        The number of hydro units.
        """
        if self.hydro_units is None:
            raise ValueError("No hydro units to count.")

        return len(self.hydro_units.hydro_units)

    def get_hydro_units_elevations(self) -> np.ndarray:
        """
        Get the elevation of the hydro units.

        Returns
        -------
        The elevation of the hydro units.
        """
        if self.hydro_units is None:
            raise ValueError("No hydro units to count.")

        return self.hydro_units.hydro_units.elevation

    def get_hydro_units_attributes(self) -> HydroUnits:
        """
        Extract the hydro units attributes.

        Returns
        -------
        The hydro units attributes.
        """
        if not hb.has_pyproj:
            raise ImportError("pyproj is required to do this.")

        if self.topography.slope is None or self.topography.aspect is None:
            self.topography.calculate_slope_aspect()

        if 'id' not in self.hydro_units.hydro_units.columns:
            unit_ids = np.unique(self.map_unit_ids)
            unit_ids = unit_ids[unit_ids != 0]
            self.hydro_units.add_property(('id', '-'), unit_ids, set_first=True)
        else:
            unit_ids = self.hydro_units.hydro_units['id'].values.tolist()

        res_area = []
        res_elevation = []
        res_elevation_mean = []
        res_elevation_min = []
        res_elevation_max = []
        res_slope = []
        res_aspect = []
        res_lat = []
        res_lon = []

        for _, unit_id in enumerate(unit_ids):
            mask_unit = self.map_unit_ids == unit_id

            # Compute the area of the unit
            n_cells = np.count_nonzero(mask_unit)
            res_area.append(round(n_cells * self.dem.res[0] * self.dem.res[1], 2))

            # Compute the mean elevation of the unit
            res_elevation_mean.append(
                self.topography.extract_unit_mean_elevation(mask_unit))
            if not self.hydro_units.has('elevation'):
                res_elevation.append(
                    self.topography.extract_unit_mean_elevation(mask_unit))
                res_elevation_min.append(
                    self.topography.extract_unit_min_elevation(mask_unit))
                res_elevation_max.append(
                    self.topography.extract_unit_max_elevation(mask_unit))

            # Compute the slope and aspect of the unit
            res_slope.append(self.topography.extract_unit_mean_slope(mask_unit))
            res_aspect.append(self.topography.extract_unit_mean_aspect(mask_unit))

            # Calculate the mean unit coordinates in lat/lon
            lat, lon = self.extract_unit_mean_lat_lon(mask_unit)
            res_lat.append(lat)
            res_lon.append(lon)

        if not self.hydro_units.has('elevation'):
            self.hydro_units.add_property(('elevation', 'm'), res_elevation)
            self.hydro_units.add_property(('elevation_min', 'm'), res_elevation_min)
            self.hydro_units.add_property(('elevation_max', 'm'), res_elevation_max)

        self.hydro_units.add_property(('elevation_mean', 'm'), res_elevation_mean)
        self.hydro_units.add_property(('area', 'm2'), res_area)
        self.hydro_units.add_property(('slope', 'deg'), res_slope)
        self.hydro_units.add_property(('aspect', 'deg'), res_aspect)
        self.hydro_units.add_property(('latitude', 'deg'), res_lat)
        self.hydro_units.add_property(('longitude', 'deg'), res_lon)

        self.hydro_units.check_land_cover_fractions_not_empty()

        self.area = sum(res_area)

        return self.hydro_units

    def save_hydro_units_to_csv(self, path: str | Path):
        """
        Save the hydro units to a csv file.

        Parameters
        ----------
        path
            Path to the output file.
        """
        if self.hydro_units is None:
            raise ValueError("No hydro units to save.")

        self.hydro_units.save_to_csv(path)

    def load_hydro_units_from_csv(self, path: str | Path):
        """
        Load hydro units from a csv file.

        Parameters
        ----------
        path
            Path to the csv file.
        """
        if not hb.has_geopandas:
            raise ImportError("geopandas is required to do this.")

        self.hydro_units.load_from_csv(path)

    def save_unit_ids_raster(
            self,
            output_path: str | Path,
            output_filename: str = 'unit_ids.tif'
    ):
        """
        Save the unit ids raster to a file.

        Parameters
        ----------
        output_path
            Path to the output file.
        output_filename
            Name of the output file. Default is 'unit_ids.tif'.
        """
        if self.map_unit_ids is None:
            raise ValueError("No unit ids raster to save.")

        full_path = Path(output_path) / output_filename

        # Create the profile
        profile = self.dem.profile
        profile.update(
            dtype=hb.rasterio.uint16,
            count=1,
            compress='lzw',
            nodata=0
        )

        with hb.rasterio.open(full_path, 'w', **profile) as dst:
            dst.write(self.map_unit_ids, 1)

    def load_unit_ids_from_raster(
            self,
            path: str,
            filename: str = 'unit_ids.tif'
    ):
        """
        Load hydro units from a raster file.

        Parameters
        ----------
        path
            Path to the directory containing the raster file. If the path is a file,
            it will be used as the full path.
        filename
            Name of the raster file. Default is 'unit_ids.tif'.
        """
        if not hb.has_rasterio:
            raise ImportError("rasterio is required to do this.")

        # If path is a file, use it directly
        if Path(path).is_file():
            full_path = Path(path)
        else:
            full_path = Path(path) / filename

        with hb.rasterio.open(full_path) as src:
            self._check_crs(src)
            if self.outline is not None:
                geoms = [mapping(polygon) for polygon in self.outline]
                self.map_unit_ids, _ = mask(src, geoms, crop=False)
            else:
                self.map_unit_ids = src.read(1)
            self.map_unit_ids[self.map_unit_ids == src.nodata] = 0

            if len(self.map_unit_ids.shape) == 3:
                self.map_unit_ids = self.map_unit_ids[0]

            self.map_unit_ids = self.map_unit_ids.astype(hb.rasterio.uint16)

    def get_dem_x_resolution(self) -> float:
        """
        Get the DEM x resolution.

        Returns
        -------
        The DEM x resolution.
        """
        return abs(self.dem.transform[0])

    def get_dem_y_resolution(self) -> float:
        """
        Get the DEM y resolution.

        Returns
        -------
        The DEM y resolution.
        """
        return abs(self.dem.transform[4])

    def get_attribute_raster_x_resolution(self, attr_name: str = "dem") -> float:
        """
        Get the given attribute raster x resolution.

        Parameters
        ----------
        attr_name
            Name of the attribute.

        Returns
        -------
        The attribute raster x resolution.
        """
        if attr_name not in self.attributes.keys():
            raise ValueError(f"Attribute raster {attr_name} not found.")

        return abs(self.attributes[attr_name].src.transform[0])

    def get_attribute_raster_y_resolution(self, attr_name: str = "dem") -> float:
        """
        Get the given attribute raster y resolution.

        Parameters
        ----------
        attr_name
            Name of the attribute.

        Returns
        -------
        The attribute raster y resolution.
        """
        if attr_name not in self.attributes.keys():
            raise ValueError(f"Attribute raster {attr_name} not found.")

        return abs(self.attributes[attr_name].src.transform[4])

    def get_dem_pixel_area(self) -> float:
        """
        Get the DEM pixel area.

        Returns
        -------
        The DEM pixel area.
        """
        return self.get_dem_x_resolution() * self.get_dem_y_resolution()

    def get_dem_mean_lat_lon(self) -> tuple[float, float]:
        # Central coordinates of the catchment
        mean_x = self.dem.bounds[0] + (self.dem.bounds[2] - self.dem.bounds[0]) / 2
        if self.dem.bounds[3] > self.dem.bounds[1]:
            mean_y = self.dem.bounds[1] + (self.dem.bounds[3] - self.dem.bounds[1]) / 2
        else:
            mean_y = self.dem.bounds[3] + (self.dem.bounds[1] - self.dem.bounds[3]) / 2

        # Get the mean coordinates of the unit in lat/lon
        transformer = hb.pyproj.Transformer.from_crs(self.crs, 4326, always_xy=True)
        mean_lon, mean_lat = transformer.transform(mean_x, mean_y)

        return mean_lat, mean_lon

    def create_dem_pixel_geometry(self, i: int, j: int) -> hb.shapely.geometry.Polygon:
        """
        Create a shapely geometry of the DEM pixel.

        Parameters
        ----------
        i
            The row of the pixel.
        j
            The column of the pixel.

        Returns
        -------
        The shapely geometry of the pixel.
        """
        if not hb.has_shapely:
            raise ImportError("shapely is required to do this.")

        xy = self.dem.xy(i, j)
        x_size = self.get_dem_x_resolution()
        y_size = self.get_dem_y_resolution()
        x_min = xy[0] - x_size / 2
        y_min = xy[1] - y_size / 2
        x_max = xy[0] + x_size / 2
        y_max = xy[1] + y_size / 2

        return hb.shapely.geometry.box(x_min, y_min, x_max, y_max)

    def initialize_area_from_land_cover_change(
            self,
            land_cover_name: str,
            land_cover_change: pd.DataFrame
    ):
        """
        Initialize the HydroUnits cover area from a land cover change object.

        Parameters
        ----------
        land_cover_name
            The name of the land cover to initialize.
        land_cover_change
            The land cover change dataframe.
        """
        if self.map_unit_ids is None:
            raise ValueError("No hydro units to initialize.")

        if land_cover_name == 'ground':
            raise ValueError("You should not initialize the 'ground' land cover type.")

        self.hydro_units.initialize_from_land_cover_change(
            land_cover_name,
            land_cover_change
        )

    def initialize_land_cover_fractions(self):
        self.hydro_units.initialize_land_cover_fractions()

    def discretize_by(self, *args, **kwargs):
        """
        Call the discretize_by method of the Discretization class.
        """
        self.discretization.discretize_by(*args, **kwargs)

    def create_elevation_bands(self, *args, **kwargs):
        """
        Call the create_elevation_bands method of the Discretization class.
        """
        self.discretization.create_elevation_bands(*args, **kwargs)

    def get_mean_elevation(self) -> float:
        """
        Call the get_mean_elevation method of the Topography class.
        """
        return self.topography.get_mean_elevation()

    def resample_dem_and_calculate_slope_aspect(
            self, *args, **kwargs
    ) -> tuple[np.ndarray, np.ndarray, np.ndarray, np.ndarray]:
        """
        Call the resample_dem_and_calculate_slope_aspect method of the Topography class.
        """
        return self.topography.resample_dem_and_calculate_slope_aspect(*args, **kwargs)

    def calculate_slope_aspect(self):
        """
        Call the calculate_slope_aspect method of the Topography class.
        """
        self.topography.calculate_slope_aspect()

    def get_hillshade(self, *args, **kwargs) -> np.ndarray:
        """
        Call the get_hillshade method of the Topography class.
        """
        return self.topography.get_hillshade(*args, **kwargs)

    def calculate_connectivity(self, *args, **kwargs) -> pd.DataFrame:
        """
        Call the calculate_connectivity method of the Connectivity class.
        """
        return self.connectivity.calculate(*args, **kwargs)

    def calculate_daily_potential_radiation(self, *args, **kwargs):
        """
        Call the calculate_daily_potential_radiation method of the
        PotentialSolarRadiation class.
        """
        self.solar_radiation.calculate_daily_potential_radiation(*args, **kwargs)

    @staticmethod
    def calculate_cast_shadows(*args, **kwargs) -> np.ndarray:
        """
        Call the calculate_cast_shadows method of the PotentialSolarRadiation class.
        """
        return PotentialSolarRadiation.calculate_cast_shadows(*args, **kwargs)

    def load_mean_annual_radiation_raster(self, *args, **kwargs):
        """
        Call the load_mean_annual_radiation_raster method of the
        PotentialSolarRadiation class.
        """
        self.solar_radiation.load_mean_annual_radiation_raster(*args, **kwargs)

    def upscale_and_save_mean_annual_radiation_rasters(self, *args, **kwargs):
        """
        Call the upscale_and_save_mean_annual_radiation_rasters method of the
        PotentialSolarRadiation class.
        """
        self.solar_radiation.upscale_and_save_mean_annual_radiation_rasters(
            *args, **kwargs
        )

    @staticmethod
    def get_solar_declination_rad(*args, **kwargs) -> float:
        """
        Call the get_solar_declination_rad method of the PotentialSolarRadiation class.
        """
        return PotentialSolarRadiation.get_solar_declination_rad(*args, **kwargs)

    @staticmethod
    def get_solar_hour_angle_limit(*args, **kwargs) -> float | np.ndarray:
        """
        Call the get_solar_hour_angle_limit method of the PotentialSolarRadiation class.
        """
        return PotentialSolarRadiation.get_solar_hour_angle_limit(*args, **kwargs)

    @staticmethod
    def get_solar_zenith(*args, **kwargs) -> float | np.ndarray:
        """
        Call the get_solar_zenith method of the PotentialSolarRadiation class.
        """
        return PotentialSolarRadiation.get_solar_zenith(*args, **kwargs)

    @staticmethod
    def get_solar_azimuth_to_south(*args, **kwargs) -> np.ndarray:
        """
        Call the get_solar_azimuth_to_south method of the PotentialSolarRadiation class.
        """
        return PotentialSolarRadiation.get_solar_azimuth_to_south(*args, **kwargs)

    @staticmethod
    def get_solar_azimuth_to_north(*args, **kwargs) -> float | np.ndarray:
        """
        Call the get_solar_azimuth_to_north method of the PotentialSolarRadiation class.
        """
        return PotentialSolarRadiation.get_solar_azimuth_to_north(*args, **kwargs)

    def extract_unit_mean_lat_lon(self, mask_unit: np.ndarray) -> tuple[float, float]:
        # Get rows and cols of the unit
        rows, cols = np.where(mask_unit)

        # Get coordinates of the unit
        xs, ys = hb.rasterio.transform.xy(self.dem.transform, list(rows), list(cols))

        # Get the mean coordinates of the unit
        mean_x = np.nanmean(xs)
        mean_y = np.nanmean(ys)

        # Get the mean coordinates of the unit in lat/lon
        transformer = hb.pyproj.Transformer.from_crs(self.crs, 4326, always_xy=True)
        mean_lon, mean_lat = transformer.transform(mean_x, mean_y)

        return mean_lat, mean_lon

    def _extract_area(self, outline: hb.shapely.geometry.Polygon):
        # The outline has to be given in meters.
        if not outline:
            return
        shapefile = hb.gpd.read_file(outline)
        self._check_crs(shapefile)
        area = hb.utils.compute_area(shapefile)
        self.area = area

    def _extract_outline(self, outline: hb.shapely.geometry.Polygon):
        if not outline:
            return
        shapefile = hb.gpd.read_file(outline)
        self._check_crs(shapefile)
        geoms = shapefile.geometry.values
        self.outline = geoms

    def _extract_raster(
            self,
            raster_path: str | Path
    ) -> (hb.rasterio.DatasetReader, np.ndarray):
        """
        Extract raster data for the catchment. Does not handle change in coordinates.

        Parameters
        ----------
        raster_path
            Path of the DEM file.

        Returns
        -------
        src
            The rasterio dataset reader object.
        masked_data
            The masked raster data.
        """
        if not Path(raster_path).is_file():
            raise FileNotFoundError(f"File {raster_path} does not exist.")

        if not hb.has_rasterio:
            raise ImportError("rasterio is required to do this.")
        if not hb.has_shapely:
            raise ImportError("shapely is required to do this.")

        src = hb.rasterio.open(raster_path)
        self._check_crs(src)

        if self.outline is not None:
            geoms = [mapping(polygon) for polygon in self.outline]
            masked_data, _ = mask(src, geoms, crop=False)
        else:
            masked_data = src.read(1)

        masked_data[masked_data == src.nodata] = np.nan
        if len(masked_data.shape) == 3:
            masked_data = masked_data[0]

        return src, masked_data

    def _check_crs(self, data: hb.rasterio.DatasetReader | hb.gpd.GeoDataFrame):
        data_crs = self._get_crs_from_file(data)
        if self.crs is None:
            self.crs = data_crs
        else:
            if self.crs != data_crs:
                raise ValueError("The CRS of the data does not match the CRS of the "
                                 "catchment.")

    @staticmethod
    def _get_crs_from_file(data: hb.rasterio.DatasetReader | hb.gpd.GeoDataFrame):
        if isinstance(data, hb.rasterio.DatasetReader):
            return data.crs
        elif isinstance(data, hb.gpd.GeoDataFrame):
            return data.crs
        else:
            raise ValueError("Unknown data format.")

    # Copy the docstring from attached classes
    discretize_by.__doc__ = CatchmentDiscretization.discretize_by.__doc__
    create_elevation_bands.__doc__ = CatchmentDiscretization.create_elevation_bands.__doc__
    get_mean_elevation.__doc__ = CatchmentTopography.get_mean_elevation.__doc__
    resample_dem_and_calculate_slope_aspect.__doc__ = CatchmentTopography.resample_dem_and_calculate_slope_aspect.__doc__
    calculate_slope_aspect.__doc__ = CatchmentTopography.calculate_slope_aspect.__doc__
    get_hillshade.__doc__ = CatchmentTopography.get_hillshade.__doc__
    calculate_connectivity.__doc__ = CatchmentConnectivity.calculate.__doc__
    calculate_daily_potential_radiation.__doc__ = PotentialSolarRadiation.calculate_daily_potential_radiation.__doc__
    calculate_cast_shadows.__doc__ = PotentialSolarRadiation.calculate_cast_shadows.__doc__
    load_mean_annual_radiation_raster.__doc__ = PotentialSolarRadiation.load_mean_annual_radiation_raster.__doc__
    upscale_and_save_mean_annual_radiation_rasters.__doc__ = PotentialSolarRadiation.upscale_and_save_mean_annual_radiation_rasters.__doc__
    get_solar_declination_rad.__doc__ = PotentialSolarRadiation.get_solar_declination_rad.__doc__
    get_solar_hour_angle_limit.__doc__ = PotentialSolarRadiation.get_solar_hour_angle_limit.__doc__
    get_solar_zenith.__doc__ = PotentialSolarRadiation.get_solar_zenith.__doc__
    get_solar_azimuth_to_south.__doc__ = PotentialSolarRadiation.get_solar_azimuth_to_south.__doc__
    get_solar_azimuth_to_north.__doc__ = PotentialSolarRadiation.get_solar_azimuth_to_north.__doc__
