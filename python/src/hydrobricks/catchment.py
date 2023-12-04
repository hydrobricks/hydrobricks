import itertools
import math
import pathlib
import warnings

import numpy as np
import pandas as pd

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
        The DEM of the catchment.
    masked_dem_data : np.ndarray
        The masked DEM data of the catchment.
    slope : np.ndarray
        The slope map of the catchment.
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
        if outline is not None and not pathlib.Path(outline).is_file():
            raise FileNotFoundError(f"File {outline} does not exist.")

        self.area = None
        self.crs = None
        self.outline = None
        self.dem = None
        self.masked_dem_data = None
        self.slope = None
        self.aspect = None
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
        if not pathlib.Path(dem_path).is_file():
            raise FileNotFoundError(f"File {dem_path} does not exist.")

        try:
            geoms = [mapping(polygon) for polygon in self.outline]
            src = hb.rasterio.open(dem_path)
            self._check_crs(src)
            self.dem = src
            self.masked_dem_data, _ = mask(src, geoms, crop=False)
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

    def create_elevation_bands(self, method='isohypse', number=100, distance=50,
                               min_elevation=None, max_elevation=None):
        """
        Construction of the elevation bands based on the chosen method.

        Parameters
        ----------
        method : str
            The method to build the elevation bands:
            'isohypse' = fixed contour intervals (provide the 'distance' parameter)
            'quantiles' = quantiles of the catchment area (same surface;
            provide the 'number' parameter)
        number : int, optional
            Number of bands to create when using the 'quantiles' method.
        distance : int, optional
            Distance (m) between the contour lines when using the 'isohypse' method.
        min_elevation : int, optional
            Minimum elevation of the elevation bands (to homogenize between runs).
        max_elevation : int, optional
            Maximum elevation of the elevation bands (to homogenize between runs).
        """
        self.discretize_by('elevation', method, number, distance,
                           min_elevation, max_elevation)

    def discretize_by(self, criteria, elevation_method='isohypse', elevation_number=100,
                      elevation_distance=50, min_elevation=None, max_elevation=None):
        """
        Construction of the elevation bands based on the chosen method.

        Parameters
        ----------
        criteria : str|list
            The criteria to use to discretize the catchment (can be combined):
            'elevation' = elevation bands
            'aspect' = aspect according to the cardinal directions (4 classes)
        elevation_method : str
            The method to build the elevation bands:
            'isohypse' = fixed contour intervals (provide the 'distance' parameter)
            'quantiles' = quantiles of the catchment area (same surface;
            provide the 'number' parameter)
        elevation_number : int, optional
            Number of elevation bands to create when using the 'quantiles' method.
        elevation_distance : int, optional
            Distance (m) between the contour lines when using the 'isohypse' method.
        min_elevation : int, optional
            Minimum elevation of the elevation bands (to homogenize between runs).
        max_elevation : int, optional
            Maximum elevation of the elevation bands (to homogenize between runs).
        """
        if not hb.has_pyproj:
            raise ImportError("pyproj is required to do this.")

        if isinstance(criteria, str):
            criteria = [criteria]

        if self.slope is None or self.aspect is None:
            self.calculate_slope_aspect()

        self.map_unit_ids = np.zeros(self.masked_dem_data.shape)

        # Create a dict to store the criteria
        criteria_dict = {}

        if 'elevation' in criteria:
            criteria_dict['elevation'] = []
            if elevation_method == 'isohypse':
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

        if 'aspect' in criteria:
            criteria_dict['aspect'] = ['N', 'E', 'S', 'W']

        res_elevation = []
        res_elevation_min = []
        res_elevation_max = []
        res_aspect_class = []

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

            # Get the aspect class if aspect is a criterion
            if 'aspect' in criteria_dict.keys():
                i = list(combinations_keys).index('aspect')
                res_aspect_class.append(criteria[i])

            unit_id += 1

        self.map_unit_ids = self.map_unit_ids.astype(hb.rasterio.uint16)

        if res_elevation:
            self.hydro_units.add_property(('elevation', 'm'), res_elevation)
            self.hydro_units.add_property(('elevation_min', 'm'), res_elevation_min)
            self.hydro_units.add_property(('elevation_max', 'm'), res_elevation_max)

        if res_aspect_class:
            self.hydro_units.add_property(('aspect_class', '-'), res_aspect_class)

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

    @staticmethod
    def _calculate_hock_equation(local_height, atm_tra, jd, zenith,
                                 incidence_angle):
        """
        Hock (2005) equation to compute the potential clear-sky direct solar
        radiation at the ice or snow surface [W/m²].

        Parameters
        ----------
        local_height : float
            Height above sea level [m]
        atm_tra : float
            Mean clear-sky atmospheric transmissivity
        jd : float
            Julian day
        zenith : float
            Solar zenith for one moment during the day (IQBAL 2012)
        incidence_angle : float
            Angle of incidence between the normal to the grid slope and the
            solar beam

        Returns
        -------
        The potential clear-sky direct solar radiation at the ice or snow
        surface [W/m²]
        """

        # True anomaly (the angle subtended at the Sun between the semi major
        # axis line and the current position)
        # DIFFERENT definition here:
        # https://physics.stackexchange.com/questions/177949/earth-sun-distance-on-a-given-day-of-the-year
        theta = (365.5 / 360) * jd

        # Current Sun-Earth distance (computed using the modern version of
        # Kepler's first law)
        current_se_dist = (ES_SM_AXIS * (1 - ES_ECCENTRICITY * ES_ECCENTRICITY)) / \
                          (1 + ES_ECCENTRICITY * np.cos(theta * TO_RAD))

        # Atmospheric pressure
        local_pressure = (SEA_ATM_PRESSURE * (
                    1 + (T_LAPSE_RATE / SEA_SURFACE_TEMPERATURE) * (
                        local_height - SEA_HEIGHT)) **
                          ((-GRAVITY * AIR_MOLAR_MASS) / (R_GAS * T_LAPSE_RATE)))

        # Hock equation (Hock, 1999) to compute the potential
        # clear-sky direct solar radiation
        solar_radiation = (SOLAR_CST * ((ES_SM_AXIS / current_se_dist) ** 2) *
                           atm_tra ** (local_pressure / (SEA_ATM_PRESSURE *
                                                         np.cos(zenith * TO_RAD))) *
                           np.cos(incidence_angle))

        return solar_radiation

    @staticmethod
    def _calculate_angle_of_incidence(zenith, slope, azimuth, aspect):
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

        Returns
        -------
        The angle of incidence.
        """

        # Solar zenith and azimuth on a slope
        zenith_rad = zenith * TO_RAD
        slope_rad = slope * TO_RAD
        azimuth_rad = azimuth * TO_RAD
        aspect_rad = (aspect - 180) * TO_RAD
        incidence_angle = np.arccos((np.cos(zenith_rad) * np.cos(slope_rad)) +
                                    (np.sin(zenith_rad) * np.sin(slope_rad) *
                                     np.cos(azimuth_rad - aspect_rad)))

        # Angle of incidence matrix
        incidence_angle[incidence_angle > 90 * TO_RAD] = 90 * TO_RAD

        return incidence_angle

    def calculate_daily_potential_radiation(self, start_date, end_date,
                                            output_filename, atm_tra=0.75):
        """
        Compute the daily mean potential clear-sky direct solar radiation
        at the ice or snow surface [W/m²] using Hock (1999)'s equation.

        Parameters
        ----------
        start_date : datetime
            Start date of the time series (used to compute the period of interest).
        end_date : datetime
            End date of the time series (used to compute the period of interest).
        output_filename : string
            Path to the daily mean potential clear-sky direct solar radiation
            netcdf file created.
        atm_tra : float, optional
            Mean clear-sky atmospheric transmissivity, default is 0.75
            (value taken in Hock 1999)

        Returns
        -------
        The daily mean potential clear-sky direct solar radiation
        at the ice or snow surface [W/m²]
        """
        # Julian days are a continuous count of days since the beginning of the Julian
        # calendar. In solar calculations, it's often used to represent the time of
        # the year.
        start_datetime = pd.to_datetime(start_date)
        end_datetime = pd.to_datetime(end_date)
        times = pd.date_range(start_datetime, end_datetime, freq='D')

        # Convert the dates to Julian days and only compute the same day once and not
        # for all years (same result)
        jd = times.strftime('%j').to_numpy().astype(int)
        jd_unique = np.unique(jd)

        mean_height = self.get_mean_elevation()

        mean_lat, _ = self._extract_unit_mean_lat_lon(self.masked_dem_data)
        lat_rad = mean_lat * TO_RAD

        # Normalized Julian day
        # '(jd-172)' calculates the number of days that have passed since the winter
        # solstice (around December 22nd), which is day 172 in a non-leap year.
        # '(360*(jd-172))/365' normalizes this count to a value between 0 and 360,
        # representing the position in the orbit of the Earth around the Sun.
        njd = ((360 * (jd_unique - 172)) / 365)

        # The solar Declination (unit: degrees) is the angle between the rays of the
        # Sun and the plane of the Earth's equator. It represents how much the sun is
        # tilted towards or away from the observer's latitude.
        # The cos(...) function is applied to the normalized day of the year. It
        # produces values between -1 and 1, representing the variation in solar
        # declination throughout the year. The constant factor 23.45 represents the
        # tilt of the Earth's axis relative to its orbital plane. This tilt causes the
        # variation in the angle of the sun's rays reaching different latitudes
        # on Earth.
        solar_declin = 23.45 * np.cos(njd * TO_RAD)
        solar_declin = solar_declin * TO_RAD  # convert to radians

        # The hour angle is the angular distance between the sun and the observer's
        # meridian. It is typically measured in degrees. The tangent of
        # solar_declin/lat_rad represents the ratio of the opposite side
        # (vertical component of the sun's rays) to the adjacent side (horizontal
        # component). It helps capture how much the sun/the observer's location is
        # tilted north or south relative to the equator. The negative sign is applied
        # because the Hour Angle is negative in the morning and positive in the
        # afternoon. Then geometry.
        hour_angle = np.arccos(-np.tan(solar_declin) * np.tan(lat_rad))

        # A 15 min time interval represents the angular size of a 15-minute time
        # interval in radians. 15 degrees of longitude for every one hour of time,
        # divided by 4 to get the number of degrees in every 15 minutes of time.
        time_interval = (15 / 4) * TO_RAD  # 4 per hour / 15deg/4 (convert for radians)

        daily_pot_radiation = np.empty(
            (len(jd_unique), self.slope.shape[0], self.slope.shape[1]))
        daily_pot_radiation.fill(np.NaN)

        for i in range(len(jd_unique)):
            print('Day', jd_unique[i])
            # List of hour angles throughout the day.
            # [::-1] reverses the array order
            ha_list = np.arange(-hour_angle[i], hour_angle[i], time_interval)[::-1]

            # The Solar zenith (IQBAL 2012) is the angle between the sun and the
            # vertical (zenith) position directly above the observer. The result is
            # expressed in degrees. The calculation involves trigonometric functions
            # to account for the observer's latitude, the solar declination, and the
            # position of the sun in the sky (represented by the Hour Angles).
            zenith = np.arccos((np.sin(lat_rad) * np.sin(solar_declin[i])) +
                               (np.cos(lat_rad) * np.cos(solar_declin[i]) *
                                np.cos(ha_list))) * TO_DEG
            azimuth = np.arccos((np.sin((90 - zenith) * TO_RAD) *
                                 np.sin(lat_rad) - np.sin(solar_declin[i])) /
                                (np.cos((90 - zenith) * TO_RAD) *
                                 np.cos(lat_rad))) * TO_DEG

            # Solar noon (sun at the local meridian and highest in the sky)
            sol_noon = np.argmin(azimuth)

            # Azimuth with negative values before solar noon and positive
            # ones after solar noon
            Az = np.concatenate((azimuth[: sol_noon + 1] * -1, azimuth[sol_noon + 1:]))

            # Potential radiation over the time intervals defined with time_interval
            inter_pot_radiation = np.empty(
                (len(zenith), self.slope.shape[0], self.slope.shape[1]))
            inter_pot_radiation.fill(np.NaN)

            for j in range(len(zenith)):
                incidence_angle = self._calculate_angle_of_incidence(
                    zenith[j], self.slope, Az[j], self.aspect)
                potential_radiation = self._calculate_hock_equation(
                    mean_height, atm_tra, jd_unique[i], zenith[j], incidence_angle)
                inter_pot_radiation[j, :, :] = potential_radiation.copy()

            daily_pot_radiation[i, :, :] = np.nanmean(inter_pot_radiation, axis=0)

        # Get the indices of jd_unique that match the values of jd
        indices = np.where(jd_unique[:, None] == jd)[1]
        whole_daily_pot_radiation = daily_pot_radiation[indices, :, :]

        rows, cols = np.where(self.masked_dem_data)
        xs, ys = hb.rasterio.transform.xy(self.dem.transform, list(rows), list(cols))
        xs = np.array(xs).reshape(self.masked_dem_data.shape)[0, :]
        ys = np.array(ys).reshape(self.masked_dem_data.shape)[:, 0]

        self._saving_potential_radiation_netcdf(
            whole_daily_pot_radiation, xs, ys, times,
            self.dem.crs, output_filename)

    @staticmethod
    def _saving_potential_radiation_netcdf(radiation, xs, ys, times, crs,
                                           output_filename):
        """
        Save the potential radiation to a netcdf file.

        Parameters
        ----------
        radiation : np.ndarray
            The potential radiation.
        xs : np.ndarray
            The x coordinates.
        ys : np.ndarray
            The y coordinates.
        times : pd.DatetimeIndex
            The dates.
        crs : str
            The CRS.
        output_filename : str|Path
            The output filename.
        """

        print('Saving to', output_filename, crs)

        if not hb.has_xarray:
            raise ImportError("xarray is required to do this.")

        ds = hb.xr.DataArray(radiation,
                             name='radiation',
                             dims=['time', 'y', 'x'],
                             coords={
                                 "x": xs,
                                 "y": ys,
                                 "time": times})
        print(ds)
        ds.rio.write_crs(crs, inplace=True)

        ds.x.attrs["axis"] = "X"
        ds.x.attrs["long_name"] = "x coordinate of projection"
        ds.x.attrs["standard_name"] = "projection_x_coordinate"
        ds.x.attrs["units"] = "metre"

        ds.y.attrs["axis"] = "Y"
        ds.y.attrs["long_name"] = "y coordinate of projection"
        ds.y.attrs["standard_name"] = "projection_y_coordinate"
        ds.y.attrs["units"] = "metre"

        ds.to_netcdf(output_filename)
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

    def save_unit_ids_raster(self, path):
        """
        Save the unit ids raster to a file.

        Parameters
        ----------
        path : str|Path
            Path to the output file.
        """
        if self.map_unit_ids is None:
            raise ValueError("No unit ids raster to save.")

        # Create the profile
        profile = self.dem.profile
        profile.update(
            dtype=hb.rasterio.uint16,
            count=1,
            compress='lzw',
            nodata=0)

        with hb.rasterio.open(path, 'w', **profile) as dst:
            dst.write(self.map_unit_ids, 1)

    def load_unit_ids_from_raster(self, path):
        """
        Load hydro units from a raster file.

        Parameters
        ----------
        path : str|Path
            Path to the raster file containing the hydro unit ids.
        """
        if not hb.has_rasterio:
            raise ImportError("rasterio is required to do this.")

        with hb.rasterio.open(path) as src:
            self._check_crs(src)
            geoms = [mapping(self.outline)]
            self.map_unit_ids, _ = mask(src, geoms, crop=False)
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
        transformer = hb.pyproj.Transformer.from_crs(self.crs, 4326)
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
