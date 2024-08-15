import random

import numpy as np
import pandas as pd

import hydrobricks as hb


class ParameterSet:
    """Class for the parameter sets"""

    def __init__(self):
        self.parameters = pd.DataFrame(
            columns=['component', 'name', 'unit', 'aliases', 'value',
                     'min', 'max', 'default_value', 'mandatory', 'prior'])
        self.constraints = []
        self._allow_changing = []

    @property
    def allow_changing(self):
        return self._allow_changing

    @allow_changing.setter
    def allow_changing(self, allow_changing):
        """
        allow_changing: list
            A list of parameters to assess. Only the parameters in this list will be
            changed. If a parameter is related to data forcing, the spatialization
            will be performed again.
        """
        self._allow_changing = allow_changing

    def define_parameter(self, component, name, unit=None, aliases=None, min_value=None,
                         max_value=None, default_value=None, mandatory=True):
        """
        Define a parameter by setting its properties.

        Parameters
        ----------
        component : str
            The component (brick) name to which the parameter refer (e.g., snowpack,
            glacier, surface_runoff). It can be a string of a list of components when
            the parameter is shared between components (e.g., melt_factor in the
            temperature index method).
        name : str
            The name of the parameter in the C++ code of hydrobricks (e.g.,
            degree_day_factor, response_factor).
        unit : ?str
            The unit of the parameter.
        aliases : list
            Aliases to the parameter name, such as names used in other implementations
            (e.g., kgl, an). Aliases must be unique.
        min_value : float/list
            Minimum value allowed for the parameter.
        max_value : float/list
            Maximum value allowed for the parameter.
        default_value : float/list
            The parameter default value.
        mandatory : bool
            If the parameter needs to be defined or if it can silently use the
            default value.
        """
        value = None
        if not mandatory and default_value is not None:
            value = default_value

        self._check_aliases_uniqueness(aliases)
        self._check_min_max_consistency(min_value, max_value)

        new_row = pd.Series(
            {'component': component, 'name': name, 'unit': unit, 'aliases': aliases,
             'value': value, 'min': min_value, 'max': max_value,
             'default_value': default_value, 'mandatory': mandatory, 'prior': None})

        self.parameters = pd.concat([self.parameters, new_row.to_frame().T],
                                    ignore_index=True)

    def add_aliases(self, parameter_name, aliases):
        """
        Add aliases to a parameter.

        Parameters
        ----------
        parameter_name : str
            The name of the parameter with the related component (e.g.,
            snowpack:degree_day_factor).
        aliases : list, str
            Aliases to the parameter name, such as names used in other implementations
            (e.g., kgl, an). Aliases must be unique.
        """
        if not isinstance(aliases, list):
            aliases = [aliases]
        index = self._get_parameter_index(parameter_name)
        self.parameters.loc[index, 'aliases'] += aliases

    def change_range(self, parameter, min_value, max_value):
        """
        Change the value range of a parameter.

        Parameters
        ----------
        parameter: str
            Name (or alias) of the parameter
        min_value
            New minimum value
        max_value
            New maximum value
        """
        index = self._get_parameter_index(parameter)
        self.parameters.loc[index, 'min'] = min_value
        self.parameters.loc[index, 'max'] = max_value

    def set_prior(self, parameter, prior):
        """
        Change the value range of a parameter.

        Parameters
        ----------
        parameter: str
            Name (or alias) of the parameter
        prior: spotpy.parameter
            The prior distribution (instance of spotpy.parameter)
        """
        if not hb.has_spotpy:
            raise ImportError("spotpy is required to do this.")

        index = self._get_parameter_index(parameter)
        prior.name = parameter
        self.parameters.loc[index, 'prior'] = prior

    def list_constraints(self):
        """
        List the constraints currently defined.
        """
        for constraint in self.constraints:
            print(' '.join(constraint))

    def define_constraint(self, parameter_1, operator, parameter_2):
        """
        Defines a constraint between 2 parameters (e.g., paramA > paramB)

        Parameters
        ----------
        parameter_1 : str
            The name of the first parameter.
        operator : str
            The operator (e.g. '<=').
        parameter_2 : str
            The name of the second parameter.

        Examples
        --------
        parameter_set.define_constraint('paramA', '>=', 'paramB')
        """
        constraint = [parameter_1, operator, parameter_2]
        self.constraints.append(constraint)

    def remove_constraint(self, parameter_1, operator, parameter_2):
        """
        Removes a constraint between 2 parameters (e.g., paramA > paramB)

        Parameters
        ----------
        parameter_1 : str
            The name of the first parameter.
        operator : str
            The operator (e.g. '<=').
        parameter_2 : str
            The name of the second parameter.

        Examples
        --------
        parameter_set.remove_constraint('paramA', '>=', 'paramB')
        """
        for i, constraint in enumerate(self.constraints):
            if parameter_1 == constraint[0] \
                    and operator == constraint[1] \
                    and parameter_2 == constraint[2]:
                del self.constraints[i]
                return

    def constraints_satisfied(self) -> bool:
        """
        Check if the constraints between parameters are satisfied.

        Returns
        -------
        True is constraints are satisfied, False otherwise.
        """
        for constraint in self.constraints:
            # Ignore constraints involving unused parameters
            if not self.has(constraint[0]) or not self.has(constraint[2]):
                continue

            val_1 = self.get(constraint[0])
            operator = constraint[1]
            val_2 = self.get(constraint[2])

            if isinstance(val_1, list) or isinstance(val_2, list):
                raise NotImplementedError

            if operator in ['>', 'gt']:
                if val_1 <= val_2:
                    return False
            elif operator in ['>=', 'ge']:
                if val_1 < val_2:
                    return False
            elif operator in ['<', 'lt']:
                if val_1 >= val_2:
                    return False
            elif operator in ['<=', 'le']:
                if val_1 > val_2:
                    return False

        return True

    def range_satisfied(self) -> bool:
        """
        Check if the parameter value ranges are satisfied.

        Returns
        -------
        True is ranges are satisfied, False otherwise.
        """

        for _, row in self.parameters.iterrows():
            min_value = row['min']
            max_value = row['max']
            value = row['value']

            if value is None:
                return False

            if not isinstance(min_value, list):
                if max_value is not None and value > max_value:
                    return False
                if min_value is not None and value < min_value:
                    return False
            else:
                assert isinstance(max_value, list)
                assert isinstance(value, list)
                for min_v, max_v, val in zip(min_value, max_value, value):
                    if max_v is not None and val > max_v:
                        return False
                    if min_v is not None and val < min_v:
                        return False

        return True

    def set_values(self, values, check_range=True, allow_adapt=False):
        """
        Set the parameter values.

        Parameters
        ----------
        values : dict
            The values must be provided as a dictionary with the parameter name with the
            related component or one of its aliases as the key.
            Example: {'k': 32, 'A': 300} or {'slow_reservoir:capacity': 300}
        check_range : bool
            Check that the parameter value falls into the allowed range.
        allow_adapt : bool
            Allow the parameter values to be adapted to enforce defined constraints
            (e.g.: min, max).
        """
        for key in values:
            index = self._get_parameter_index(key)
            value = values[key]
            if check_range:
                value = self._check_value_range(index, key, value,
                                                allow_adapt=allow_adapt)
            self.parameters.loc[index, 'value'] = value

    def has(self, name):
        """
        Check if a parameter exists.

        Parameters
        ----------
        name : str
            The name of the parameter.

        Returns
        ------
        True if found, False otherwise.
        """
        index = self._get_parameter_index(name, raise_exception=False)
        return index is not None

    def get(self, name):
        """
        Get the value of a parameter by name.

        Parameters
        ----------
        name : str
            The name of the parameter.

        Returns
        ------
        The parameter value.
        """
        index = self._get_parameter_index(name)
        return self.parameters.loc[index, 'value']

    def is_ok(self):
        """
        Check if all the parameters are defined and have a value.

        Returns
        -------
        True if all parameters are defined and have a value, False otherwise.
        """
        for _, row in self.parameters.iterrows():
            if row['value'] is None:
                return False
        return True

    def get_undefined(self):
        """
        Get the undefined parameters.

        Returns
        ------
        A list of the undefined parameter names.
        """
        undefined = []
        for _, row in self.parameters.iterrows():
            if row['value'] is None:
                undefined.append(row['name'])
        return undefined

    def get_model_parameters(self):
        """
        Get the model-only parameters (excluding data-related parameters).
        """
        return self.parameters[self.parameters['component'] != 'data']

    def add_data_parameter(self, name, value=None, min_value=None,
                           max_value=None, unit=None):
        """
        Add a parameter related to the data.

        Parameters
        ----------
        name : str
            The name of the parameter.
        value : float/list
            The parameter value.
        min_value : float/list
            Minimum value allowed for the parameter.
        max_value : float/list
            Maximum value allowed for the parameter.
        unit : str
            The unit of the parameter.
        """
        aliases = [name]

        self._check_aliases_uniqueness(aliases)
        self._check_min_max_consistency(min_value, max_value)

        new_row = pd.Series(
            {'component': 'data', 'name': name, 'unit': unit, 'aliases': aliases,
             'value': value, 'min': min_value, 'max': max_value,
             'default_value': value, 'mandatory': False})

        self.parameters = pd.concat([self.parameters, new_row.to_frame().T],
                                    ignore_index=True)

    def is_for_forcing(self, parameter_name):
        """
        Check if the parameter relates to forcing data.

        Parameters
        ----------
        parameter_name
            The name of the parameter.

        Returns
        -------
        True if relates to forcing data, False otherwise.
        """
        index = self._get_parameter_index(parameter_name)
        return self.parameters.loc[index, 'component'] == 'data'

    def set_random_values(self, parameters):
        """
        Set the provided parameter to random values.

        Parameters
        ----------
        parameters : list
            The name or alias of the parameters to set to random values.
            Example: ['kr', 'A']

        Returns
        -------
        A dataframe with the assigned parameter values.
        """

        # Create a dataframe to return assigned values
        assigned_values = pd.DataFrame(columns=parameters)

        for i in range(1100):
            for key in parameters:
                index = self._get_parameter_index(key)
                min_value = self.parameters.loc[index, 'min']
                max_value = self.parameters.loc[index, 'max']

                if isinstance(min_value, list):
                    if self.parameters.loc[index, 'value'] is None:
                        self.parameters.loc[index, 'value'] = [0] * len(min_value)
                    for (idx, min_val), max_val in zip(enumerate(min_value), max_value):
                        self.parameters.loc[index, 'value'][idx] = \
                            random.uniform(min_val, max_val)
                else:
                    self.parameters.loc[index, 'value'] = random.uniform(
                        min_value, max_value)

                if isinstance(min_value, list):
                    assigned_values.loc[0, key] = self.parameters.loc[
                        index, 'value'].copy()
                else:
                    assigned_values.loc[0, key] = self.parameters.loc[index, 'value']

            if self.constraints_satisfied():
                break

            if i >= 1000:
                raise ValueError('The parameter constraints could not be satisfied.')

        return assigned_values

    def needs_random_forcing(self):
        """
        Check if one of the parameters to assess involves the meteorological data.

        Returns
        -------
        True if one of the parameters to assess involves the meteorological data.
        """
        for param in self.allow_changing:
            if not self.has(param):
                raise ValueError(f'The parameter {param} was not found.')
            if self.is_for_forcing(param):
                return True
        return False

    def get_for_spotpy(self):
        """
        Get the parameters to assess ready to be used in spotpy.

        Returns
        -------
        A list of the parameters as spotpy objects.
        """
        if not hb.has_spotpy:
            raise ImportError("spotpy is required to do this.")

        spotpy_params = []
        for param_name in self.allow_changing:
            index = self._get_parameter_index(param_name)
            param = self.parameters.loc[index]
            if param['prior'] and not np.isnan(param['prior']):
                spotpy_params.append(
                    param['prior']
                )
            else:
                spotpy_params.append(
                    hb.spotpy.parameter.Uniform(param_name, low=param['min'],
                                                high=param['max'])
                )

        return spotpy_params

    def save_as(self, directory, name, file_type='both'):
        """
        Create a configuration file containing the parameter values.

        Such a file can be used when using the command-line version of hydrobricks. It
        contains the model parameter values.

        Parameters
        ----------
        directory : str
            The directory to write the file.
        name : str
            The name of the generated file.
        file_type : file_type
            The type of file to generate: 'json', 'yaml', or 'both'.
        """
        grouped_params = self.parameters.groupby('component', sort=False)
        file_content = {}

        for group_name, group in grouped_params:
            group_content = {}
            for _, row in group.iterrows():
                group_content.update({row['name']: row['value']})
            file_content.update({group_name: group_content})

        hb.utils.dump_config_file(file_content, directory, name, file_type)

    def generate_parameters(self, land_cover_types, land_cover_names, options,
                            structure):
        """
        Generate a parameters object for the provided model options and structure.

        Parameters
        ----------
        land_cover_types : list
            The land cover types.
        land_cover_names : list
            The land cover names.
        options : dict
            The model options.
        structure : dict
            The model structure.
        """
        # General parameters
        self._generate_snow_parameters(options)

        # Parameters for the glaciers
        self._generate_glacier_parameters(land_cover_types, land_cover_names,
                                          structure)

        # Parameters for the different bricks
        for key, brick in structure.items():
            self._generate_brick_parameters(key, brick)
            self._generate_process_parameters(key, brick)

    def _generate_process_parameters(self, key, brick):
        if 'processes' not in brick:
            return

        skip_processes = [
            # No parameters
            'infiltration:socont', 'outflow:rest_direct', 'outflow:direct', 'et:socont',
            'overflow',
            # Already defined for snowpacks + glaciers
            'melt:degree_day', 'melt:degree_day_aspect', 'melt:temperature_index'
        ]

        for _, process in brick['processes'].items():
            if process['kind'] in skip_processes:
                continue
            elif process['kind'] == 'outflow:linear':
                self.define_parameter(
                    component=key, name='response_factor', unit='1/d', aliases=[],
                    min_value=0.001, max_value=1, mandatory=True)
            elif process['kind'] == 'runoff:socont':
                self.define_parameter(
                    component=key, name='beta', unit='m^(4/3)/s', aliases=['beta'],
                    min_value=100, max_value=30000, mandatory=True)
            elif process['kind'] == 'outflow:percolation':
                self.define_parameter(
                    component=key, name='percolation_rate', unit='mm/d',
                    aliases=['percol'], min_value=0, max_value=10, mandatory=True)
            else:
                raise RuntimeError(f"The process {process['kind']} is not recognised "
                                   f"in parameters generation.")

    def _generate_brick_parameters(self, key, brick):
        if 'parameters' not in brick:
            return

        for param_name, _ in brick['parameters'].items():
            if param_name in ['no_melt_when_snow_cover', 'infinite_storage']:
                continue
            elif param_name == 'capacity':
                self.define_parameter(
                    component=key, name=param_name, unit='mm', aliases=[],
                    min_value=0, max_value=3000, mandatory=True)
            else:
                raise RuntimeError(f"The parameter {param_name} is not recognised in "
                                   f"parameters generation.")

    def _generate_glacier_parameters(self, land_cover_types, land_cover_names,
                                     structure):
        if 'glacier' not in land_cover_types:
            return

        glacier_names = [cover_name for cover_type, cover_name in
                         zip(land_cover_types, land_cover_names) if
                         cover_type == 'glacier']

        for i, cover_name in enumerate(glacier_names):
            melt_method = structure[cover_name]['processes']['melt']['kind']

            if len(glacier_names) == 1:
                t_aliases = ['melt_t_ice']
            else:
                t_aliases = [f'melt_t_ice_{cover_name.replace("-", "_")}',
                             f'melt_t_ice_{i}']

            if melt_method == 'melt:degree_day':
                if len(glacier_names) == 1:
                    a_aliases = ['a_ice']
                else:
                    a_aliases = [f'a_ice_{cover_name.replace("-", "_")}',
                                 f'a_ice_{i}']

                self.define_parameter(
                    component=cover_name, name='degree_day_factor',
                    unit='mm/d/°C', aliases=a_aliases, min_value=5, max_value=20)
                self.define_parameter(
                    component=cover_name, name='melting_temperature',
                    unit='°C', aliases=t_aliases, min_value=0, max_value=5,
                    default_value=0, mandatory=False)

                if len(glacier_names) > 1 and cover_name == 'glacier_debris':
                    self.define_constraint('a_ice_glacier_debris', '<',
                                           'a_ice_glacier_ice')
                self.define_constraint('a_snow', '<', a_aliases[0])

            elif melt_method == 'melt:degree_day_aspect':
                if len(glacier_names) == 1:
                    a_n_aliases = ['a_ice_n']
                    a_s_aliases = ['a_ice_s']
                    a_ew_aliases = ['a_ice_ew']
                else:
                    a_n_aliases = [f'a_ice_n_{cover_name.replace("-", "_")}',
                                   f'a_ice_n_{i}']
                    a_s_aliases = [f'a_ice_s_{cover_name.replace("-", "_")}',
                                   f'a_ice_s_{i}']
                    a_ew_aliases = [f'a_ice_ew_{cover_name.replace("-", "_")}',
                                    f'a_ice_ew_{i}']

                self.define_parameter(
                    component=cover_name, name='degree_day_factor_n',
                    unit='mm/d/°C', aliases=a_n_aliases, min_value=1, max_value=20)
                self.define_parameter(
                    component=cover_name, name='degree_day_factor_s',
                    unit='mm/d/°C', aliases=a_s_aliases, min_value=5, max_value=20)
                self.define_parameter(
                    component=cover_name, name='degree_day_factor_ew',
                    unit='mm/d/°C', aliases=a_ew_aliases, min_value=5, max_value=20)
                self.define_parameter(
                    component=cover_name, name='melting_temperature',
                    unit='°C', aliases=t_aliases, min_value=0, max_value=5,
                    default_value=0, mandatory=False)

                if len(glacier_names) > 1 and cover_name == 'glacier_debris':
                    self.define_constraint('a_ice_n_glacier_debris', '<',
                                           'a_ice_n_glacier_ice')
                    self.define_constraint('a_ice_s_glacier_debris', '<',
                                           'a_ice_s_glacier_ice')
                    self.define_constraint('a_ice_ew_glacier_debris', '<',
                                           'a_ice_ew_glacier_ice')
                self.define_constraint('a_snow', '<', a_n_aliases[0])
                self.define_constraint('a_snow', '<', a_s_aliases[0])
                self.define_constraint('a_snow', '<', a_ew_aliases[0])
                self.define_constraint('a_snow_n', '<', a_n_aliases[0])
                self.define_constraint('a_snow_s', '<', a_s_aliases[0])
                self.define_constraint('a_snow_ew', '<', a_ew_aliases[0])

            elif melt_method == 'melt:temperature_index':
                if len(glacier_names) == 1:
                    r_aliases = ['r_ice']
                else:
                    r_aliases = [f'r_ice_{cover_name.replace("-", "_")}',
                                 f'r_ice_{i}']

                self.define_parameter(
                    component=cover_name, name='radiation_coefficient',
                    unit='m2/W*mm/d/°C', aliases=r_aliases, min_value=0, max_value=1)
                self.define_parameter(
                    component=cover_name, name='melting_temperature',
                    unit='°C', aliases=t_aliases, min_value=0, max_value=5,
                    default_value=0, mandatory=False)

                if len(glacier_names) > 1 and cover_name == 'glacier_debris':
                    self.define_constraint('r_ice_glacier_debris', '<',
                                           'r_ice_glacier_ice')
                self.define_constraint('r_snow', '<', r_aliases[0])

            else:
                raise RuntimeError(f"The glacier melt method {melt_method} is not "
                                   f"recognised in parameters generation.")

    def _generate_snow_parameters(self, options):
        if 'snow_melt_process' in options or 'with_snow' in options:
            self.define_parameter(
                component='snow_rain_transition', name='transition_start', unit='°C',
                aliases=['prec_t_start'], min_value=-2, max_value=2, default_value=0,
                mandatory=False)

            self.define_parameter(
                component='snow_rain_transition', name='transition_end', unit='°C',
                aliases=['prec_t_end'], min_value=0, max_value=4, default_value=2,
                mandatory=False)

            if 'snow_melt_process' in options:
                if options['snow_melt_process'] == 'melt:degree_day':
                    self.define_parameter(
                        component='type:snowpack', name='degree_day_factor',
                        unit='mm/d/°C', aliases=['a_snow'], min_value=2, max_value=12)
                    self.define_parameter(
                        component='type:snowpack', name='melting_temperature',
                        unit='°C', aliases=['melt_t_snow'], min_value=0, max_value=5,
                        default_value=0, mandatory=False)
                elif options['snow_melt_process'] == 'melt:degree_day_aspect':
                    self.define_parameter(
                        component='type:snowpack', name='degree_day_factor_n',
                        unit='mm/d/°C', aliases=['a_snow_n'],
                        min_value=0, max_value=12)
                    self.define_parameter(
                        component='type:snowpack', name='degree_day_factor_s',
                        unit='mm/d/°C', aliases=['a_snow_s'],
                        min_value=2, max_value=12)
                    self.define_parameter(
                        component='type:snowpack', name='degree_day_factor_ew',
                        unit='mm/d/°C', aliases=['a_snow_ew'],
                        min_value=2, max_value=12)
                    self.define_parameter(
                        component='type:snowpack', name='melting_temperature',
                        unit='°C', aliases=['melt_t_snow'], min_value=0,
                        max_value=5, default_value=0, mandatory=False)
                elif options['snow_melt_process'] == 'melt:temperature_index':
                    # This melt factor parameter is initialized on the snow but applied
                    # to both snow and ice (debris-covered or clean).
                    self.define_parameter(
                        component='type:snowpack,type:glacier', name='melt_factor',
                        unit='mm/d/°C', aliases=['melt_factor', 'mf'],
                        min_value=0, max_value=12)
                    self.define_parameter(
                        component='type:snowpack', name='radiation_coefficient',
                        unit='m2/W*mm/d/°C', aliases=['r_snow'],
                        min_value=0, max_value=1)
                    self.define_parameter(
                        component='type:snowpack', name='melting_temperature',
                        unit='°C', aliases=['melt_t_snow'], min_value=0,
                        max_value=5, default_value=0, mandatory=False)
                else:
                    raise RuntimeError(
                        f"The snow melt process option "
                        f"{options['snow_melt_process']} is not recognised.")

    @staticmethod
    def _check_min_max_consistency(min_value, max_value):
        if min_value is None or max_value is None:
            return

        if not isinstance(min_value, list) and not isinstance(max_value, list):
            if max_value < min_value:
                raise ValueError(f'The provided min value ({min_value} is greater than '
                                 f'the max value ({max_value}).')
            return

        if not isinstance(min_value, list) or not isinstance(max_value, list):
            raise TypeError('Mixing lists and floats for the definition of '
                            'min/max values')

        if len(min_value) != len(max_value):
            raise ValueError('The length of the min/max lists are not equal.')

        for min_v, max_v in zip(min_value, max_value):
            if max_v < min_v:
                raise ValueError(f'The provided min value ({min_v} in list is greater '
                                 f'than the max value ({max_v}).')

    def _check_aliases_uniqueness(self, aliases):
        if aliases is None:
            return

        existing_aliases = self.parameters.explode('aliases')['aliases'].tolist()
        for alias in aliases:
            if alias in existing_aliases:
                raise ValueError(f'The alias "{alias}" already exists. '
                                 f'It must be unique.')

    def _check_value_range(self, index, key, value, allow_adapt=False):
        max_value = self.parameters.loc[index, 'max']
        min_value = self.parameters.loc[index, 'min']

        if not isinstance(min_value, list):
            if max_value is not None and value > max_value:
                if allow_adapt:
                    return max_value
                raise ValueError(f'The value {value} for the parameter "{key}" is '
                                 f'above the maximum threshold ({max_value}).')
            if min_value is not None and value < min_value:
                if allow_adapt:
                    return min_value
                raise ValueError(f'The value {value} for the parameter "{key}" is '
                                 f'below the minimum threshold ({min_value}).')
        else:
            assert isinstance(max_value, list)
            assert isinstance(value, list)
            for i, (min_v, max_v, val) in enumerate(zip(min_value, max_value, value)):
                if max_v is not None and val > max_v:
                    if allow_adapt:
                        value[i] = max_v
                    else:
                        raise ValueError(f'The value {val} for the parameter "{key}" '
                                         f'is above the maximum threshold ({max_v}).')
                if min_v is not None and val < min_v:
                    if allow_adapt:
                        value[i] = min_v
                    else:
                        raise ValueError(f'The value {val} for the parameter "{key}" '
                                         f'is below the minimum threshold ({min_v}).')

        return value

    def _get_parameter_index(self, name, raise_exception=True):
        for index, row in self.parameters.iterrows():
            if row['aliases'] is not None and name in row['aliases'] \
                    or name == row['component'] + ':' + row['name']:
                return index

        if raise_exception:
            raise ValueError(f'The parameter "{name}" was not found')

        return None
