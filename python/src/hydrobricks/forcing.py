from enum import StrEnum, auto

import numpy as np
import pyet

import hydrobricks as hb

from .time_series import TimeSeries1D, TimeSeries2D


class Forcing:
    """Class for forcing data"""

    class Variable(StrEnum):
        P = auto()  # Precipitation
        T = auto()  # Temperature
        T_MIN = auto()  # Minimum temperature
        T_MAX = auto()  # Maximum temperature
        T_DEW_POINT = auto()  # Dew point temperature
        PET = auto()  # Potential evapotranspiration
        RH = auto()  # Relative humidity
        RH_MIN = auto()  # Minimum relative humidity
        RH_MAX = auto()  # Maximum relative humidity
        R_NET = auto()  # Net radiation
        R_SOLAR = auto()  # Solar radiation
        SD = auto()  # Sunshine duration
        WIND = auto()  # Wind speed
        PRESSURE = auto()  # Atmospheric pressure

    def __init__(self, hydro_units):
        super().__init__()
        self.data1D = TimeSeries1D()
        self.data2D = TimeSeries2D()
        self.hydro_units = hydro_units.hydro_units
        self._operations = []
        self._is_initialized = False

    def is_initialized(self):
        """ Return True if the forcing is initialized. """
        return self._is_initialized

    def get_variable_enum(self, variable):
        """
        Match the variable name to the enum corresponding value.

        Parameters
        ----------
        variable : str
            Variable name.
        """
        if variable in self.Variable.__members__:
            return self.Variable[variable]
        else:
            if variable in ['precipitation', 'precip', 'p']:
                return self.Variable.P
            elif variable in ['temperature', 'temp', 't']:
                return self.Variable.T
            elif variable in ['temperature_min', 'min_temperature', 't_min', 'tmin']:
                return self.Variable.T_MIN
            elif variable in ['temperature_max', 'max_temperature', 't_max', 'tmax']:
                return self.Variable.T_MAX
            elif variable in ['pet']:
                return self.Variable.PET
            elif variable in ['relative_humidity', 'rh']:
                return self.Variable.RH
            elif variable in ['relative_humidity_min', 'min_relative_humidity',
                              'rh_min', 'rhmin']:
                return self.Variable.RH_MIN
            elif variable in ['relative_humidity_max', 'max_relative_humidity',
                              'rh_max', 'rhmax']:
                return self.Variable.RH_MAX
            elif variable in ['net_radiation', 'r_net', 'r_n', 'rn']:
                return self.Variable.R_NET
            elif variable in ['solar_radiation', 'r_solar', 'r_s', 'rs']:
                return self.Variable.R_SOLAR
            elif variable in ['sunshine_duration', 'sd']:
                return self.Variable.SD
            elif variable in ['wind_speed', 'wind']:
                return self.Variable.WIND
            elif variable in ['pressure']:
                return self.Variable.PRESSURE

            else:
                raise ValueError(f'Variable {variable} is not recognized.')

    def _can_be_negative(self, variable):
        if variable in [self.Variable.P, self.Variable.PET, self.Variable.RH,
                        self.Variable.RH_MIN, self.Variable.RH_MAX, self.Variable.R_NET,
                        self.Variable.R_SOLAR, self.Variable.SD, self.Variable.WIND,
                        self.Variable.PRESSURE]:
            return False
        elif variable in [self.Variable.T, self.Variable.T_MIN, self.Variable.T_MAX,
                          self.Variable.T_DEW_POINT]:
            return True
        else:
            raise ValueError(f'Undefined if variable {variable} can be negative.')

    def load_station_data_from_csv(self, path, column_time, time_format, content):
        """
        Read 1D time series data from csv file.

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
        # Change the variable names (key) to the enum corresponding values
        for key in list(content.keys()):
            enum_val = self.get_variable_enum(key)
            content[enum_val] = content.pop(key)

        self.data1D.load_from_csv(path, column_time, time_format, content)

    def set_prior_correction(self, **kwargs):
        """
        Define the prior correction operations.

        Parameters
        ----------
        variable: str
            Name of the variable to correct.
        method: str
            Name of the method to use. Possible values are:
            * additive: add a constant value
            * multiplicative: multiply by a constant value
        correction_factor: float
            Value of the correction factor (to add or multiply).
        """
        kwargs['type'] = 'prior_correction'
        self._operations.append(kwargs)

    def set_spatialization_from_station_data(self, **kwargs):
        """
        Define the spatialization operations from station data to all hydro units.

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
            Reference (station) elevation.
            For method(s): 'elevation_gradient'
        gradient : float/list
            Gradient of the variable to apply per 100m (e.g., °C/100m).
            Can be a unique value or a list providing a value for every month.
            For method(s): 'elevation_gradient', 'elevation_multi_gradients'
        gradient_1 : float/list
            Same as parameter 'gradient'
        gradient_2 : float/list
            Gradient of the variable to apply per 100m (e.g., °C/100m) for the units
            above the elevation threshold defined by 'elevation_threshold'.
            For method(s): 'elevation_multi_gradients'
        elevation_threshold : int/float
            Threshold elevation to switch from gradient to gradient_2.
            For method(s): 'elevation_multi_gradients'
        """
        kwargs['type'] = 'spatialize_from_station'
        self._operations.append(kwargs)

    def set_spatialization_from_gridded_data(self, **kwargs):
        """
        Define the spatialization operations from gridded data to all hydro units.

        Parameters
        ----------

        """
        kwargs['type'] = 'spatialize_from_grid'
        self._operations.append(kwargs)

    def set_pet_computation(self, **kwargs):
        """
        Define the PET computation operations using the pyet library. The PET will be
        computed for all hydro units.

        Parameters
        ----------

        """
        kwargs['type'] = 'compute_pet'
        self._operations.append(kwargs)

    def apply_operations(self, parameters, apply_to_all=True):
        """
        Apply the pre-defined operations.

        Parameters
        ----------
        parameters: ParameterSet
            The parameter object instance.
        apply_to_all: bool
            If True, the operations will be applied to all variables. If False, the
            operations will only be applied to the variables related to parameters
            defined in the parameters.allow_changing list. This is useful to avoid
            re-applying, during the calibration phase, operations that have already
            been applied previously.
        """
        # The operations will be applied in the order defined in the list
        operation_types = ['prior_correction', 'spatialize_from_station',
                           'spatialize_from_grid', 'compute_pet']

        for operation_type in operation_types:
            self._apply_operations_of_type(operation_type, parameters, apply_to_all)

        self._is_initialized = True

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
        if not hb.has_netcdf:
            raise ImportError("netcdf4 is required to do this.")

        time = self.data2D.get_dates_as_mjd()

        # Create netCDF file
        nc = hb.Dataset(path, 'w', 'NETCDF4')

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

        for idx, variable in enumerate(self.data2D.data_name):
            if max_compression:
                var_data = nc.createVariable(
                    variable, 'float32', ('time', 'hydro_units'), zlib=True,
                    least_significant_digit=3)
            else:
                var_data = nc.createVariable(
                    variable, 'float32', ('time', 'hydro_units'), zlib=True)
            var_data[:, :] = self.data2D.data[idx]

        nc.close()

    def get_total_precipitation(self):
        idx = self.data2D.data_name.index(self.Variable.P)
        data = self.data2D.data[idx].sum(axis=0)
        areas = self.hydro_units['area']
        tot_precip = data * areas / areas.sum()
        return tot_precip.sum()

    def _apply_operations_of_type(self, operation_type, parameters, apply_to_all):
        for operation_ref in self._operations:
            operation = operation_ref.copy()

            if operation['type'] != operation_type:
                continue

            # Remove the type key
            operation.pop('type')

            # Extract the operation options
            apply_operation = False
            for key in operation:
                value = operation[key]
                if isinstance(value, str) and value.startswith('param:'):
                    parameter_name = value.replace('param:', '')
                    operation[key] = parameters.get(parameter_name)
                    if not apply_to_all:  # Restrict to parameters that changed
                        if parameter_name in parameters.allow_changing:
                            apply_operation = True

            # Apply the operation (or not)
            if apply_to_all or apply_operation:
                if operation_type == 'prior_correction':
                    self._apply_prior_correction(**operation)
                elif operation_type == 'spatialize_from_station':
                    self._apply_spatialization_from_station_data(**operation)
                elif operation_type == 'spatialize_from_grid':
                    self._apply_spatialization_from_gridded_data(**operation)
                elif operation_type == 'compute_pet':
                    self._apply_pet_computation(**operation)
                else:
                    raise ValueError(f'Unknown operation type: {operation_type}')

    def _apply_prior_correction(self, variable, method='multiplicative', **kwargs):
        variable = self.get_variable_enum(variable)
        idx = self.data1D.data_name.index(variable)

        # Extract kwargs (None if not provided)
        correction_factor = kwargs.get('correction_factor', None)
        if correction_factor is None:
            raise ValueError('Correction factor not provided.')

        if method == 'multiplicative':
            correction_factor = kwargs['correction_factor']
            self.data1D.data[idx] *= correction_factor
        elif method == 'additive':
            correction_factor = kwargs['correction_factor']
            self.data1D.data[idx] += correction_factor
        else:
            raise ValueError(f'Unknown method: {method}')

    def _apply_spatialization_from_station_data(self, variable, method='default',
                                                **kwargs):
        variable = self.get_variable_enum(variable)
        unit_values = np.zeros((len(self.data1D.time), len(self.hydro_units)))
        hydro_units = self.hydro_units.reset_index()
        idx_1D = self.data1D.data_name.index(variable)
        data_raw = self.data1D.data[idx_1D].copy()

        # Extract kwargs (None if not provided)
        ref_elevation = kwargs.get('ref_elevation', None)
        gradient = kwargs.get('gradient', None)
        gradient_1 = kwargs.get('gradient_1', None)
        gradient_2 = kwargs.get('gradient_2', None)
        elevation_threshold = kwargs.get('elevation_threshold', None)

        # Specify default methods
        if method == 'default':
            if variable == self.Variable.T:
                method = 'additive_elevation_gradient'
            elif variable == self.Variable.P:
                method = 'multiplicative_elevation_gradient'
            elif variable == self.Variable.PET:
                method = 'constant'
            else:
                raise ValueError(f'Unknown default method for variable: {variable}')

        if gradient is None:
            gradient = gradient_1

        # Check inputs
        if method == 'multiplicative_elevation_threshold_gradients':
            if not elevation_threshold:
                method = 'multiplicative_elevation_gradient'

        if method in ['additive_elevation_gradient',
                      'multiplicative_elevation_gradient']:
            if gradient is None:
                method = 'constant'

        if isinstance(gradient, list):
            if len(gradient) not in [1, 12]:
                raise ValueError(
                    f'The gradient should have a length of 1 or 12. '
                    f'Here: {len(gradient)}')

        # Apply methods
        for i_unit, unit in hydro_units.iterrows():
            elevation = unit['elevation']

            if method == 'constant':
                unit_values[:, i_unit] = data_raw

            elif method == 'additive_elevation_gradient':
                if isinstance(gradient, float) or isinstance(gradient, list) \
                        and len(gradient) == 1:
                    unit_values[:, i_unit] = data_raw + gradient * (
                                elevation - ref_elevation) / 100
                elif isinstance(gradient, list) and len(gradient) == 12:
                    for m in range(12):
                        month = self.data1D.time.dt.month == m + 1
                        month = month.to_numpy()
                        unit_values[month, i_unit] = data_raw[month] + gradient[m] * (
                                elevation - ref_elevation) / 100
                else:
                    raise ValueError(f'Wrong gradient format: {gradient}')

            elif method == 'multiplicative_elevation_gradient':
                if isinstance(gradient, float) or isinstance(gradient, list) \
                        and len(gradient) == 1:
                    unit_values[:, i_unit] = data_raw * (
                            1 + gradient * (elevation - ref_elevation) / 100)
                elif isinstance(gradient, list) and len(gradient) == 12:
                    for m in range(12):
                        month = self.data1D.time.dt.month == m + 1
                        month = month.to_numpy()
                        unit_values[month, i_unit] = \
                            data_raw[month] * (1 + gradient[m] * (
                                    elevation - ref_elevation) / 100)
                else:
                    raise ValueError(f'Wrong gradient format: {gradient}')

            elif method == 'multiplicative_elevation_threshold_gradients':
                if elevation < elevation_threshold:
                    unit_values[:, i_unit] = data_raw * (
                            1 + gradient * (elevation - ref_elevation) / 100)
                elif ref_elevation > elevation_threshold:
                    unit_values[:, i_unit] = data_raw * (
                            1 + gradient_2 * (elevation - ref_elevation) / 100)
                else:
                    precip_below = data_raw * (1 + gradient * (
                            elevation_threshold - ref_elevation) / 100)
                    unit_values[:, i_unit] = precip_below * (
                            1 + gradient_2 * (elevation - elevation_threshold) / 100)

        # Check outputs
        if not self._can_be_negative(variable):
            unit_values[unit_values < 0] = 0

        # Store outputs
        if variable in self.data2D.data_name:
            idx_2D = self.data2D.data_name.index(variable)
            self.data2D.data[idx_2D] = unit_values
        else:
            self.data2D.data.append(unit_values)
            self.data2D.data_name.append(variable)
            self.data2D.time = self.data1D.time

    def _apply_spatialization_from_gridded_data(self, variable, **kwargs):
        variable = self.get_variable_enum(variable)
        pass

    def _apply_pet_computation(self, method, use_variables, **kwargs):
        # Check that the provided method is a function of pyet
        if method not in dir(pyet):
            raise ValueError(f'Unknown method: {method}')

        use_variables = [self.get_variable_enum(v) for v in use_variables]

        pass
