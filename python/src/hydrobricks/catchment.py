import itertools
import math
import warnings
from pathlib import Path

import numpy as np

import hydrobricks as hb
from hydrobricks.constants import (
    AIR_MOLAR_MASS,
    ES_ECCENTRICITY,
    ES_SM_AXIS,
    GRAVITY,
    R_GAS,
    SEA_ATM_PRESSURE,
    SEA_HEIGHT,
    SEA_SURFACE_TEMPERATURE,
    SOLAR_CST,
    T_LAPSE_RATE,
    TO_DEG,
    TO_RAD,
)

if hb.has_shapely:
    from shapely.geometry import mapping

if hb.has_rasterio:
    from rasterio.enums import Resampling
    from rasterio.mask import mask


class Catchment:
    """
    Creation of catchment-related data

    Parameters
    ----------
    outline : str|Path
        Path to the outline of the catchment.
    land_cover_types : list, optional
        The land cover types of the catchment.
    land_cover_names : list, optional
        The land cover names of the catchment.
    hydro_units_data : pd.DataFrame, optional
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
    masked_dem_data : np.ndarray
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

    def __init__(self, outline=None, land_cover_types=None, land_cover_names=None,
                 hydro_units_data=None):
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
        self.masked_dem_data = None
        self.slope = None
        self.aspect = None
        self.mean_annual_radiation = None
        self.map_unit_ids = None
        self.hydro_units = hb.HydroUnits(land_cover_types, land_cover_names,
                                         hydro_units_data)

        self._extract_outline(outline)
        self._extract_area(outline)

    def __del__(self):
        if self.dem is not None:
            self.dem.close()

    def extract_dem(self, dem_path) -> bool:
        """
        Extract the DEM data for the catchment. Does not handle change in coordinates.

        Parameters
        ----------
        dem_path : str|Path
            Path of the DEM file.

        Returns
        -------
        True if successful, False otherwise.
        """
        if not Path(dem_path).is_file():
            raise FileNotFoundError(f"File {dem_path} does not exist.")

        if not hb.has_rasterio:
            raise ImportError("rasterio is required to do this.")
        if not hb.has_shapely:
            raise ImportError("shapely is required to do this.")

        try:
            src = hb.rasterio.open(dem_path)
            self._check_crs(src)
            self.dem = src

            if self.outline is not None:
                geoms = [mapping(polygon) for polygon in self.outline]
                self.masked_dem_data, _ = mask(src, geoms, crop=False)
            else:
                self.masked_dem_data = src.read(1)

            self.masked_dem_data[self.masked_dem_data == src.nodata] = np.nan
            if len(self.masked_dem_data.shape) == 3:
                self.masked_dem_data = self.masked_dem_data[0]
            return True
        except ValueError as e:
            print(e)
            return False
        except Exception as e:
            print(e)
            return False

    def create_elevation_bands(self, method='equal_intervals', number=100, distance=50,
                               min_elevation=None, max_elevation=None):
        """
        Construction of the elevation bands based on the chosen method.

        Parameters
        ----------
        method : str
            The method to build the elevation bands:
            'equal_intervals' = fixed contour intervals (provide the 'distance' parameter)
            'quantiles' = quantiles of the catchment area (same surface;
            provide the 'number' parameter)
        number : int, optional
            Number of bands to create when using the 'quantiles' method.
        distance : int, optional
            Distance (m) between the contour lines when using the 'equal_intervals' method.
        min_elevation : int, optional
            Minimum elevation of the elevation bands (to homogenize between runs).
        max_elevation : int, optional
            Maximum elevation of the elevation bands (to homogenize between runs).
        """
        self.discretize_by('elevation', method, number, distance,
                           min_elevation, max_elevation)

    def discretize_by(self, criteria, elevation_method='equal_intervals', elevation_number=100,
                      elevation_distance=100, min_elevation=None, max_elevation=None,
                      slope_method='equal_intervals', slope_number=6, slope_distance=15,
                      min_slope=0, max_slope=90, radiation_method='equal_intervals',
                      radiation_number=5, radiation_distance=50, min_radiation=None,
                      max_radiation=None):
        """
        Construction of the elevation bands based on the chosen method.

        Parameters
        ----------
        criteria : str|list
            The criteria to use to discretize the catchment (can be combined):
            'elevation' = elevation bands
            'aspect' = aspect according to the cardinal directions (4 classes)
            'radiation' = potential radiation (Hock, 1999)
            'slope' = slope in degrees
        elevation_method : str
            The method to build the elevation bands:
            'equal_intervals' = fixed contour intervals (provide the 'elevation_distance' parameter)
            'quantiles' = quantiles of the catchment area (same surface;
            provide the 'elevation_number' parameter)
        elevation_number : int, optional
            Number of elevation bands to create when using the 'quantiles' method.
        elevation_distance : int, optional
            Distance (m) between the contour lines when using the 'equal_intervals' method.
        min_elevation : int, optional
            Minimum elevation of the elevation bands (to homogenize between runs).
        max_elevation : int, optional
            Maximum elevation of the elevation bands (to homogenize between runs).
        slope_method : str
            The method to build the slope categories:
            'equal_intervals' = fixed slope intervals (provide the 'slope_distance' parameter)
            'quantiles' = quantiles of the catchment area (same surface;
            provide the 'slope_number' parameter)
        slope_number : int, optional
            Number of slope bands to create when using the 'quantiles' method.
        slope_distance : int, optional
            Distance (degrees) between the slope lines when using the 'equal_intervals' method.
        min_slope : int, optional
            Minimum slope of the slope bands (to homogenize between runs).
        max_slope : int, optional
            Maximum slope of the slope bands (to homogenize between runs).
        radiation_method : str
            The method to build the radiation categories:
            'equal_intervals' = fixed radiation intervals (provide the 'radiation_distance' parameter)
            'quantiles' = quantiles of the catchment area (same surface;
            provide the 'radiation_number' parameter)
        radiation_number : int, optional
            Number of radiation bands to create when using the 'quantiles' method.
        radiation_distance : int, optional
            Distance (W/m2) between the radiation lines for the 'equal_intervals' method.
        min_radiation : int, optional
            Minimum radiation of the radiation bands (to homogenize between runs).
        max_radiation : int, optional
            Maximum radiation of the radiation bands (to homogenize between runs).
        """
        if not hb.has_pyproj:
            raise ImportError("pyproj is required to do this.")

        if isinstance(criteria, str):
            criteria = [criteria]

        if self.slope is None or self.aspect is None:
            self.calculate_slope_aspect()
        if 'radiation' in criteria and self.mean_annual_radiation is None:
            raise RuntimeError("Please first compute the radiation.")

        self.map_unit_ids = np.zeros(self.masked_dem_data.shape)

        # Create a dict to store the criteria
        criteria_dict = {}

        if 'elevation' in criteria:
            criteria_dict['elevation'] = []
            if elevation_method == 'equal_intervals':
                dist = elevation_distance
                if min_elevation is None:
                    min_elevation = np.nanmin(self.masked_dem_data)
                    min_elevation = min_elevation - (min_elevation % dist)
                if max_elevation is None:
                    max_elevation = np.nanmax(self.masked_dem_data)
                    max_elevation = max_elevation + (dist - max_elevation % dist)
                elevations = np.arange(min_elevation, max_elevation + dist, dist)
                for i in range(len(elevations) - 1):
                    criteria_dict['elevation'].append(elevations[i:i + 2])
            elif elevation_method == 'quantiles':
                elevations = np.nanquantile(
                    self.masked_dem_data, np.linspace(0, 1, num=elevation_number))
                for i in range(len(elevations) - 1):
                    criteria_dict['elevation'].append(elevations[i:i + 2])
            else:
                raise ValueError("Unknown elevation band creation method.")

        if 'slope' in criteria:
            criteria_dict['slope'] = []
            if slope_method == 'equal_intervals':
                dist = slope_distance
                if min_slope is None:
                    min_slope = np.nanmin(self.slope)
                    min_slope = min_slope - (min_slope % dist)
                if max_slope is None:
                    max_slope = np.nanmax(self.slope)
                    max_slope = max_slope + (dist - max_slope % dist)
                slopes = np.arange(min_slope, max_slope + dist, dist)
                for i in range(len(slopes) - 1):
                    criteria_dict['slope'].append(slopes[i:i + 2])
            elif slope_method == 'quantiles':
                slopes = np.nanquantile(self.slope, np.linspace(0, 1, num=slope_number))
                for i in range(len(slopes) - 1):
                    criteria_dict['slope'].append(slopes[i:i + 2])
            else:
                raise ValueError("Unknown slope band creation method.")

        if 'aspect' in criteria:
            criteria_dict['aspect'] = ['N', 'E', 'S', 'W']

        if 'radiation' in criteria:
            if self.mean_annual_radiation is None:
                raise ValueError("No radiation data available. "
                                 "Compute the radiation first")
            criteria_dict['radiation'] = []
            if radiation_method == 'equal_intervals':
                dist = radiation_distance
                if min_radiation is None:
                    min_radiation = np.nanmin(self.mean_annual_radiation)
                    min_radiation = min_radiation - (min_radiation % dist)
                if max_radiation is None:
                    max_radiation = np.nanmax(self.mean_annual_radiation)
                    max_radiation = max_radiation + (dist - max_radiation % dist)
                radiations = np.arange(min_radiation, max_radiation + dist, dist)
                for i in range(len(radiations) - 1):
                    criteria_dict['radiation'].append(radiations[i:i + 2])
            elif radiation_method == 'quantiles':
                radiations = np.nanquantile(
                    self.mean_annual_radiation, np.linspace(0, 1, num=radiation_number))
                for i in range(len(radiations) - 1):
                    criteria_dict['radiation'].append(radiations[i:i + 2])
            else:
                raise ValueError("Unknown radiation band creation method.")

        res_elevation = []
        res_elevation_min = []
        res_elevation_max = []
        res_slope = []
        res_slope_min = []
        res_slope_max = []
        res_aspect_class = []
        res_radiation = []
        res_radiation_min = []
        res_radiation_max = []

        combinations = list(itertools.product(*criteria_dict.values()))
        combinations_keys = criteria_dict.keys()

        unit_id = 1
        for i, criteria in enumerate(combinations):
            mask_unit = np.ones(self.masked_dem_data.shape, dtype=bool)
            # Mask nan values
            mask_unit[np.isnan(self.masked_dem_data)] = False

            for criterion_name, criterion in zip(combinations_keys, criteria):
                if criterion_name == 'elevation':
                    mask_elev = np.logical_and(
                        self.masked_dem_data >= criterion[0],
                        self.masked_dem_data < criterion[1])
                    mask_unit = np.logical_and(mask_unit, mask_elev)
                elif criterion_name == 'slope':
                    mask_slope = np.logical_and(
                        self.slope >= criterion[0],
                        self.slope < criterion[1])
                    mask_unit = np.logical_and(mask_unit, mask_slope)
                elif criterion_name == 'aspect':
                    if criterion == 'N':
                        mask_aspect = np.logical_or(
                            np.logical_and(self.aspect >= 315, self.aspect <= 360),
                            np.logical_and(self.aspect >= 0, self.aspect < 45))
                    elif criterion == 'E':
                        mask_aspect = np.logical_and(self.aspect >= 45,
                                                     self.aspect < 135)
                    elif criterion == 'S':
                        mask_aspect = np.logical_and(self.aspect >= 135,
                                                     self.aspect < 225)
                    elif criterion == 'W':
                        mask_aspect = np.logical_and(self.aspect >= 225,
                                                     self.aspect < 315)
                    else:
                        raise ValueError("Unknown aspect value.")
                    mask_unit = np.logical_and(mask_unit, mask_aspect)
                elif criterion_name == 'radiation':
                    mask_radi = np.logical_and(
                        self.mean_annual_radiation >= criterion[0],
                        self.mean_annual_radiation < criterion[1])
                    mask_unit = np.logical_and(mask_unit, mask_radi)

            # If the unit is empty, skip it
            if np.count_nonzero(mask_unit) == 0:
                continue

            # Check that all cells in unit_ids are 0
            assert np.count_nonzero(self.map_unit_ids[mask_unit]) == 0

            # Set the unit id
            self.map_unit_ids[mask_unit] = unit_id

            # Set the mean elevation of the unit if elevation is a criterion
            if 'elevation' in criteria_dict.keys():
                i = list(combinations_keys).index('elevation')
                elevations = criteria[i]
                res_elevation.append(round(float(np.mean(elevations)), 2))
                res_elevation_min.append(round(float(elevations[0]), 2))
                res_elevation_max.append(round(float(elevations[1]), 2))

            # Set the mean slope of the unit if slope is a criterion
            if 'slope' in criteria_dict.keys():
                i = list(combinations_keys).index('slope')
                slopes = criteria[i]
                res_slope.append(round(float(np.mean(slopes)), 2))
                res_slope_min.append(round(float(slopes[0]), 2))
                res_slope_max.append(round(float(slopes[1]), 2))

            # Get the aspect class if aspect is a criterion
            if 'aspect' in criteria_dict.keys():
                i = list(combinations_keys).index('aspect')
                res_aspect_class.append(criteria[i])

            # Get the radiation class if radiation is a criterion
            if 'radiation' in criteria_dict.keys():
                i = list(combinations_keys).index('radiation')
                radiations = criteria[i]
                res_radiation.append(round(float(np.mean(radiations)), 2))
                res_radiation_min.append(round(float(radiations[0]), 2))
                res_radiation_max.append(round(float(radiations[1]), 2))

            unit_id += 1

        self.map_unit_ids = self.map_unit_ids.astype(hb.rasterio.uint16)

        if res_elevation:
            self.hydro_units.add_property(('elevation', 'm'), res_elevation)
            self.hydro_units.add_property(('elevation_min', 'm'), res_elevation_min)
            self.hydro_units.add_property(('elevation_max', 'm'), res_elevation_max)

        if res_slope:
            self.hydro_units.add_property(('slope', 'deg'), res_slope)
            self.hydro_units.add_property(('slope_min', 'deg'), res_slope_min)
            self.hydro_units.add_property(('slope_max', 'deg'), res_slope_max)

        if res_aspect_class:
            self.hydro_units.add_property(('aspect_class', '-'), res_aspect_class)

        if res_radiation:
            self.hydro_units.add_property(('radiation', 'W/m2'), res_radiation)
            self.hydro_units.add_property(('radiation_min', 'W/m2'), res_radiation_min)
            self.hydro_units.add_property(('radiation_max', 'W/m2'), res_radiation_max)

        self._initialize_land_cover_fractions()
        self.get_hydro_units_attributes()
        self.hydro_units.populate_bounded_instance()

    def get_hydro_units_nb(self):
        """
        Get the number of hydro units.

        Returns
        -------
        The number of hydro units.
        """
        if self.hydro_units is None:
            raise ValueError("No hydro units to count.")

        return len(self.hydro_units.hydro_units)

    def get_hydro_units_elevations(self):
        """
        Get the elevation of the hydro units.

        Returns
        -------
        The elevation of the hydro units.
        """
        if self.hydro_units is None:
            raise ValueError("No hydro units to count.")

        return self.hydro_units.hydro_units.elevation

    def get_hydro_units_attributes(self):
        """
        Extract the hydro units attributes.

        Returns
        -------
        The hydro units attributes.
        """
        if not hb.has_pyproj:
            raise ImportError("pyproj is required to do this.")

        if self.slope is None or self.aspect is None:
            self.calculate_slope_aspect()

        unit_ids = np.unique(self.map_unit_ids)
        unit_ids = unit_ids[unit_ids != 0]

        if 'id' not in self.hydro_units.hydro_units.columns:
            self.hydro_units.add_property(('id', '-'), unit_ids, set_first=True)

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
            res_elevation_mean.append(self._extract_unit_mean_elevation(mask_unit))
            if not self.hydro_units.has('elevation'):
                res_elevation.append(self._extract_unit_mean_elevation(mask_unit))
                res_elevation_min.append(self._extract_unit_min_elevation(mask_unit))
                res_elevation_max.append(self._extract_unit_max_elevation(mask_unit))

            # Compute the slope and aspect of the unit
            res_slope.append(self._extract_unit_mean_slope(mask_unit))
            res_aspect.append(self._extract_unit_mean_aspect(mask_unit))

            # Calculate the mean unit coordinates in lat/lon
            lat, lon = self._extract_unit_mean_lat_lon(mask_unit)
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

    def get_mean_elevation(self):
        """
        Get the catchment mean elevation.

        Returns
        -------
        The catchment mean elevation.
        """
        return np.nanmean(self.masked_dem_data)

    def resample_dem_and_calculate_slope_aspect(self, resolution, output_path):
        """
        Resample the DEM and calculate the slope and aspect of the whole DEM.

        Parameters
        ----------
        resolution : ?float
            Desired pixel resolution.
        output_path : str|Path
            Path of the directory to save the downsampled DEM to.

        Returns
        -------
        The downsampled DEM, the masked downsampled DEM data, the slope and the aspect.
        """
        if not hb.has_rasterio:
            raise ImportError("rasterio is required to do this.")
        if not hb.has_xrspatial:
            raise ImportError("xarray-spatial is required to do this.")

        # Only resample the DEM if the resolution is different from the original
        if resolution is None or resolution == self.get_dem_x_resolution():
            if self.slope is None or self.aspect is None:
                self.calculate_slope_aspect()
            return self.dem, self.masked_dem_data, self.slope, self.aspect

        with warnings.catch_warnings():
            warnings.filterwarnings("ignore", category=UserWarning)  # pyproj
            xr_dem = hb.rxr.open_rasterio(self.dem.files[0]).drop_vars('band')[0]

        x_downscale_factor = self.get_dem_x_resolution() / resolution
        y_downscale_factor = self.get_dem_y_resolution() / resolution

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
        new_dem = hb.rasterio.open(filepath)
        if self.outline is not None:
            geoms = [mapping(polygon) for polygon in self.outline]
            new_masked_dem_data, _ = mask(new_dem, geoms, crop=False)
        else:
            new_masked_dem_data = new_dem.read(1)
        new_masked_dem_data[new_masked_dem_data == new_dem.nodata] = np.nan
        if len(new_masked_dem_data.shape) == 3:
            new_masked_dem_data = new_masked_dem_data[0]
        new_slope = hb.xrs.slope(xr_dem_downsampled, name='slope').to_numpy()
        new_aspect = hb.xrs.aspect(xr_dem_downsampled, name='aspect').to_numpy()
        xr_dem_downsampled.close()

        return new_dem, new_masked_dem_data, new_slope, new_aspect

    def calculate_slope_aspect(self):
        """
        Calculate the slope and aspect of the whole DEM.
        """
        if not hb.has_xrspatial:
            raise ImportError("xarray-spatial is required to do this.")

        with warnings.catch_warnings():
            warnings.filterwarnings("ignore", category=UserWarning)  # pyproj
            xr_dem = hb.rxr.open_rasterio(self.dem.files[0]).drop_vars('band')[0]
            self.slope = hb.xrs.slope(xr_dem, name='slope').to_numpy()
            self.aspect = hb.xrs.aspect(xr_dem, name='aspect').to_numpy()

    def get_hillshade(self, azimuth=315, altitude=45, z_factor=1):
        """
        Create a hillshade from the DEM.

        Adapted from https://github.com/royalosyin/Work-with-DEM-data-using-Python-
        from-Simple-to-Complicated/blob/master/ex07-Hillshade%20from%20a%20Digital
        %20Elevation%20Model%20(DEM).ipynb

        Parameters
        ----------
        azimuth : float
            The desired azimuth for the hillshade.
        altitude : float
            The desired sun angle altitude for the hillshade.
        z_factor : float
            The z factor to amplify the relief.

        Returns
        -------
        A numpy array containing hillshade values.
        """
        x, y = np.gradient(self.dem.read(1))
        x_pixel_size = self.get_dem_x_resolution()
        y_pixel_size = self.get_dem_y_resolution()

        if azimuth > 360.0:
            raise ValueError(
                "Azimuth value should be less than or equal to 360 degrees")

        if altitude > 90.0:
            raise ValueError(
                "Altitude value should be less than or equal to 90 degrees")

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

    @staticmethod
    def get_solar_declination_rad(day_of_year):
        """
        Compute the solar declination.

        The solar declination is the angle between the rays of the Sun and the
        plane of the Earth's equator. It represents how much the sun is
        tilted towards or away from the observer's latitude. The calculation
        involves trigonometric functions to account for the observer's latitude
        and the position of the sun in the sky.

        Parameters
        ----------
        day_of_year : int|np.array
            Day of the year (1-366).

        Returns
        -------
        The solar declination in radians
        """

        # Normalized day of the year
        # '(jd-172)' calculates the number of days that have passed since the summer
        # solstice (around June 21st), which is day 172 in a non-leap year.
        # '(360*(jd-172))/365' normalizes this count to a value representing the
        # position in the orbit of the Earth around the Sun (in degrees).
        ndy = ((360 * (day_of_year - 172)) / 365)

        # The cos(...) function is applied to the normalized day of the year. It
        # produces values between -1 and 1, representing the variation in solar
        # declination throughout the year. The constant factor 23.45 represents the
        # tilt of the Earth's axis relative to its orbital plane. This tilt causes the
        # variation in the angle of the sun's rays reaching different latitudes
        # on Earth.
        solar_declin = 23.45 * np.cos(ndy * TO_RAD) * TO_RAD

        return solar_declin

    @staticmethod
    def get_solar_hour_angle_limit(solar_declination, lat_rad):
        """
        Compute the hour angle limit value (min/max).

        The hour angle is the angular distance between the sun and the observer's
        meridian. It is typically measured in degrees. The tangent of
        solar_declin/lat_rad represents the ratio of the opposite side
        (vertical component of the sun's rays) to the adjacent side (horizontal
        component). It helps capture how much the sun/the observer's location is
        tilted north or south relative to the equator.

        Parameters
        ----------
        solar_declination : float|np.array
            Solar declination in radians.
        lat_rad : float
            Latitude in radians.

        Returns
        -------
        The limit value of the hour angle in radians.
        """

        # The negative sign is applied because the Hour Angle is negative in the
        # morning and positive in the afternoon.
        hour_angle = np.arccos(-np.tan(solar_declination) * np.tan(lat_rad))

        return hour_angle

    @staticmethod
    def get_solar_zenith(hour_angles, lat_rad, solar_declination):
        """
        Compute the solar zenith.

        The Solar zenith (IQBAL 2012) is the angle between the sun and the
        vertical (zenith) position directly above the observer. The result is
        expressed in degrees. The calculation involves trigonometric functions
        to account for the observer's latitude, the solar declination, and the
        position of the sun in the sky (represented by the Hour Angles).

        Parameters
        ----------
        hour_angles : float|np.ndarray
            Hour angle(s).
        lat_rad : float
            Latitude in radians.
        solar_declination : float
            Solar declination in radians.

        Returns
        -------
        The solar zenith in degrees.
        """
        zenith = np.arccos((np.sin(lat_rad) * np.sin(solar_declination)) +
                           (np.cos(lat_rad) * np.cos(solar_declination) *
                            np.cos(hour_angles))) * TO_DEG
        return zenith

    @staticmethod
    def get_solar_azimuth_to_south(hour_angles, lat_rad, solar_declination):
        """
        Compute the solar azimuth relative to the south.

        The solar azimuth is the angle between the sun and the observer's meridian.
        It is typically measured in degrees.
        Azimuth with negative values before solar noon and positive values with
        positive values after solar noon. Solar noon is defined by the change in sign
        of the hour angle (negative in the morning, positive in the afternoon).
        From https://www.astrolabe-science.fr/diagramme-solaire-azimut-hauteur

        Parameters
        ----------
        hour_angles : float|np.ndarray
            Array with the hour angles.
        lat_rad : float
            Latitude in radians.
        solar_declination : float
            Solar declination.

        Returns
        -------
        The solar azimuth in degrees.
        """
        convert_to_float = False
        if isinstance(hour_angles, (int, float)):
            hour_angles = np.array([hour_angles])
            convert_to_float = True

        azimuth = np.degrees(np.arctan(np.sin(hour_angles) / (
                np.sin(lat_rad) * np.cos(hour_angles) -
                np.cos(lat_rad) * np.tan(solar_declination))))
        azimuth[np.where((azimuth < 0) & (hour_angles > 0))] += 180
        azimuth[np.where((azimuth > 0) & (hour_angles < 0))] -= 180

        if convert_to_float:
            azimuth = azimuth[0]

        return azimuth

    @staticmethod
    def get_solar_azimuth_to_north(hour_angles, lat_rad, solar_declination):
        """
        Compute the solar azimuth relative to the north.
        See get_solar_azimuth_to_south() for more details.
        """
        azimuth = Catchment.get_solar_azimuth_to_south(
            hour_angles, lat_rad, solar_declination)
        azimuth += 180

        return azimuth

    @staticmethod
    def _calculate_radiation_hock_equation(elevation, atmos_transmissivity, day_of_year,
                                           zenith, incidence_angle):
        """
        Hock (2005) equation to compute the potential clear-sky direct solar
        radiation at the ice or snow surface [W/m²].

        Parameters
        ----------
        elevation : float
            Height above sea level [m]
        atmos_transmissivity : float
            Mean clear-sky atmospheric transmissivity
        day_of_year : float
            Day of the year (Julian Day)
        zenith : float
            Solar zenith for one moment during the day (IQBAL 2012)
        incidence_angle : np.array
            Angle of incidence between the normal to the grid slope and the
            solar beam

        Returns
        -------
        The potential clear-sky direct solar radiation at the ice or snow
        surface [W/m²]

        Notes
        -----
        This function is based on the R package TopoSol, authored by Matthew Olson.

        References
        ----------
        - Original R package: https://github.com/mattols/TopoSol
        """

        # True anomaly (the angle subtended at the Sun between the semi major
        # axis line and the current position)
        # Different definition here:
        # https://physics.stackexchange.com/questions/177949/earth-sun-distance-on-a-given-day-of-the-year
        theta = (365.5 / 360) * day_of_year

        # Current Sun-Earth distance (computed using the modern version of
        # Kepler's first law)
        current_se_dist = (ES_SM_AXIS * (1 - ES_ECCENTRICITY * ES_ECCENTRICITY)) / \
                          (1 + ES_ECCENTRICITY * np.cos(theta * TO_RAD))

        # Atmospheric pressure
        local_pressure = (SEA_ATM_PRESSURE * (
                1 + (T_LAPSE_RATE / SEA_SURFACE_TEMPERATURE) *
                (elevation - SEA_HEIGHT)
        ) ** ((-GRAVITY * AIR_MOLAR_MASS) / (R_GAS * T_LAPSE_RATE)))

        # Hock equation (Hock, 1999) to compute the potential
        # clear-sky direct solar radiation
        if zenith > 90 - 1e-10:
            empty_matrix = np.zeros(incidence_angle.shape)
            # Set border values to nan
            empty_matrix[0, :] = np.nan
            empty_matrix[-1, :] = np.nan
            empty_matrix[:, 0] = np.nan
            empty_matrix[:, -1] = np.nan

            return empty_matrix

        solar_radiation = (SOLAR_CST * ((ES_SM_AXIS / current_se_dist) ** 2) *
                           atmos_transmissivity **
                           (local_pressure / (SEA_ATM_PRESSURE *
                                              np.cos(zenith * TO_RAD))) *
                           np.cos(incidence_angle))

        return solar_radiation

    @staticmethod
    def _calculate_angle_of_incidence(zenith, slope, azimuth, aspect,
                                      tolerance=10 ** (-6)):
        """
        Calculate the angle of incidence.

        Parameters
        ----------
        zenith : float
            Solar zenith (IQBAL 2012), in degrees
        slope : float
            Slope of the DEM, in degrees
        azimuth : float
            Azimuth for ZSLOPE CALC, in degrees
        aspect : float
            Aspect of the DEM, in degrees
        tolerance : float
            Error tolerance in the trigonometric computations.

        Returns
        -------
        The angle of incidence.

        Notes
        -----
        This function is based on the R package TopoSol, authored by Matthew Olson.

        References
        ----------
        - Original R package: https://github.com/mattols/TopoSol
        """

        # Solar zenith and azimuth on a slope
        zenith_rad = zenith * TO_RAD
        slope_rad = slope * TO_RAD
        azimuth_rad = azimuth * TO_RAD
        aspect_rad = (aspect - 180) * TO_RAD

        cosine_term = (np.cos(zenith_rad) * np.cos(slope_rad)) + \
                      (np.sin(zenith_rad) * np.sin(slope_rad) *
                       np.cos(azimuth_rad - aspect_rad))

        if np.nanmax(np.abs(cosine_term) - 1) < tolerance:
            incidence_angle = np.arccos(np.clip(cosine_term, -1, 1))
        else:
            raise ValueError("Argument of arccos is above or below 1.")

        # Angle of incidence matrix
        incidence_angle[incidence_angle > 90 * TO_RAD] = 90 * TO_RAD

        return incidence_angle

    @staticmethod
    def calculate_cast_shadows(dem_dataset, masked_dem, zenith, azimuth):
        """
        Calculate the cast shadows.

        The approach relies on tilting the DEM to obtain a horizon matching the sun
        rays and filling the DEM. The algorithm is applied to the whole topography
        before masking the areas outside the catchment.

        Parameters
        ----------
        dem_dataset :
            DEM as read by rasterio, containing the DEM topography
        masked_dem : float
            DEM topography, masked with np.nan for the areas outside the study catchment
        zenith : float
            Solar zenith (IQBAL 2012), in degrees
        azimuth : float
            Azimuth relative to the south for ZSLOPE CALC, in degrees

        Returns
        -------
        A np.array with the cast shadows (1), the in sun areas (0),
        and the masked area (np.nan).

        Notes
        -----
        The algorithm is applied to the whole topography before masking the
        areas outside the catchment.
        """
        dem = dem_dataset.read(1)
        x_size = abs(dem_dataset.res[0])
        y_size = abs(dem_dataset.res[1])

        if x_size != y_size:
            raise ValueError("The DEM x and y resolutions must be equal "
                             "for computing the cast shadows.")

        if zenith >= 90:
            cast_shadows = np.ones(dem.shape)
            cast_shadows[np.isnan(masked_dem)] = np.nan
            return cast_shadows

        # Get mean pixel size
        pixel_size = (x_size + y_size) / 2

        # Get the x and y grids for the DEM
        xv, yv = np.indices(dem.shape)

        # Get the base arrays for the solar ray IDs and the offset distances
        if azimuth < -135:  # NNE
            ref_grid = xv
            base_ray_ids = yv[:, ::-1]
            angle = np.tan(-(azimuth + 90) * TO_RAD)
        elif azimuth < -90:  # ENE
            ref_grid = yv[:, ::-1]
            base_ray_ids = xv
            angle = np.tan((azimuth + 180) * TO_RAD)
        elif azimuth < -45:  # ESE
            ref_grid = yv[:, ::-1]
            base_ray_ids = xv[::-1, :]
            angle = np.tan(-azimuth * TO_RAD)
        elif azimuth < 0:  # SSE
            ref_grid = xv[::-1, :]
            base_ray_ids = yv[:, ::-1]
            angle = np.tan((90 + azimuth) * TO_RAD)
        elif azimuth < 45:  # SSW
            ref_grid = xv[::-1, :]
            base_ray_ids = yv
            angle = np.tan((90 - azimuth) * TO_RAD)
        elif azimuth < 90:  # WSW
            ref_grid = yv
            base_ray_ids = xv[::-1, :]
            angle = np.tan(azimuth * TO_RAD)
        elif azimuth < 135:  # WNW
            ref_grid = yv
            base_ray_ids = xv
            angle = np.tan((180 - azimuth) * TO_RAD)
        else:  # NNW
            ref_grid = xv
            base_ray_ids = yv
            angle = np.tan((azimuth - 90) * TO_RAD)

        # Computation of the solar ray paths for each pixel (rays with different IDs).
        ray_ids = base_ray_ids - ref_grid / angle  # Adding the offset (from azimuth)
        ray_ids = ray_ids - np.nanmin(ray_ids)  # Set all IDs to positive values
        ray_ids = ray_ids.astype(int) + 1  # Convert to int

        # Compute the tilted DEM
        orthogonal_distance = ref_grid + base_ray_ids / angle  # Projected distance
        tilt_heights = orthogonal_distance * pixel_size / np.tan(zenith * TO_RAD)
        tilted_dem = dem + tilt_heights  # DEM after tilt of zenith °, azimuthal dir.

        # Remapping of the solar rays on another matrix to process them.
        mapped = np.ones((np.nanmax(ray_ids) + 1, np.max(ref_grid) + 1))
        mapped = mapped * np.nan  # Mapping of the tilted DEM from solar rays ...
        mapped[ray_ids, ref_grid] = tilted_dem  # ... to row/columns.
        accumulated = np.fmax.accumulate(mapped, axis=1)
        cast_sh = (accumulated > mapped).astype(float)
        cast_shadows = cast_sh[ray_ids, ref_grid]

        # Put the mask back on (we previously needed the surrounding topography).
        cast_shadows[np.isnan(masked_dem)] = np.nan

        return cast_shadows

    def calculate_daily_potential_radiation(self, output_path, resolution=None,
                                            atmos_transmissivity=0.75,
                                            steps_per_hour=4,
                                            with_cast_shadows=True):
        """
        Compute the daily mean potential clear-sky direct solar radiation
        at the DEM surface [W/m²] using Hock (1999)'s equation.
        It is computed for each day of the year and saved in a netcdf file.

        Parameters
        ----------
        output_path : string
            Path to for created daily and annual mean potential clear-sky direct solar
            radiation files.
        resolution : float, optional
            Desired pixel resolution, default is the DEM resolution.
        atmos_transmissivity : float, optional
            Mean clear-sky atmospheric transmissivity, default is 0.75
            (value taken in Hock 1999)
        steps_per_hour : int, optional
            Number of steps per hour to compute the potential radiation, default is 4.
        with_cast_shadows : bool, optional
            If True, the cast shadows are taken into account. Default is True.

        Returns
        -------
        The daily mean potential clear-sky direct solar radiation
        at the DEM surface [W/m²]

        Notes
        -----
        This function is based on the R package TopoSol, authored by Matthew Olson.

        References
        ----------
        - Original R package: https://github.com/mattols/TopoSol
        """
        # Resample the DEM and calculate the slope and aspect
        dem, masked_dem_data, slope, aspect = (
            self.resample_dem_and_calculate_slope_aspect(resolution, output_path))
        n_rows = slope.shape[0]
        n_cols = slope.shape[1]

        # Create an array with the day of the year (Julian Day)
        day_of_year = np.arange(1, 367)

        # Get some catchment attributes
        mean_elevation = self.get_mean_elevation()
        mean_lat, _ = self._extract_unit_mean_lat_lon(self.masked_dem_data)
        lat_rad = mean_lat * TO_RAD

        # Compute the solar declination
        solar_declin = self.get_solar_declination_rad(day_of_year)

        # Compute the hour angle starting value
        ha_limit = self.get_solar_hour_angle_limit(solar_declin, lat_rad)

        # Time intervals (360°/24h = 15° per hour, divided by the # of steps / hour)
        time_interval = (15 / steps_per_hour) * TO_RAD

        # Create array for the daily potential radiation
        daily_radiation = np.full((len(day_of_year), n_rows, n_cols), np.nan)

        # Loop over the days of the year
        for i in range(len(day_of_year)):
            # Print every 10 days
            if day_of_year[i] % 10 == 0:
                print('Computing radiation for day', day_of_year[i])

            # List of hour angles throughout the day.
            ha_list = np.arange(-ha_limit[i], ha_limit[i] + time_interval,
                                time_interval)

            # Compute the zenith and azimuth
            zenith = self.get_solar_zenith(ha_list, lat_rad, solar_declin[i])
            azimuth = self.get_solar_azimuth_to_south(ha_list, lat_rad, solar_declin[i])

            # Potential radiation over the time intervals
            inter_pot_radiation = np.full((len(ha_list), n_rows, n_cols), np.nan)
            for j in range(len(zenith)):
                # Shorten computation time, because if zenith >= 90 : no irradiation.
                if zenith[j] >= 90:
                    inter_pot_radiation[j, :, :] = np.zeros_like(zenith[j])
                    continue

                incidence_angle = self._calculate_angle_of_incidence(
                    zenith[j], slope, azimuth[j], aspect)
                potential_radiation = self._calculate_radiation_hock_equation(
                    mean_elevation, atmos_transmissivity, day_of_year[i],
                    zenith[j], incidence_angle)

                # Account for cast shadows
                if with_cast_shadows:
                    cast_shadows = self.calculate_cast_shadows(
                        dem, masked_dem_data, zenith[j], azimuth[j])
                    potential_radiation = potential_radiation * (1 - cast_shadows)

                inter_pot_radiation[j, :, :] = potential_radiation.copy()

            with warnings.catch_warnings():
                # This function throws a warning for the first slides of nanmean,
                # it is normal and due to the NaN bands at the sides of the
                # slope rasters, etc.
                warnings.filterwarnings(action='ignore', message='Mean of empty slice')
                daily_radiation[i, :, :] = np.nansum(inter_pot_radiation, axis=0)
                daily_radiation[i, :, :] /= 24 * steps_per_hour

        # Mean annual potential radiation
        mean_annual_radiation = np.full((n_rows, n_cols), np.nan)
        mean_annual_radiation[:, :] = np.nanmean(daily_radiation, axis=0)
        self.upscale_and_save_mean_annual_radiation_rasters(
            mean_annual_radiation, dem, output_path)

        # Put the mask back on (we need the surrounding topography in the steps before)
        # And make sure the padding lines are also set to nans and not 0
        mean_annual_radiation[np.isnan(masked_dem_data)] = np.nan
        mean_annual_radiation[0, :] = np.nan
        mean_annual_radiation[-1, :] = np.nan
        mean_annual_radiation[:, 0] = np.nan
        mean_annual_radiation[:, -1] = np.nan

        # Save the daily potential radiation to a netcdf file
        self._save_potential_radiation_netcdf(
            daily_radiation, dem, masked_dem_data, day_of_year, output_path)

        # If DEM is the downsampled one, close it
        if dem.res[0] != self.get_dem_x_resolution():
            dem.close()

    def _save_potential_radiation_netcdf(
            self, radiation, dem, masked_dem_data, day_of_year, output_path,
            output_filename='daily_potential_radiation.nc'):
        """
        Save the potential radiation to a netcdf file.

        Parameters
        ----------
        radiation : np.ndarray
            The potential radiation.
        dem : rasterio.Dataset
            The DEM.
        masked_dem_data : np.ndarray
            The masked DEM data.
        day_of_year : np.ndarray
            The array with the days of the year.
        output_path : str
            Path to the directory to save the netcdf file to.
        output_filename : str, optional
            Name of the output file. Default is 'daily_potential_radiation.nc'.
        """
        full_path = Path(output_path) / output_filename
        print('Saving to', str(full_path), self.dem.crs)

        if not hb.has_xarray:
            raise ImportError("xarray is required to do this.")

        rows, cols = np.where(masked_dem_data)
        xs, ys = hb.rasterio.transform.xy(dem.transform, list(rows), list(cols))
        xs = np.array(xs).reshape(masked_dem_data.shape)[0, :]
        ys = np.array(ys).reshape(masked_dem_data.shape)[:, 0]

        ds = hb.xr.DataArray(radiation,
                             name='radiation',
                             dims=['day_of_year', 'y', 'x'],
                             coords={
                                 "x": xs,
                                 "y": ys,
                                 "day_of_year": day_of_year})

        with warnings.catch_warnings():
            warnings.filterwarnings("ignore", category=UserWarning)  # pyproj
            ds.rio.write_crs(self.dem.crs, inplace=True)

        ds.x.attrs["axis"] = "X"
        ds.x.attrs["long_name"] = "x coordinate of projection"
        ds.x.attrs["standard_name"] = "projection_x_coordinate"
        ds.x.attrs["units"] = "meter"

        ds.y.attrs["axis"] = "Y"
        ds.y.attrs["long_name"] = "y coordinate of projection"
        ds.y.attrs["standard_name"] = "projection_y_coordinate"
        ds.y.attrs["units"] = "meter"

        try:
            ds.to_netcdf(full_path)
            print('File successfully written.')
        except Exception as e:
            raise RuntimeError(f"Error writing to file: {e}")
        finally:
            ds.close()

    def save_hydro_units_to_csv(self, path):
        """
        Save the hydro units to a csv file.

        Parameters
        ----------
        path : str|Path
            Path to the output file.
        """
        if self.hydro_units is None:
            raise ValueError("No hydro units to save.")

        self.hydro_units.save_to_csv(path)

    def save_unit_ids_raster(self, output_path, output_filename='unit_ids.tif'):
        """
        Save the unit ids raster to a file.

        Parameters
        ----------
        output_path : str|Path
            Path to the output file.
        output_filename : str, optional
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
            nodata=0)

        with hb.rasterio.open(full_path, 'w', **profile) as dst:
            dst.write(self.map_unit_ids, 1)

    def upscale_and_save_mean_annual_radiation_rasters(
            self, mean_annual_radiation, dem, output_path,
            output_filename='annual_potential_radiation.tif'):
        """
        Save the mean annual radiation rasters (downsampled and at DEM resolution)
        to a file.

        Parameters
        ----------
        mean_annual_radiation : np.ndarray
            Downsampled mean annual radiation.
        dem : rasterio.Dataset
            The DEM at the resolution of the radiation.
        output_path : str
            Path to the output directory.
        output_filename : str, optional
            Name of the output file. Default is 'annual_potential_radiation.tif'.
        """
        # Create the profile
        profile = dem.profile

        # Define the output paths
        temp_path = Path(output_path) / 'downsampled_annual_potential_radiation.tif'
        res_path = Path(output_path) / output_filename

        # If both resolutions are the same, just save the mean annual radiation
        if dem.res[0] == self.get_dem_x_resolution():
            with hb.rasterio.open(res_path, 'w', **profile) as dst:
                dst.write(mean_annual_radiation, 1)
            self.mean_annual_radiation = mean_annual_radiation
            return

        # Save a temporary file to upscale the mean annual radiation
        with hb.rasterio.open(temp_path, 'w', **profile) as dst:
            dst.write(mean_annual_radiation, 1)

        # Upscale the mean annual radiation to the DEM resolution
        with warnings.catch_warnings():
            warnings.filterwarnings("ignore", category=UserWarning)  # pyproj
            with hb.rxr.open_rasterio(temp_path).drop_vars('band')[0] as xr_dem:
                xr_dem_upscaled = xr_dem.rio.reproject(
                    xr_dem.rio.crs,
                    shape=self.dem.shape,
                    resampling=Resampling.bilinear,
                )
                xr_dem_upscaled.rio.to_raster(res_path)

        self.mean_annual_radiation = xr_dem_upscaled.to_numpy()
        xr_dem_upscaled.close()

    def load_mean_annual_radiation_raster(self, dir_path,
                                          filename='annual_potential_radiation.tif'):
        """
        Load the mean annual radiation raster.

        Parameters
        ----------
        dir_path : str
            Path to the input file directory.
        filename : str, optional
            Name of the input file. Default is 'annual_potential_radiation.tif'.
        """
        self.mean_annual_radiation = hb.rxr.open_rasterio(
            Path(dir_path) / 'annual_potential_radiation.tif').drop_vars('band')[0]

    def load_unit_ids_from_raster(self, dir_path, filename='unit_ids.tif'):
        """
        Load hydro units from a raster file.

        Parameters
        ----------
        dir_path : str
            Path to the directory containing the raster file.
        filename : str, optional
            Name of the raster file. Default is 'unit_ids.tif'.
        """
        if not hb.has_rasterio:
            raise ImportError("rasterio is required to do this.")

        full_path = Path(dir_path) / filename
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

    def get_dem_x_resolution(self):
        """
        Get the DEM x resolution.

        Returns
        -------
        The DEM x resolution.
        """
        return abs(self.dem.transform[0])

    def get_dem_y_resolution(self):
        """
        Get the DEM y resolution.

        Returns
        -------
        The DEM y resolution.
        """
        return abs(self.dem.transform[4])

    def get_dem_pixel_area(self):
        """
        Get the DEM pixel area.

        Returns
        -------
        The DEM pixel area.
        """
        return self.get_dem_x_resolution() * self.get_dem_y_resolution()

    def get_dem_mean_lat_lon(self):
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

    def create_dem_pixel_geometry(self, i, j):
        """
        Create a shapely geometry of the DEM pixel.

        Parameters
        ----------
        i : int
            The row of the pixel.
        j : int
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

    def initialize_area_from_land_cover_change(self, land_cover_name,
                                               land_cover_change):
        """
        Initialize the HydroUnits cover area from a land cover change object.

        Parameters
        ----------
        land_cover_name : str
            The name of the land cover to initialize.
        land_cover_change : pd.DataFrame
            The land cover change dataframe.
        """
        if self.map_unit_ids is None:
            raise ValueError("No hydro units to initialize.")

        if land_cover_name == 'ground':
            raise ValueError("You should not initialize the 'ground' land cover type.")

        self.hydro_units.initialize_from_land_cover_change(
            land_cover_name, land_cover_change)

    def _initialize_land_cover_fractions(self):
        self.hydro_units.initialize_land_cover_fractions()

    def _extract_area(self, outline):
        # The outline has to be given in meters.
        if not outline:
            return
        shapefile = hb.gpd.read_file(outline)
        self._check_crs(shapefile)
        area = hb.utils.compute_area(shapefile)
        self.area = area

    def _extract_outline(self, outline):
        if not outline:
            return
        shapefile = hb.gpd.read_file(outline)
        self._check_crs(shapefile)
        geoms = shapefile.geometry.values
        self.outline = geoms

    def _extract_unit_mean_aspect(self, mask_unit):
        aspect_rad = np.radians(self.aspect[mask_unit])
        circular_mean_aspect_rad = math.atan2(np.mean(np.sin(aspect_rad)),
                                              np.mean(np.cos(aspect_rad)))
        circular_mean_aspect_deg = np.degrees(circular_mean_aspect_rad)
        if circular_mean_aspect_deg < 0:
            circular_mean_aspect_deg = circular_mean_aspect_deg + 360

        return circular_mean_aspect_deg

    def _extract_unit_mean_slope(self, mask_unit):
        return round(float(np.nanmean(self.slope[mask_unit])), 2)

    def _extract_unit_mean_elevation(self, mask_unit):
        return round(float(np.nanmean(self.masked_dem_data[mask_unit])), 2)

    def _extract_unit_min_elevation(self, mask_unit):
        return round(float(np.nanmin(self.masked_dem_data[mask_unit])), 2)

    def _extract_unit_max_elevation(self, mask_unit):
        return round(float(np.nanmax(self.masked_dem_data[mask_unit])), 2)

    def _extract_unit_mean_lat_lon(self, mask_unit):
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

    def _check_crs(self, data):
        data_crs = self._get_crs_from_file(data)
        if self.crs is None:
            self.crs = data_crs
        else:
            if self.crs != data_crs:
                raise ValueError("The CRS of the data does not match the CRS of the "
                                 "catchment.")

    @staticmethod
    def _get_crs_from_file(data):
        if isinstance(data, hb.rasterio.DatasetReader):
            return data.crs
        elif isinstance(data, hb.gpd.GeoDataFrame):
            return data.crs
        else:
            raise ValueError("Unknown data format.")
