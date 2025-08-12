from __future__ import annotations

import sys

if sys.version_info < (3, 11):
    try:
        from strenum import LowercaseStrEnum, StrEnum
    except ImportError:
        raise ImportError("Please install the 'StrEnum' package to use StrEnum "
                          "on Python versions prior to 3.11.")
else:
    from enum import StrEnum

from enum import auto
from pathlib import Path

import numpy as np
import pandas as pd
from cftime import num2date

import hydrobricks as hb
from hydrobricks.catchment import Catchment
from hydrobricks.constants import TO_RAD
from hydrobricks.hydro_units import HydroUnits
from hydrobricks.parameters import ParameterSet
from hydrobricks.time_series import TimeSeries1D, TimeSeries2D


class Forcing:
    """Class for forcing data"""

    if sys.version_info < (3, 11):
        StrEnumClass = LowercaseStrEnum
    else:
        StrEnumClass = StrEnum

    class Variable(StrEnumClass):
        P = auto()  # Precipitation [mm]
        T = auto()  # Temperature [°C]
        T_MIN = auto()  # Minimum temperature [°C]
        T_MAX = auto()  # Maximum temperature [°C]
        T_DEW_POINT = auto()  # Dew point temperature [°C]
        PET = auto()  # Potential evapotranspiration [mm]
        RH = auto()  # Relative humidity [%]
        RH_MIN = auto()  # Minimum relative humidity [%]
        RH_MAX = auto()  # Maximum relative humidity [%]
        R_NET = auto()  # Net radiation [MJ m-2 d-1]
        R_SOLAR = auto()  # Solar radiation [MJ m-2 d-1]
        SD = auto()  # Sunshine duration [h]
        WIND = auto()  # Wind speed [m s-1]
        PRESSURE = auto()  # Atmospheric pressure [kPa]

    def __init__(
            self,
            spatial_entity: HydroUnits | Catchment
    ):
        if isinstance(spatial_entity, HydroUnits):
            hydro_units = spatial_entity
            catchment = None
        elif isinstance(spatial_entity, Catchment):
            hydro_units = spatial_entity.hydro_units
            catchment = spatial_entity
        else:
            raise TypeError('The spatial_entity argument must be a HydroUnits or '
                            'Catchment object, not {type(spatial_entity)}.')

        # Check hydro units
        if len(hydro_units.hydro_units) == 0:
            raise ValueError('The hydro_units argument must contain at least '
                             'one hydrological unit.')

        super().__init__()
        self.data1D = TimeSeries1D()
        self.data2D = TimeSeries2D()
        self.catchment = catchment
        self.hydro_units = hydro_units.hydro_units
        self._operations = []
        self._is_initialized = False

    def is_initialized(self) -> bool:
        """ Return True if the forcing is initialized. """
        return self._is_initialized

    def get_variable_enum(self, variable: str) -> Variable:
        """
        Match the variable name to the enum corresponding value.

        Parameters
        ----------
        variable
            Variable name.
        """
        if variable in self.Variable.__members__:
            return self.Variable[variable]

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
        elif variable in ['relative_humidity_min', 'min_relative_humidity', 'rh_min',
                          'rhmin']:
            return self.Variable.RH_MIN
        elif variable in ['relative_humidity_max', 'max_relative_humidity', 'rh_max',
                          'rhmax']:
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

    def _can_be_negative(self, variable: Variable) -> bool:
        if variable in [
            self.Variable.P,
            self.Variable.PET,
            self.Variable.RH,
            self.Variable.RH_MIN,
            self.Variable.RH_MAX,
            self.Variable.R_NET,
            self.Variable.R_SOLAR,
            self.Variable.SD,
            self.Variable.WIND,
            self.Variable.PRESSURE
        ]:
            return False
        elif variable in [
            self.Variable.T,
            self.Variable.T_MIN,
            self.Variable.T_MAX,
            self.Variable.T_DEW_POINT
        ]:
            return True
        else:
            raise ValueError(f'Undefined if variable {variable} can be negative.')

    def load_station_data_from_csv(
            self,
            path: str | Path,
            column_time: str,
            time_format: str,
            content: dict[str, str] | dict[str, Variable] = None
    ):
        """
        Read 1D time series data from csv file.

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
        """
        # Change the variable names (key) to the enum corresponding values
        for key in list(content.keys()):
            enum_val = self.get_variable_enum(key)
            content[enum_val] = content.pop(key)

        self.data1D.load_from_csv(path, column_time, time_format, content)

    def correct_station_data(self, **kwargs):
        """
        Define the prior correction operations.

        Parameters
        ----------
        variable : str
            Name of the variable to correct.
        method : str
            Name of the method to use. Possible values are:
            * additive: add a constant value
            * multiplicative: multiply by a constant value
        correction_factor : float
            Value of the correction factor (to add or multiply).
        """
        kwargs['type'] = 'prior_correction'
        self._operations.append(kwargs)

    def spatialize_from_station_data(self, **kwargs):
        """
        Define the spatialization operations from station data to all hydro units.

        Parameters
        ----------
        variable : str
            Name of the variable to spatialize.
        method : str
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

    def spatialize_from_gridded_data(self, **kwargs):
        """
        Define the spatialization operations from gridded data to all hydro units.

        Parameters
        ----------
        variable : str
            Name of the variable to spatialize.
        method : str
            Name of the method to use. Can be:
            * regrid_from_netcdf: regrid data from a single or multiple netCDF files.
        path : str|Path
            Path to the file containing the data or to a folder containing multiple
            files.
        file_pattern : str, optional
            Pattern of the files to read. If None, the path is considered to be
            a single file.
        data_crs : int, optional
            CRS (as EPSG id) of the data file. If None, the CRS is read from the file.
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
        apply_data_gradient : bool, optional
            If True, elevation-based gradients will be retrieved from the data and
            applied to the hydro units (e.g., for temperature and precipitation).
            If False, the data will be regridded without applying any gradient.
            Default is True for temperature and precipitation variables, and False
            for other variables.
        """
        kwargs['type'] = 'spatialize_from_grid'
        self._operations.append(kwargs)

    def compute_pet(self, **kwargs):
        """
        Define the PET computation operations using the pyet library. The PET will be
        computed for all hydro units.

        Parameters
        ----------
        method : str
            Name of the method to use. Possible values are those provided in the
            table from the pyet documentation: https://pypi.org/project/pyet/. The
            method name or the pyet function name can be used.
        use : list
            List of the meteorological variables to use to compute the PET. Only the
            variables listed here will be used. The variables must be named according
            to pyet naming convention (see pyet API documentation:
            https://pyet.readthedocs.io/en/latest/api/index.html).
            These variables must be available (data loaded in forcing) and spatialized.
            Example: use=['t', 'tmin', 'tmax', 'lat', 'elevation']
        lat : float, optional
            Latitude of the catchment. If not provided, the latitude computed for each
            hydro unit will be used.
        other options : see pyet documentation for function-specific options. These
            options will be passed to the pyet function.
        """
        if not hb.has_pyet:
            raise ImportError("pyet is required to do this.")

        kwargs['type'] = 'compute_pet'

        self._operations.append(kwargs)

    def apply_operations(
            self,
            parameters: ParameterSet = None,
            apply_to_all: bool = True
    ):
        """
        Apply the pre-defined operations.

        Parameters
        ----------
        parameters
            The parameter object instance.
        apply_to_all
            If True, the operations will be applied to all variables. If False, the
            operations will only be applied to the variables related to parameters
            defined in the parameters.allow_changing list. This is useful to avoid
            re-applying, during the calibration phase, operations that have already
            been applied previously.
        """
        # The operations will be applied in the order defined in the list
        operation_types = [
            'prior_correction',
            'spatialize_from_station',
            'spatialize_from_grid',
            'compute_pet'
        ]

        for operation_type in operation_types:
            self._apply_operations_of_type(operation_type, parameters, apply_to_all)

        self._is_initialized = True

    def save_as(self, path: str | Path, max_compression: bool = False):
        """
        Create a netCDF file with the data.

        Parameters
        ----------
        path
            Path of the file to create.
        max_compression
            Option to allow maximum compression for data in file.
        """
        if not hb.has_netcdf:
            raise ImportError("netcdf4 is required to do this.")

        if not self.is_initialized():
            print("Applying operations before saving...")
            self.apply_operations()
            self._is_initialized = True

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

    def load_from(self, path: str | Path):
        """
        Load data from a netCDF file created using save_as().

        Parameters
        ----------
        path
            Path of the file to read.
        """
        if not hb.has_netcdf:
            raise ImportError("netcdf4 is required to do this.")

        # Open netCDF file
        nc = hb.Dataset(path, 'r', 'NETCDF4')

        # Check that hydro units are the same
        hydro_units_nc = nc.variables['id'][:]
        hydro_units_def = self.hydro_units[('id', '-')].values
        if not np.array_equal(hydro_units_nc, hydro_units_def):
            raise ValueError("The hydrological units in the netCDF file are not "
                             "the same as those in the forcing object. The netCDF file "
                             "contains hydrological units with ids: "
                             f"{hydro_units_nc}. The model contains hydrological "
                             f"units with ids: {hydro_units_def}.")

        # Load time
        ts = num2date(nc.variables['time'][:], units=nc.variables['time'].units)
        self.data2D.time = [pd.Timestamp(dt.year, dt.month, dt.day) for dt in ts]
        self.data2D.time = pd.Series(self.data2D.time)

        # Load variable names
        self.data2D.data_name = [self.get_variable_enum(var) for var in nc.variables
                                 if var not in ['id', 'time']]

        # Load data
        self.data2D.data = []
        for variable in self.data2D.data_name:
            self.data2D.data.append(nc.variables[variable][:])

        nc.close()

    def get_total_precipitation(self) -> float:
        idx = self.data2D.data_name.index(self.Variable.P)
        data = self.data2D.data[idx].sum(axis=0)
        areas = self.hydro_units[('area', 'm2')]
        tot_precip = data * areas.values / areas.sum()
        return tot_precip.sum()

    def _apply_operations_of_type(
            self,
            operation_type: str,
            parameters: ParameterSet | None = None,
            apply_to_all: bool = True
    ):
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
                    if parameters is None:
                        raise ValueError('A parameters object must be provided '
                                         'to apply the operations as it is '
                                         f'required by the "{value}" option.')
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

    def _apply_prior_correction(
            self,
            variable: str,
            method: str = 'multiplicative',
            **kwargs
    ):
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

    def _apply_spatialization_from_station_data(
            self,
            variable: str,
            method: str = 'default',
            **kwargs
    ):
        # Checking that the correction_factor option is not used here anymore
        if 'correction_factor' in kwargs:
            raise ValueError('The correction_factor option is to be used only '
                             'in the correct_station_data() method.')

        variable = self.get_variable_enum(variable)
        unit_values = np.zeros((len(self.data1D.time), len(self.hydro_units)))
        hydro_units = self.hydro_units.reset_index()
        idx_1d = self.data1D.data_name.index(variable)
        data_raw = self.data1D.data[idx_1d].copy()

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

        # Extract kwargs (None if not provided)
        ref_elevation = kwargs.get('ref_elevation', None)
        gradient = kwargs.get('gradient', None)

        # Check inputs
        if gradient is None:
            gradient = kwargs.get('gradient_1', None)

        if isinstance(gradient, list):
            if len(gradient) not in [1, 12]:
                raise ValueError(f'The gradient should have a length of 1 or 12. '
                                 f'Here: {len(gradient)}')

        # Apply methods
        for i_unit, (_, unit) in enumerate(hydro_units.iterrows()):

            elevation = unit['elevation'].values

            if method == 'constant':
                unit_values[:, i_unit] = data_raw

            elif method == 'additive_elevation_gradient':
                if ref_elevation is None:
                    raise ValueError('Reference elevation not provided.')
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
                if ref_elevation is None:
                    raise ValueError('Reference elevation not provided.')
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
                gradient_2 = kwargs.get('gradient_2', None)
                elevation_threshold = kwargs.get('elevation_threshold', None)
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

            else:
                raise ValueError(f'Unknown method: {method}')

        # Check outputs
        if not self._can_be_negative(variable):
            unit_values[unit_values < 0] = 0

        # Store outputs
        if variable in self.data2D.data_name:
            idx_2d = self.data2D.data_name.index(variable)
            self.data2D.data[idx_2d] = unit_values
        else:
            self.data2D.data.append(unit_values)
            self.data2D.data_name.append(variable)
            self.data2D.time = self.data1D.time

    def _apply_spatialization_from_gridded_data(
            self,
            variable: str,
            method: str = 'default',
            **kwargs
    ):
        variable = self.get_variable_enum(variable)

        # Specify default methods
        if method == 'default':
            method = 'regrid_from_netcdf'

        if method == 'regrid_from_netcdf':
            path = kwargs.get('path', '')
            file_pattern = kwargs.get('file_pattern', None)
            data_crs = kwargs.get('data_crs', None)
            var_name = kwargs.get('var_name', '')
            dim_time = kwargs.get('dim_time', 'time')
            dim_x = kwargs.get('dim_x', 'x')
            dim_y = kwargs.get('dim_y', 'y')
            raster_hydro_units = kwargs.get('raster_hydro_units', '')
            if variable in {self.Variable.P, self.Variable.T}:
                apply_data_gradient = kwargs.get('apply_data_gradient', True)
                if variable == self.Variable.P:
                    gradient_type = kwargs.get('gradient_type', 'multiplicative')
                elif variable == self.Variable.T:
                    gradient_type = kwargs.get('gradient_type', 'additive')
                else:
                    gradient_type = kwargs.get('gradient_type', 'additive')
            else:
                apply_data_gradient = kwargs.get('apply_data_gradient', False)
                gradient_type = kwargs.get('gradient_type', 'additive')

            dem_path = None
            if apply_data_gradient:
                if self.catchment is None:
                    raise ValueError("apply_data_gradient is True, but no catchment "
                                     "is defined. The catchment is required to "
                                     "retrieve the elevation-based gradients.")
                if self.catchment.dem is None:
                    raise ValueError("apply_data_gradient is True, but no DEM is "
                                     "defined in the catchment. The DEM is required "
                                     "to retrieve the elevation-based gradients.")

                dem_path = self.catchment.dem.files
                # Drop items with .aux.xml extension
                dem_path = [p for p in dem_path if not p.endswith('.aux.xml')]
                if len(dem_path) > 1:
                    raise ValueError("apply_data_gradient is True, but the catchment "
                                     "contains multiple DEM files. Only one DEM file "
                                     "is supported for the elevation-based gradients.")
                dem_path = dem_path[0]

            self.data2D.regrid_from_netcdf(
                path,
                file_pattern=file_pattern,
                data_crs=data_crs,
                var_name=var_name,
                dim_time=dim_time,
                dim_x=dim_x,
                dim_y=dim_y,
                hydro_units=self.hydro_units,
                raster_hydro_units=raster_hydro_units,
                apply_data_gradient=apply_data_gradient,
                gradient_type=gradient_type,
                dem_path=dem_path
            )
            self.data2D.data_name.append(variable)
        else:
            raise ValueError(f'Unknown method: {method}')

    def _apply_pet_computation(self, method: str, use: list[str], **kwargs):
        if not hb.has_pyet:
            raise ImportError("pyet is required to do this.")

        pyet_args = {}

        use_unit_elevation = False
        use_unit_latitude = False
        if 'elevation' in use:
            use_unit_elevation = True
        if 'latitude' in use or 'lat' in use:
            # If latitude provided in the arguments
            if 'latitude' in kwargs:
                pyet_args['lat'] = kwargs['latitude'] * TO_RAD
            elif 'lat' in kwargs:
                pyet_args['lat'] = kwargs['lat'] * TO_RAD
            else:
                use_unit_latitude = True

        use = self._remove_lat_elevation_options(use)
        self._check_variables_available(use)

        # Loop over the hydro units to compute the PET (pyet xarray implementation is
        # not working as expected in multiplicative operations)
        pet = np.zeros((len(self.data2D.time), len(self.hydro_units)))
        for i_unit, (_, unit) in enumerate(self.hydro_units.iterrows()):
            if use_unit_elevation:
                pyet_args['elevation'] = unit['elevation'].values
            if use_unit_latitude:
                pyet_args['lat'] = unit['latitude'].values * TO_RAD
            pyet_args = self._set_pyet_variables_data(pyet_args, use, i_unit)
            pet[:, i_unit] = self._compute_pet(method, pyet_args)

        # Store outputs
        if self.Variable.PET not in self.data2D.data_name:
            self.data2D.data.append(pet)
            self.data2D.data_name.append(self.Variable.PET)
        else:
            idx = self.data2D.data_name.index(self.Variable.PET)
            self.data2D.data[idx] = pet

    @staticmethod
    def _compute_pet(method: str, pyet_args: dict) -> np.ndarray:
        if method in ['Penman', 'penman']:
            return hb.pyet.penman(**pyet_args)
        elif method in ['Penman-Monteith', 'pm']:
            return hb.pyet.pm(**pyet_args)
        elif method in ['ASCE-PM', 'pm_asce']:
            return hb.pyet.pm_asce(**pyet_args)
        elif method in ['FAO-56', 'pm_fao56']:
            return hb.pyet.pm_fao56(**pyet_args)
        elif method in ['Priestley-Taylor', 'priestley_taylor']:
            return hb.pyet.priestley_taylor(**pyet_args)
        elif method in ['Kimberly-Penman', 'kimberly_penman']:
            return hb.pyet.kimberly_penman(**pyet_args)
        elif method in ['Thom-Oliver', 'thom_oliver']:
            return hb.pyet.thom_oliver(**pyet_args)
        elif method in ['Blaney-Criddle', 'blaney_criddle']:
            return hb.pyet.blaney_criddle(**pyet_args)
        elif method in ['Hamon', 'hamon']:
            return hb.pyet.hamon(**pyet_args)
        elif method in ['Romanenko', 'romanenko']:
            return hb.pyet.romanenko(**pyet_args)
        elif method in ['Linacre', 'linacre']:
            return hb.pyet.linacre(**pyet_args)
        elif method in ['Haude', 'haude']:
            return hb.pyet.haude(**pyet_args)
        elif method in ['Turc', 'turc']:
            return hb.pyet.turc(**pyet_args)
        elif method in ['Jensen-Haise', 'jensen_haise']:
            return hb.pyet.jensen_haise(**pyet_args)
        elif method in ['McGuinness-Bordne', 'mcguinness_bordne']:
            return hb.pyet.mcguinness_bordne(**pyet_args)
        elif method in ['Hargreaves', 'hargreaves']:
            return hb.pyet.hargreaves(**pyet_args)
        elif method in ['FAO-24 radiation', 'fao_24']:
            return hb.pyet.fao_24(**pyet_args)
        elif method in ['Abtew', 'abtew']:
            return hb.pyet.abtew(**pyet_args)
        elif method in ['Makkink', 'makkink']:
            return hb.pyet.makkink(**pyet_args)
        elif method in ['Oudin', 'oudin']:
            return hb.pyet.oudin(**pyet_args)
        else:
            raise ValueError(f'Unknown PET method: {method}')

    def _set_pyet_variables_data(
            self,
            pyet_args: dict,
            use: list[str],
            i_unit: int
    ) -> dict:
        for v in use:
            v = self.get_variable_enum(v)
            idx = self.data2D.data_name.index(v)
            pyet_var_name = {
                self.Variable.T: 'tmean',
                self.Variable.T_MIN: 'tmin',
                self.Variable.T_MAX: 'tmax',
                self.Variable.T_DEW_POINT: 'tdew',
                self.Variable.RH: 'rh',
                self.Variable.RH_MIN: 'rhmin',
                self.Variable.RH_MAX: 'rhmax',
                self.Variable.R_NET: 'rn',
                self.Variable.R_SOLAR: 'rs',
                self.Variable.WIND: 'wind',
                self.Variable.PRESSURE: 'pressure',
            }

            pyet_args[pyet_var_name.get(v)] = pd.Series(
                self.data2D.data[idx][:, i_unit], index=self.data2D.time)

        return pyet_args

    def _check_variables_available(self, use: list[str]):
        # Check if all variables are available
        use = [self.get_variable_enum(v) for v in use]
        for v in use:
            if v not in self.data2D.data_name:
                raise ValueError(f"Variable {v} is not available.")

    @staticmethod
    def _remove_lat_elevation_options(use: list[str]):
        # Remove latitude and elevation from the list
        use = [u for u in use if u not in ['latitude', 'lat', 'elevation']]
        return use
