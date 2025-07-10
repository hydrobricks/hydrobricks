from __future__ import annotations

import warnings
from pathlib import Path
from typing import TYPE_CHECKING

import numpy as np

import hydrobricks as hb

if TYPE_CHECKING:
    from hydrobricks.catchment import Catchment

if hb.has_rasterio:
    from rasterio.enums import Resampling

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


class PotentialSolarRadiation:
    """
    A class to handle solar radiation data for a catchment area.
    """

    def __init__(self, catchment: Catchment):
        """
        Initialize the SolarRadiation class.

        Parameters
        ----------
        catchment
            The catchment object.
        """
        self.catchment = catchment
        self.mean_annual_radiation = None

    def calculate_daily_potential_radiation(
            self,
            output_path: str,
            resolution: float | None = None,
            atmos_transmissivity: float = 0.75,
            steps_per_hour: int = 4,
            with_cast_shadows: bool = True
    ):
        """
        Compute the daily mean potential clear-sky direct solar radiation
        at the DEM surface [W/m²] using Hock (1999)'s equation.
        It is computed for each day of the year and saved in a netcdf file.

        Parameters
        ----------
        output_path
            Path to for created daily and annual mean potential clear-sky direct solar
            radiation files.
        resolution
            Desired pixel resolution, default is the DEM resolution.
        atmos_transmissivity
            Mean clear-sky atmospheric transmissivity, default is 0.75
            (value taken in Hock 1999)
        steps_per_hour
            Number of steps per hour to compute the potential radiation, default is 4.
        with_cast_shadows
            If True, the cast shadows are taken into account. Default is True.

        Notes
        -----
        This function is based on the R package TopoSol, authored by Matthew Olson.

        References
        ----------
        - Original R package: https://github.com/mattols/TopoSol
        """
        # Resample the DEM and calculate the slope and aspect
        dem, masked_dem_data, slope, aspect = (
            self.catchment.topography.resample_dem_and_calculate_slope_aspect(
                resolution, output_path))
        n_rows = slope.shape[0]
        n_cols = slope.shape[1]

        # Create an array with the day of the year (Julian Day)
        day_of_year = np.arange(1, 367)

        # Get some catchment attributes
        mean_elevation = self.catchment.topography.get_mean_elevation()
        mean_lat, _ = self.catchment.extract_unit_mean_lat_lon(
            self.catchment.dem_data)
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
            ha_list = np.arange(
                -ha_limit[i],
                ha_limit[i] + time_interval,
                time_interval
            )

            # Compute the zenith and azimuth
            zenith = self.get_solar_zenith(
                ha_list,
                lat_rad,
                solar_declin[i]
            )
            azimuth = self.get_solar_azimuth_to_south(
                ha_list,
                lat_rad,
                solar_declin[i]
            )

            # Potential radiation over the time intervals
            inter_pot_radiation = np.full((len(ha_list), n_rows, n_cols), np.nan)
            for j in range(len(zenith)):
                # Shorten computation time, because if zenith >= 90 : no irradiation.
                if zenith[j] >= 90:
                    inter_pot_radiation[j, :, :] = np.zeros_like(zenith[j])
                    continue

                incidence_angle = self._calculate_angle_of_incidence(
                    zenith[j],
                    slope,
                    azimuth[j],
                    aspect
                )
                potential_radiation = self._calculate_radiation_hock_equation(
                    mean_elevation,
                    atmos_transmissivity,
                    day_of_year[i],
                    zenith[j],
                    incidence_angle
                )

                # Account for cast shadows
                if with_cast_shadows:
                    cast_shadows = self.calculate_cast_shadows(
                        dem,
                        masked_dem_data,
                        zenith[j],
                        azimuth[j]
                    )
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
            mean_annual_radiation,
            dem,
            output_path
        )

        # Put the mask back on (we need the surrounding topography in the steps before)
        # And make sure the padding lines are also set to nans and not 0
        mean_annual_radiation[np.isnan(masked_dem_data)] = np.nan
        mean_annual_radiation[0, :] = np.nan
        mean_annual_radiation[-1, :] = np.nan
        mean_annual_radiation[:, 0] = np.nan
        mean_annual_radiation[:, -1] = np.nan

        # Save the daily potential radiation to a netcdf file
        self._save_potential_radiation_netcdf(
            daily_radiation,
            dem,
            masked_dem_data,
            day_of_year,
            output_path
        )

        # If DEM is the downsampled one, close it
        if dem.res[0] != self.catchment.get_dem_x_resolution():
            dem.close()

    def load_mean_annual_radiation_raster(
            self,
            dir_path: str,
            filename: str = 'annual_potential_radiation.tif'
    ):
        """
        Load the mean annual radiation raster.

        Parameters
        ----------
        dir_path
            Path to the input file directory.
        filename
            Name of the input file. Default is 'annual_potential_radiation.tif'.
        """
        self.mean_annual_radiation = hb.rxr.open_rasterio(
            Path(dir_path) / filename).drop_vars('band')[0]

    def upscale_and_save_mean_annual_radiation_rasters(
            self,
            mean_annual_radiation: np.ndarray,
            dem: hb.rasterio.Dataset,
            output_path: str,
            output_filename: str = 'annual_potential_radiation.tif'
    ):
        """
        Save the mean annual radiation rasters (downsampled and at DEM resolution)
        to a file.

        Parameters
        ----------
        mean_annual_radiation
            Downsampled mean annual radiation.
        dem
            The DEM at the resolution of the radiation.
        output_path
            Path to the output directory.
        output_filename
            Name of the output file. Default is 'annual_potential_radiation.tif'.
        """
        # Create the profile
        profile = dem.profile

        # Define the output paths
        temp_path = Path(output_path) / 'downsampled_annual_potential_radiation.tif'
        res_path = Path(output_path) / output_filename

        # If both resolutions are the same, just save the mean annual radiation
        if dem.res[0] == self.catchment.get_dem_x_resolution():
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
                    shape=self.catchment.dem.shape,
                    resampling=Resampling.bilinear,
                )
                xr_dem_upscaled.rio.to_raster(res_path)

        self.mean_annual_radiation = xr_dem_upscaled.to_numpy()
        xr_dem_upscaled.close()

    @staticmethod
    def calculate_cast_shadows(
            dem_dataset: hb.rasterio.Dataset,
            masked_dem: np.ndarray,
            zenith: float,
            azimuth: float
    ) -> np.ndarray:
        """
        Calculate the cast shadows.

        The approach relies on tilting the DEM to obtain a horizon matching the sun
        rays and filling the DEM. The algorithm is applied to the whole topography
        before masking the areas outside the catchment.

        Parameters
        ----------
        dem_dataset
            DEM as read by rasterio, containing the DEM topography
        masked_dem
            DEM topography, masked with np.nan for the areas outside the study catchment
        zenith
            Solar zenith (IQBAL 2012), in degrees
        azimuth
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

    @staticmethod
    def get_solar_declination_rad(day_of_year: int | np.ndarray) -> float:
        """
        Compute the solar declination.

        The solar declination is the angle between the rays of the Sun and the
        plane of the Earth's equator. It represents how much the sun is
        tilted towards or away from the observer's latitude. The calculation
        involves trigonometric functions to account for the observer's latitude
        and the position of the sun in the sky.

        Parameters
        ----------
        day_of_year
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
    def get_solar_hour_angle_limit(
            solar_declination: float | np.ndarray,
            lat_rad: float,
    ) -> float | np.ndarray:
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
        solar_declination
            Solar declination in radians.
        lat_rad
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
    def get_solar_zenith(
            hour_angles: float | np.ndarray,
            lat_rad: float,
            solar_declination: float
    ) -> float | np.ndarray:
        """
        Compute the solar zenith.

        The Solar zenith (IQBAL 2012) is the angle between the sun and the
        vertical (zenith) position directly above the observer. The result is
        expressed in degrees. The calculation involves trigonometric functions
        to account for the observer's latitude, the solar declination, and the
        position of the sun in the sky (represented by the Hour Angles).

        Parameters
        ----------
        hour_angles
            Hour angle(s).
        lat_rad
            Latitude in radians.
        solar_declination
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
    def get_solar_azimuth_to_south(
            hour_angles: float | np.ndarray,
            lat_rad: float,
            solar_declination: float
    ) -> np.ndarray:
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
        hour_angles
            Array with the hour angles.
        lat_rad
            Latitude in radians.
        solar_declination
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
    def get_solar_azimuth_to_north(
            hour_angles: float | np.ndarray,
            lat_rad: float,
            solar_declination: float
    ) -> float | np.ndarray:
        """
        Compute the solar azimuth relative to the north.
        See get_solar_azimuth_to_south() for more details.
        """
        azimuth = PotentialSolarRadiation.get_solar_azimuth_to_south(
            hour_angles, lat_rad, solar_declination)
        azimuth += 180

        return azimuth

    @staticmethod
    def _calculate_radiation_hock_equation(
            elevation: float,
            atmos_transmissivity: float,
            day_of_year: int | float,
            zenith: float,
            incidence_angle: np.array
    ) -> np.array:
        """
        Hock (2005) equation to compute the potential clear-sky direct solar
        radiation at the ice or snow surface [W/m²].

        Parameters
        ----------
        elevation
            Height above sea level [m]
        atmos_transmissivity
            Mean clear-sky atmospheric transmissivity
        day_of_year
            Day of the year (Julian Day)
        zenith
            Solar zenith for one moment during the day (IQBAL 2012)
        incidence_angle
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

        solar_radiation = (
                SOLAR_CST * ((ES_SM_AXIS / current_se_dist) ** 2) *
                atmos_transmissivity ** (
                        local_pressure / (SEA_ATM_PRESSURE * np.cos(zenith * TO_RAD))
                ) *
                np.cos(incidence_angle)
        )

        return solar_radiation

    @staticmethod
    def _calculate_angle_of_incidence(
            zenith: float,
            slope: float,
            azimuth: float,
            aspect: float,
            tolerance: float = 10 ** (-6)
    ) -> float:
        """
        Calculate the angle of incidence.

        Parameters
        ----------
        zenith
            Solar zenith (IQBAL 2012), in degrees
        slope
            Slope of the DEM, in degrees
        azimuth
            Azimuth for ZSLOPE CALC, in degrees
        aspect
            Aspect of the DEM, in degrees
        tolerance
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

    def _save_potential_radiation_netcdf(
            self,
            radiation: np.ndarray,
            dem: hb.rasterio.Dataset,
            masked_dem_data: np.ndarray,
            day_of_year: np.ndarray,
            output_path: str | None,
            output_filename: str = 'daily_potential_radiation.nc'):
        """
        Save the potential radiation to a netcdf file.

        Parameters
        ----------
        radiation
            The potential radiation.
        dem
            The DEM.
        masked_dem_data
            The masked DEM data.
        day_of_year
            The array with the days of the year.
        output_path
            Path to the directory to save the netcdf file to.
        output_filename
            Name of the output file. Default is 'daily_potential_radiation.nc'.
        """
        full_path = Path(output_path) / output_filename
        print('Saving to', str(full_path), self.catchment.dem.crs)

        if not hb.has_xarray:
            raise ImportError("xarray is required to do this.")

        rows, cols = np.where(masked_dem_data)
        xs, ys = hb.rasterio.transform.xy(dem.transform, list(rows), list(cols))
        xs = np.array(xs).reshape(masked_dem_data.shape)[0, :]
        ys = np.array(ys).reshape(masked_dem_data.shape)[:, 0]

        ds = hb.xr.DataArray(
            radiation,
            name='radiation',
            dims=['day_of_year', 'y', 'x'],
            coords={
                "x": xs,
                "y": ys,
                "day_of_year": day_of_year
            }
        )

        with warnings.catch_warnings():
            warnings.filterwarnings("ignore", category=UserWarning)  # pyproj
            ds.rio.write_crs(self.catchment.dem.crs, inplace=True)

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
