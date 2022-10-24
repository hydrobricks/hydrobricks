import random

import pandas as pd
from hydrobricks import utils


class ParameterSet:
    """Class for the parameter sets"""

    def __init__(self):
        self.parameters = pd.DataFrame(
            columns=['component', 'name', 'unit', 'aliases', 'value',
                     'min', 'max', 'default_value', 'mandatory'])

    def define_parameter(self, component, name, unit=None, aliases=None, min_value=None,
                         max_value=None, default_value=None, mandatory=True):
        """
        Define a parameter by setting its properties.

        Parameters
        ----------
        component : str
            The component (brick) name to which the parameter refer (e.g., snowpack,
            glacier, surface-runoff).
        name : str
            The name of the parameter in the C++ code of hydrobricks (e.g.,
            degreeDayFactor, responseFactor).
        unit : str
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
             'default_value': default_value, 'mandatory': mandatory})

        self.parameters = pd.concat([self.parameters, new_row.to_frame().T],
                                    ignore_index=True)

    def set_values(self, values):
        """
        Set the parameter values.

        Parameters
        ----------
        values : dict
            The values must be provided as a dictionary with the parameter name with the
            related component or one of its aliases as the key.
            Example: {'k': 32, 'A': 300} or {'slow-reservoir:capacity': 300}
        """
        for key in values:
            index = self._get_parameter_index(key)
            self._check_value_range(index, key, values[key])
            self.parameters.loc[index, 'value'] = values[key]

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

        for key in parameters:
            index = self._get_parameter_index(key)
            min_value = self.parameters.loc[index, 'min']
            max_value = self.parameters.loc[index, 'max']

            if type(min_value) == list:
                new_values = []
                for min_val, max_val in zip(min_value, max_value):
                    new_values.append(random.uniform(min_val, max_val))
                self.parameters.loc[index, 'value'] = new_values
            else:
                self.parameters.loc[index, 'value'] = random.uniform(
                    min_value, max_value)

            assigned_values.loc[0, key] = self.parameters.loc[index, 'value']

        return assigned_values

    def create_file(self, directory, name, file_type='both'):
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

        utils.dump_config_file(file_content, directory, name, file_type)

    @staticmethod
    def _check_min_max_consistency(min_value, max_value):
        if min_value is None or max_value is None:
            return

        if type(min_value) != list and type(max_value) != list:
            if max_value < min_value:
                raise Exception(f'The provided min value ({min_value} is greater than '
                                f'the max value ({max_value}).')
            return

        if type(min_value) != list or type(max_value) != list:
            raise Exception('Mixing lists and floats for the definition of '
                            'min/max values')

        if len(min_value) != len(max_value):
            raise Exception('The length of the min/max lists are not equal.')

        for min_v, max_v in zip(min_value, max_value):
            if max_v < min_v:
                raise Exception(f'The provided min value ({min_v} in list is greater '
                                f'than the max value ({max_v}).')

    def _check_aliases_uniqueness(self, aliases):
        if aliases is None:
            return

        existing_aliases = self.parameters.explode('aliases')['aliases'].tolist()
        for alias in aliases:
            if alias in existing_aliases:
                raise Exception(f'The alias "{alias}" already exists. '
                                f'It must be unique.')

    def _check_value_range(self, index, key, value):
        max_value = self.parameters.loc[index, 'max']
        min_value = self.parameters.loc[index, 'min']

        if type(min_value) != list:
            if max_value is not None and value > max_value:
                raise Exception(f'The value {value} for the parameter "{key}" is above '
                                f'the maximum threshold ({max_value}).')
            if min_value is not None and value < min_value:
                raise Exception(f'The value {value} for the parameter "{key}" is below '
                                f'the minimum threshold ({min_value}).')
        else:
            assert type(max_value) == list
            assert type(value) == list
            for min_v, max_v, val in zip(min_value, max_value, value):
                if max_v is not None and value > max_v:
                    raise Exception(f'The value {val} for the parameter "{key}" is '
                                    f'above the maximum threshold ({max_v}).')
                if min_v is not None and value < min_v:
                    raise Exception(f'The value {val} for the parameter "{key}" is '
                                    f'below the minimum threshold ({min_v}).')

    def _get_parameter_index(self, name):
        for index, row in self.parameters.iterrows():
            if row['aliases'] is not None and name in row['aliases'] \
                    or name == row['component'] + ':' + row['name']:
                return index

        raise Exception(f'The parameter "{name}" was not found')
