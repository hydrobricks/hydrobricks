from __future__ import annotations
import logging
import sys
from typing import TYPE_CHECKING, Any

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

from hydrobricks import Dataset, pyet
from hydrobricks._constants import TO_RAD
from hydrobricks._optional import HAS_PYET, HAS_NETCDF
from hydrobricks._exceptions import DataError, ForcingError, DependencyError
from hydrobricks.parameters import ParameterSet
from hydrobricks.time_series import TimeSeries1D, TimeSeries2D

logger = logging.getLogger(__name__)

if TYPE_CHECKING:
    from hydrobricks.catchment import Catchment
    from hydrobricks.hydro_units import HydroUnits


class Forcing:
    """Class for managing forcing (meteorological) data for hydrological models."""

    if sys.version_info < (3, 11):
        StrEnumClass = LowercaseStrEnum
    else:
        StrEnumClass = StrEnum

    class Variable(StrEnumClass):
        """Enumeration of supported meteorological variables."""
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
    ) -> None:
        """
        Initialize Forcing object for a spatial entity.

        Parameters
        ----------
        spatial_entity
            Either a HydroUnits or Catchment object defining the spatial structure.

        Raises
        ------
        TypeError
            If spatial_entity is not HydroUnits or Catchment.
        ValueError
            If the hydro_units object is empty.
        """
        # Import here to avoid circular imports
        from hydrobricks.catchment import Catchment
        from hydrobricks.hydro_units import HydroUnits

        if isinstance(spatial_entity, HydroUnits):
            hydro_units = spatial_entity
            catchment = None
        elif isinstance(spatial_entity, Catchment):
            hydro_units = spatial_entity.hydro_units
            catchment = spatial_entity
        else:
            raise ConfigurationError(
                f'The spatial_entity argument must be a HydroUnits or '
                f'Catchment object, not {type(spatial_entity)}.',
                item_name='spatial_entity',
                item_value=type(spatial_entity).__name__,
                reason='Invalid type'
            )

        # Check hydro units
        if len(hydro_units.hydro_units) == 0:
            raise ConfigurationError(
                'The hydro_units object contains no hydrological units.',
                reason='Empty hydro units'
            )

        super().__init__()
        self.data1D: TimeSeries1D = TimeSeries1D()
        self.data2D: TimeSeries2D = TimeSeries2D()
        self.catchment: Catchment | None = catchment
        self.hydro_units: pd.DataFrame = hydro_units.hydro_units
        self._operations: list[dict[str, Any]] = []
        self._is_initialized: bool = False

    def is_initialized(self) -> bool:
        """
        Check if the forcing has been initialized.

        Returns
        -------
        bool
            True if the forcing has been initialized, False otherwise.
        """
        return self._is_initialized

    def get_variable_enum(self, variable: str) -> Variable:
        """
        Match a variable name string to the corresponding Variable enum value.

        Parameters
        ----------
        variable
            Variable name or alias (e.g., 'precipitation', 'precip', 'p', 'P').

        Returns
        -------
        Variable
            The corresponding Variable enum value.

        Raises
        ------
        ValueError
            If the variable name is not recognized.

        Examples
        --------
        >>> forcing = Forcing(hydro_units)
        >>> var = forcing.get_variable_enum('precip')
        >>> var == Forcing.Variable.P
        True
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
            raise ForcingError(
                f'Variable {variable} is not recognized.',
                variable=variable
            )

    def _can_be_negative(self, variable: Variable) -> bool:
        """
        Check if a given variable can have negative values.

        Parameters
        ----------
        variable
            The Variable to check.

        Returns
        -------
        bool
            True if the variable can be negative, False otherwise.

        Raises
        ------
        ValueError
            If the variable is not recognized.
        """
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
            raise ForcingError(
                f'Undefined if variable {variable} can be negative.',
                variable=variable
            )

    def load_station_data_from_csv(
            self,
            path: str | Path,
            column_time: str,
            time_format: str,
            content: dict[str, str] | None = None
    ) -> None:
        """
        Read 1D time series data from CSV file for a single station.

        Parameters
        ----------
        path
            Path to the CSV file containing station data.
        column_time
            Column name containing the time values.
        time_format
            Format string for parsing time values (e.g., '%Y-%m-%d').
        content
            Dictionary mapping variable names/aliases to CSV column names.
            Example: {'precipitation': 'Precipitation (mm)', 'temperature': 'Temp (C)'}
            Default: None

        Raises
        ------
        FileNotFoundError
            If the CSV file does not exist.
        KeyError
            If required columns are not found in the CSV file.

        Examples
        --------
        >>> forcing.load_station_data_from_csv(
        ...     'weather.csv',
        ...     'Date',
        ...     '%Y-%m-%d',
        ...     {'precipitation': 'P (mm)', 'temperature': 'T (C)'}
        ... )
        """
        # Change the variable names (key) to the enum corresponding values
        for key in list(content.keys()):
            enum_val = self.get_variable_enum(key)
            content[enum_val] = content.pop(key)

        self.data1D.load_from_csv(path, column_time, time_format, content)

    def correct_station_data(self, **kwargs: Any) -> None:
        """
        Define prior correction operations to apply to forcing data.

        Parameters
        ----------
        **kwargs
            Keyword arguments defining the correction operation.
            Required keys:
            - variable : str
                Name of the variable to correct.
            - method : str
                Correction method: 'additive' or 'multiplicative'
            - correction_factor : float
                Value of the correction factor (to add or multiply).

        Examples
        --------
        >>> forcing.correct_station_data(
        ...     variable='temperature',
        ...     method='additive',
        ...     correction_factor=0.5  # Add 0.5 degrees
        ... )
        """
        kwargs['type'] = 'prior_correction'
        self._operations.append(kwargs)

    def spatialize_from_station_data(self, **kwargs: Any) -> None:
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

    def spatialize_from_gridded_data(self, **kwargs: Any) -> None:
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

    def compute_pet(self, **kwargs: Any) -> None:
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

        Raises
        ------
        DependencyError
            If pyet is not installed.
        ValueError
            If required variables are not available.
        """
        if not HAS_PYET:
            raise DependencyError(
                "PyEt is required for PET computation.",
                package_name='pyet',
                operation='Forcing.compute_pet',
                install_command='pip install pyet'
            )

        kwargs['type'] = 'compute_pet'

        self._operations.append(kwargs)

    def apply_operations(
            self,
            parameters: ParameterSet | None = None,
            apply_to_all: bool = True
    ) -> None:
        """
        Apply the pre-defined operations.

        Executes all spatialization, correction, and PET computation operations
        that were previously defined. Operations are applied in a fixed order:
        1. Prior corrections
        2. Station data spatialization
        3. Gridded data spatialization
        4. PET computation

        Parameters
        ----------
        parameters
            The parameter object instance. Required if operations reference parameters
            using the 'param:' prefix.
        apply_to_all
            If True, the operations will be applied to all variables. If False, the
            operations will only be applied to the variables related to parameters
            defined in the parameters.allow_changing list. This is useful to avoid
            re-applying, during the calibration phase, operations that have already
            been applied previously. Default: True

        Raises
        ------
        ValueError
            If operations reference parameters but no parameter object is provided.
        """
        logger.debug(f"Applying forcing operations: apply_to_all={apply_to_all}, "
                     f"parameters={'provided' if parameters else 'None'}")

        # The operations will be applied in the order defined in the list
        operation_types = [
            'prior_correction',
            'spatialize_from_station',
            'spatialize_from_grid',
            'compute_pet'
        ]

        for operation_type in operation_types:
            logger.debug(f"  Applying {operation_type} operations")
            self._apply_operations_of_type(operation_type, parameters, apply_to_all)

        logger.debug("All forcing operations completed successfully")
        self._is_initialized = True

    def save_as(self, path: str | Path, max_compression: bool = False) -> None:
        """
        Create a netCDF file with the forcing data.

        Saves the 2D spatialized forcing data to a netCDF4 file with the structure
        suitable for later loading with load_from().

        Parameters
        ----------
        path
            Path of the file to create.
        max_compression
            Option to allow maximum compression for data in file. When True, uses
            compression with least_significant_digit=3 for better storage efficiency.
            Default: False

        Raises
        ------
        ImportError
            If netcdf4 is not installed.

        Notes
        -----
        If apply_operations() has not been called, it will be called automatically
        before saving to ensure data is properly spatialized.
        """
        if not HAS_NETCDF:
            raise ImportError("netcdf4 is required to do this.")

        if not self.is_initialized():
            logger.info("Applying operations before saving...")
            self.apply_operations()
            self._is_initialized = True

        time = self.data2D.get_dates_as_mjd()

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

    def load_from(self, path: str | Path) -> None:
        """
        Load data from a netCDF file created using save_as().

        Reads a previously saved netCDF file containing spatialized forcing data
        and loads it into the Forcing object's data2D structure.

        Parameters
        ----------
        path
            Path of the file to read.

        Raises
        ------
        ImportError
            If netcdf4 is not installed.
        ValueError
            If the hydro units in the file don't match the Forcing object's hydro units.

        Notes
        -----
        The loaded data will have the same structure as if it was created through
        spatialization operations. The Forcing object will be marked as initialized
        after successful loading.
        """
        if not HAS_NETCDF:
            raise ImportError("netcdf4 is required to do this.")

        # Open netCDF file
        nc = Dataset(path, 'r', 'NETCDF4')

        # Check that hydro units are the same
        hydro_units_nc = nc.variables['id'][:]
        hydro_units_def = self.hydro_units[('id', '-')].values
        if not np.array_equal(hydro_units_nc, hydro_units_def):
            raise DataError(
                "The hydrological units in the netCDF file are not "
                "the same as those in the forcing object. The netCDF file "
                "contains hydrological units with ids: "
                f"{hydro_units_nc}. The model contains hydrological "
                f"units with ids: {hydro_units_def}.",
                data_type='netCDF hydro units',
                reason='ID mismatch'
            )

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
        """
        Calculate the catchment-average total precipitation.

        Computes the weighted average precipitation across all hydro units based on
        their areas.

        Returns
        -------
        float
            Total precipitation in mm (or original units if data is in different units).
        """
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
    ) -> None:
        """
        Apply operations of a specific type in sequence.

        Iterates through operations of the given type and applies them with the
        appropriate parameters. Substitutes parameter references (param: prefix)
        with actual parameter values.

        Parameters
        ----------
        operation_type
            Type of operations to apply ('prior_correction', 'spatialize_from_station',
            'spatialize_from_grid', or 'compute_pet').
        parameters
            Parameter object for substituting parameter references. Required if
            operations use 'param:' prefix.
        apply_to_all
            If False, only apply operations for variables in allow_changing list.

        Raises
        ------
        ValueError
            If operation references parameters but no parameter object provided.
        """
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
                        raise ConfigurationError(
                            'A parameters object must be provided '
                            'to apply the operations as it is '
                            f'required by the "{value}" option.',
                            item_name='forcing',
                            reason='Missing required parameter'
                        )
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
                    raise ConfigurationError(
                        f'Unknown operation type: {operation_type}',
                        item_name='operation_type',
                        item_value=operation_type,
                        reason='Unknown operation'
                    )

    def _apply_prior_correction(
            self,
            variable: str,
            method: str = 'multiplicative',
            **kwargs: Any
    ) -> None:
        """
        Apply a prior correction to 1D station data.

        Modifies the loaded station data by applying an additive or multiplicative
        correction factor to a specific variable.

        Parameters
        ----------
        variable
            Name or alias of the variable to correct.
        method
            Correction method: 'multiplicative' (default) or 'additive'.
        **kwargs
            Must contain 'correction_factor' key with the factor value.

        Raises
        ------
        ValueError
            If correction_factor not provided or unknown method used.
        """
        variable = self.get_variable_enum(variable)
        idx = self.data1D.data_name.index(variable)

        # Extract kwargs (None if not provided)
        correction_factor = kwargs.get('correction_factor', None)
        if correction_factor is None:
            raise ForcingError(
                'Correction factor not provided.',
                variable=str(variable),
                method=method
            )

        if method == 'multiplicative':
            correction_factor = kwargs['correction_factor']
            self.data1D.data[idx] *= correction_factor
        elif method == 'additive':
            correction_factor = kwargs['correction_factor']
            self.data1D.data[idx] += correction_factor
        else:
            raise ForcingError(
                f'Unknown correction method: {method}',
                variable=str(variable),
                method=method
            )

    def _apply_spatialization_from_station_data(
            self,
            variable: str,
            method: str = 'default',
            **kwargs: Any
    ) -> None:
        """
        Spatialize 1D station data to 2D hydro units using elevation gradients.

        Converts point observations from a station to distributed values across
        hydro units using elevation-dependent methods (additive or multiplicative
        gradients).

        Parameters
        ----------
        variable
            Name or alias of the variable to spatialize.
        method
            Spatialization method: 'constant', 'additive_elevation_gradient',
            'multiplicative_elevation_gradient', or
            'multiplicative_elevation_threshold_gradients'.
        **kwargs
            Required/optional parameters depend on the method:
            - ref_elevation: Reference station elevation (required for gradient methods)
            - gradient: Elevation gradient value(s)
            - gradient_2: Second gradient for threshold method

        Raises
        ------
        ValueError
            If required parameters are missing or invalid method specified.
        """
        # Checking that the correction_factor option is not used here anymore
        if 'correction_factor' in kwargs:
            raise ConfigurationError(
                'The correction_factor option is to be used only '
                'in the correct_station_data() method.',
                item_name='correction_factor',
                reason='Wrong method for this option'
            )

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
                raise ForcingError(
                    f'Unknown default method for variable: {variable}',
                    variable=str(variable),
                    method='spatialization'
                )

        # Extract kwargs (None if not provided)
        ref_elevation = kwargs.get('ref_elevation', None)
        gradient = kwargs.get('gradient', None)

        # Check inputs
        if gradient is None:
            gradient = kwargs.get('gradient_1', None)

        if isinstance(gradient, list):
            if len(gradient) not in [1, 12]:
                raise ConfigurationError(
                    f'The gradient should have a length of 1 or 12. '
                    f'Here: {len(gradient)}',
                    item_name='gradient',
                    item_value=len(gradient),
                    reason='Invalid length'
                )

        # Apply methods
        for i_unit, (_, unit) in enumerate(hydro_units.iterrows()):

            elevation = unit['elevation'].values

            if method == 'constant':
                unit_values[:, i_unit] = data_raw

            elif method == 'additive_elevation_gradient':
                if ref_elevation is None:
                    raise ConfigurationError(
                        'Reference elevation not provided.',
                        item_name='ref_elevation',
                        reason='Missing required parameter'
                    )
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
                    raise ConfigurationError(
                        f'Wrong gradient format: {gradient}',
                        item_name='gradient',
                        item_value=gradient,
                        reason='Invalid format'
                    )

            elif method == 'multiplicative_elevation_gradient':
                if ref_elevation is None:
                    raise ConfigurationError(
                        'Reference elevation not provided.',
                        item_name='ref_elevation',
                        reason='Missing required parameter'
                    )
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
                    raise ConfigurationError(
                        f'Wrong gradient format: {gradient}',
                        item_name='gradient',
                        item_value=gradient,
                        reason='Invalid format'
                    )

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
                raise ForcingError(
                    f'Unknown method: {method}',
                    variable=str(variable),
                    method=method
                )

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
            **kwargs: Any
    ) -> None:
        """
        Spatialize 2D gridded data to hydro units with optional elevation gradients.

        Regrids spatial meteorological data from netCDF files to hydro units,
        optionally applying elevation-based corrections.

        Parameters
        ----------
        variable
            Name or alias of the variable to spatialize.
        method
            Spatialization method. Default is 'regrid_from_netcdf'.
        **kwargs
            Parameters for the regridding operation including:
            - path: Path to netCDF file(s)
            - file_pattern: Glob pattern for multiple files
            - data_crs: CRS of the data (as EPSG code)
            - var_name: Name of variable in the netCDF file
            - raster_hydro_units: Path to hydro unit IDs raster
            - apply_data_gradient: Whether to apply elevation gradients
            - gradient_type: 'additive' or 'multiplicative'

        Raises
        ------
        ValueError
            If required parameters missing or invalid method specified.
        """
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
                    raise DataError(
                        "apply_data_gradient is True, but no catchment "
                        "is defined. The catchment is required to "
                        "retrieve the elevation-based gradients.",
                        data_type='catchment',
                        reason='Missing catchment'
                    )
                if self.catchment.dem is None:
                    raise DataError(
                        "apply_data_gradient is True, but no DEM is "
                        "defined in the catchment. The DEM is required "
                        "to retrieve the elevation-based gradients.",
                        data_type='DEM',
                        reason='Missing DEM'
                    )

                dem_path = self.catchment.dem.files
                # Drop items with .aux.xml extension
                dem_path = [p for p in dem_path if not p.endswith('.aux.xml')]
                if len(dem_path) > 1:
                    raise DataError(
                        "apply_data_gradient is True, but the catchment "
                        "contains multiple DEM files. Only one DEM file "
                        "is supported for the elevation-based gradients.",
                        data_type='DEM',
                        reason='Multiple DEM files'
                    )
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
            raise ForcingError(
                f'Unknown method: {method}',
                variable=str(variable),
                method=method
            )

    def _apply_pet_computation(self, method: str, use: list[str], **kwargs: Any) -> None:
        """
        Compute potential evapotranspiration (PET) for all hydro units.

        Uses the pyet library to compute PET based on available meteorological data
        and the specified method.

        Parameters
        ----------
        method
            Name of the PET computation method (e.g., 'penman', 'pm', 'hargreaves').
        use
            List of variables to use for computation. Can include 'elevation', 'latitude'/'lat'.
        **kwargs
            Additional parameters for the PET method and unit-specific data.

        Raises
        ------
        ImportError
            If pyet library is not installed.
        ValueError
            If required variables are not available.
        """
        if not HAS_PYET:
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
        """
        Compute PET using the pyet library.

        Routes the computation to the appropriate pyet method based on the
        specified method name.

        Parameters
        ----------
        method
            Name of the PET computation method.
        pyet_args
            Dictionary of arguments to pass to the pyet method.

        Returns
        -------
        np.ndarray
            Array of computed PET values.

        Raises
        ------
        ValueError
            If the method name is not recognized.
        """
        if method in ['Penman', 'penman']:
            return pyet.penman(**pyet_args)
        elif method in ['Penman-Monteith', 'pm']:
            return pyet.pm(**pyet_args)
        elif method in ['ASCE-PM', 'pm_asce']:
            return pyet.pm_asce(**pyet_args)
        elif method in ['FAO-56', 'pm_fao56']:
            return pyet.pm_fao56(**pyet_args)
        elif method in ['Priestley-Taylor', 'priestley_taylor']:
            return pyet.priestley_taylor(**pyet_args)
        elif method in ['Kimberly-Penman', 'kimberly_penman']:
            return pyet.kimberly_penman(**pyet_args)
        elif method in ['Thom-Oliver', 'thom_oliver']:
            return pyet.thom_oliver(**pyet_args)
        elif method in ['Blaney-Criddle', 'blaney_criddle']:
            return pyet.blaney_criddle(**pyet_args)
        elif method in ['Hamon', 'hamon']:
            return pyet.hamon(**pyet_args)
        elif method in ['Romanenko', 'romanenko']:
            return pyet.romanenko(**pyet_args)
        elif method in ['Linacre', 'linacre']:
            return pyet.linacre(**pyet_args)
        elif method in ['Haude', 'haude']:
            return pyet.haude(**pyet_args)
        elif method in ['Turc', 'turc']:
            return pyet.turc(**pyet_args)
        elif method in ['Jensen-Haise', 'jensen_haise']:
            return pyet.jensen_haise(**pyet_args)
        elif method in ['McGuinness-Bordne', 'mcguinness_bordne']:
            return pyet.mcguinness_bordne(**pyet_args)
        elif method in ['Hargreaves', 'hargreaves']:
            return pyet.hargreaves(**pyet_args)
        elif method in ['FAO-24 radiation', 'fao_24']:
            return pyet.fao_24(**pyet_args)
        elif method in ['Abtew', 'abtew']:
            return pyet.abtew(**pyet_args)
        elif method in ['Makkink', 'makkink']:
            return pyet.makkink(**pyet_args)
        elif method in ['Oudin', 'oudin']:
            return pyet.oudin(**pyet_args)
        else:
            raise ForcingError(
                f'Unknown PET method: {method}',
                variable='PET',
                method=method
            )

    def _set_pyet_variables_data(
            self,
            pyet_args: dict,
            use: list[str],
            i_unit: int
    ) -> dict:
        """
        Set meteorological variables for pyet computation.

        Extracts spatialized meteorological data for a specific hydro unit and
        adds it to the pyet arguments dictionary using pyet-compatible variable names.

        Parameters
        ----------
        pyet_args
            Dictionary of arguments for pyet. Will be updated with variable data.
        use
            List of variable names to extract and add.
        i_unit
            Index of the hydro unit.

        Returns
        -------
        dict
            Updated pyet_args dictionary with meteorological variables added.
        """
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

    def _check_variables_available(self, use: list[str]) -> None:
        """
        Validate that all required variables are available in data2D.

        Parameters
        ----------
        use
            List of variable names to check.

        Raises
        ------
        ValueError
            If any variable is not available in data2D.
        """
        # Check if all variables are available
        use = [self.get_variable_enum(v) for v in use]
        for v in use:
            if v not in self.data2D.data_name:
                raise DataError(
                    f"Variable {v} is not available.",
                    data_type='forcing variable',
                    reason='Variable not in data'
                )

    @staticmethod
    def _remove_lat_elevation_options(use: list[str]) -> list[str]:
        """
        Remove latitude and elevation options from variable list.

        These are special options that don't correspond to meteorological variables
        in the data but rather to spatial parameters.

        Parameters
        ----------
        use
            List of variable names, potentially including 'latitude', 'lat', 'elevation'.

        Returns
        -------
        list[str]
            Filtered list with latitude and elevation removed.
        """
        # Remove latitude and elevation from the list
        use = [u for u in use if u not in ['latitude', 'lat', 'elevation']]
        return use
