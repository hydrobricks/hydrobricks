import numpy as np
from netCDF4 import Dataset

from .time_series import TimeSeries


class Forcing(TimeSeries):
    """Class for forcing data"""

    def __init__(self, hydro_units):
        super().__init__()
        self.hydro_units = hydro_units.hydro_units

    def spatialize(self, variable, method='constant', ref_elevation=0, gradient=0,
                   gradient_2=0, elevation_threshold=None, correction_factor=None):
        """
        Spatializes the provided variable to all hydro units using the defined method.

        Parameters
        ----------
        variable: str
            Name of the variable to spatialize.
        method: str
            Name of the method to use. Can be:
            * constant: the same value will be used
            * additive_elevation_gradient: use of an additive elevation gradient that
              is either constant or changes for every month.
              Parameters: 'ref_elevation', 'gradient'.
            * multiplicative_elevation_gradient: use of a multiplicative elevation
              gradient that is either constant or changes for every month.
              Parameters: 'ref_elevation', 'gradient'.
            * multiplicative_elevation_threshold_gradients: same as
              multiplicative_elevation_gradient, but with an elevation threshold with a
              gradient below and a gradient above.
              Parameters: 'ref_elevation', 'gradient', 'gradient_2',
              'elevation_threshold'
        ref_elevation : float
            Reference elevation.
            For method(s): 'elevation_gradient'
        gradient : float/list
            Gradient of the variable to apply per 100m (e.g., °C/100m).
            Can be a unique value or a list providing a value for every month.
            For method(s): 'elevation_gradient', 'elevation_multi_gradients'
        gradient_2 : float/list
            Gradient of the variable to apply per 100m (e.g., °C/100m) for the units
            above the elevation threshold defined by 'elevation_threshold'.
            For method(s): 'elevation_multi_gradients'
        elevation_threshold : int/float
            Threshold elevation to switch from gradient to gradient_2
        correction_factor: float
            Correction factor to apply to the precipitation data before spatialization
        """
        unit_values = np.zeros((len(self.time), len(self.hydro_units)))
        hydro_units = self.hydro_units.reset_index()
        i_col = self.data_name.index(variable)
        data_raw = self.data_raw[i_col].copy()

        # Check inputs
        if method == 'multiplicative_elevation_threshold_gradients':
            if not elevation_threshold:
                method = 'multiplicative_elevation_gradient'

        if method in ['additive_elevation_gradient',
                      'multiplicative_elevation_gradient']:
            if isinstance(gradient, int) and gradient == 0:
                method = 'constant'

        if isinstance(gradient, list):
            if len(gradient) not in [1, 12]:
                raise ValueError(
                    f'The gradient should have a length of 1 or 12. '
                    f'Here: {len(gradient)}')

        # Pre-process (e.g., correction factor)
        if correction_factor:
            data_raw = data_raw * correction_factor

        # Apply methods
        for i_unit, unit in hydro_units.iterrows():
            elevation = unit['elevation']

            if method == 'constant':
                unit_values[:, i_unit] = data_raw

            elif method == 'additive_elevation_gradient':
                if len(gradient) == 1:
                    unit_values[:, i_unit] = data_raw + gradient * \
                                                  (elevation - ref_elevation) / 100
                elif len(gradient) == 12:
                    for m in range(12):
                        month = self.time.dt.month == m + 1
                        month = month.to_numpy()
                        unit_values[month, i_unit] = data_raw[month] + gradient[m] * \
                                                     (elevation - ref_elevation) / 100

            elif method == 'multiplicative_elevation_gradient':
                if len(gradient) == 1:
                    unit_values[:, i_unit] = data_raw * (
                                1 + gradient * (elevation - ref_elevation) / 100)
                elif len(gradient) == 12:
                    for m in range(12):
                        month = self.time.dt.month == m + 1
                        month = month.to_numpy()
                        unit_values[month, i_unit] = \
                            data_raw[month] * (1 + gradient[m] * (
                                    elevation - ref_elevation) / 100)

            elif method == 'multiplicative_elevation_threshold_gradients':
                if elevation < elevation_threshold:
                    unit_values[:, i_unit] = data_raw * (
                            1 + gradient * (elevation - ref_elevation) / 100)
                else:
                    precip_below = data_raw * (1 + gradient * (
                            elevation_threshold - ref_elevation) / 100)
                    unit_values[:, i_unit] = precip_below * (
                            1 + gradient_2 * (elevation - elevation_threshold) / 100)

        # Check outputs
        if variable in ['pet', 'precipitation']:
            unit_values[unit_values < 0] = 0

        self.data_spatialized[i_col] = unit_values

    def spatialize_temperature(self, ref_elevation, lapse):
        """
        Spatializes the temperature using a temperature lapse that is either constant
        or changes for every month.

        Parameters
        ----------
        ref_elevation : float
            Elevation of the reference station.
        lapse : float/list
            Temperature lapse [°C/100m] to compute the temperature for every hydro unit.
            Can be a unique value or a list providing a value for every month.
        """
        self.spatialize('temperature', 'additive_elevation_gradient',
                        ref_elevation=ref_elevation, gradient=lapse)

    def spatialize_pet(self, ref_elevation, gradient=0):
        """
        Spatializes the PET using a gradient that is either constant or changes for
        every month.

        Parameters
        ----------
        ref_elevation : float
            Elevation of the reference station.
        gradient : float/list
            Gradient [mm/100m] to compute the PET for every hydro unit.
            Can be a unique value or a list providing a value for every month.
        """
        self.spatialize('pet', 'additive_elevation_gradient',
                        ref_elevation=ref_elevation, gradient=gradient)

    def spatialize_precipitation(self, ref_elevation, gradient_1, gradient_2=None,
                                 elevation_threshold=None, correction_factor=None):
        """
        Spatializes the precipitation using a single gradient for the full elevation
        range or a two-gradients approach with an elevation threshold.

        Parameters
        ----------
        ref_elevation : float
            Elevation of the reference station.
        gradient_1 : float
            Precipitation gradient (ratio) per 100 m of altitude.
        gradient_2 : float
            Precipitation gradient (ratio) per 100 m of altitude for the units above
            the threshold elevation (optional).
        elevation_threshold: float
            Threshold to switch from gradient 1 to gradient 2 (optional).
        correction_factor: float
            Correction factor to apply to the precipitation data before spatialization
        """
        if not elevation_threshold:
            self.spatialize('pet', 'multiplicative_elevation_gradient',
                            ref_elevation=ref_elevation, gradient=gradient_1,
                            correction_factor=correction_factor)
        else:
            self.spatialize('pet', 'multiplicative_elevation_threshold_gradients',
                            ref_elevation=ref_elevation, gradient=gradient_1,
                            gradient_2=gradient_2,
                            elevation_threshold=elevation_threshold,
                            correction_factor=correction_factor)

    def create_file(self, path, max_compression=False):
        """
        Read hydro units properties from csv file.

        Parameters
        ----------
        path : str
            Path of the file to create.
        max_compression: bool
            Option to allow maximum compression for data in file.
        """

        time = self._date_as_mjd()

        # Create netCDF file
        nc = Dataset(path, 'w', 'NETCDF4')

        # Dimensions
        nc.createDimension('hydro_units', len(self.hydro_units))
        nc.createDimension('time', len(time))

        # Variables
        var_id = nc.createVariable('id', 'int', ('hydro_units',))
        var_id[:] = self.hydro_units['id']

        var_time = nc.createVariable('time', 'float32', ('time',))
        var_time[:] = time
        var_time.units = 'days since 1858-11-17 00:00:00'
        var_time.comment = 'Modified Julian Day Numer'

        for index, variable in enumerate(self.data_name):
            if max_compression:
                var_data = nc.createVariable(
                    variable, 'float32', ('time', 'hydro_units'), zlib=True,
                    least_significant_digit=3)
            else:
                var_data = nc.createVariable(
                    variable, 'float32', ('time', 'hydro_units'), zlib=True)
            var_data[:, :] = self.data_spatialized[index]

        nc.close()