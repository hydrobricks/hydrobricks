import random

import hydrobricks as hb
import pandas as pd


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
            glacier, surface-runoff).
        name : str
            The name of the parameter in the C++ code of hydrobricks (e.g.,
            degree_day_factor, response_factor).
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
             'default_value': default_value, 'mandatory': mandatory, 'prior': None})

        self.parameters = pd.concat([self.parameters, new_row.to_frame().T],
                                    ignore_index=True)

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

    def define_constraint(self, parameter_1, operator, parameter_2):
        """
        Define a constraint between 2 parameters (e.g., paramA > paramB)

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

    def are_constraints_satisfied(self) -> bool:
        """
        Check if the constraints between parameters are satisfied.

        Returns
        -------
        True is constraints are satisfied, False otherwise.
        """
        for constraint in self.constraints:
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

            if self.are_constraints_satisfied():
                break

            if i >= 1000:
                raise RuntimeError('The parameter constraints could not be satisfied.')

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
                raise RuntimeError(f'The parameter {param} was not found.')
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
            if param['prior']:
                spotpy_params.append(
                    param['prior']
                )
            else:
                spotpy_params.append(
                    hb.spotpy.parameter.Uniform(param_name, low=param['min'],
                                                high=param['max'])
                )

        return spotpy_params

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

        hb.utils.dump_config_file(file_content, directory, name, file_type)

    @staticmethod
    def _check_min_max_consistency(min_value, max_value):
        if min_value is None or max_value is None:
            return

        if not isinstance(min_value, list) and not isinstance(max_value, list):
            if max_value < min_value:
                raise Exception(f'The provided min value ({min_value} is greater than '
                                f'the max value ({max_value}).')
            return

        if not isinstance(min_value, list) or not isinstance(max_value, list):
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
            assert isinstance(max_value, list)
            assert isinstance(value, list)
            for min_v, max_v, val in zip(min_value, max_value, value):
                if max_v is not None and value > max_v:
                    raise Exception(f'The value {val} for the parameter "{key}" is '
                                    f'above the maximum threshold ({max_v}).')
                if min_v is not None and value < min_v:
                    raise Exception(f'The value {val} for the parameter "{key}" is '
                                    f'below the minimum threshold ({min_v}).')

    def _get_parameter_index(self, name, raise_exception=True):
        for index, row in self.parameters.iterrows():
            if row['aliases'] is not None and name in row['aliases'] \
                    or name == row['component'] + ':' + row['name']:
                return index

        if raise_exception:
            raise Exception(f'The parameter "{name}" was not found')

        return None
