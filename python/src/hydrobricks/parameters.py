from __future__ import annotations
import random
from dataclasses import dataclass
from typing import Hashable

import numpy as np
import pandas as pd

from hydrobricks import spotpy
from hydrobricks._optional import HAS_SPOTPY
from hydrobricks.utils import dump_config_file


@dataclass(frozen=True)
class ParamSpec:
    """Static specification for a parameter."""
    name: str
    unit: str | None = None
    aliases: list[str] | None = None
    min_value: float | list[float] | None = None
    max_value: float | list[float] | None = None
    default_value: float | list[float] | None = None
    mandatory: bool = True

    def to_kwargs(self) -> dict:
        # Return a dict suitable to unpack into define_parameter (excluding component)
        return {
            'name': self.name,
            'unit': self.unit,
            'aliases': None if self.aliases is None else list(self.aliases),
            'min_value': self.min_value,
            'max_value': self.max_value,
            'default_value': self.default_value,
            'mandatory': self.mandatory,
        }


# -----------------------------------------------------------------------------
# Unified registry for process-like parameter specs.
# Bricks (e.g., reservoirs) keep a separate registry.
# -----------------------------------------------------------------------------

PROCESS_PARAM_SPECS: dict[str, list[ParamSpec]] = {
    'outflow:linear': [
        ParamSpec(
            name='response_factor', unit='1/d', aliases=[],
            min_value=0.001, max_value=1, mandatory=True
        )
    ],
    'runoff:socont': [
        ParamSpec(
            name='beta', unit='m^(4/3)/s', aliases=['beta'],
            min_value=100, max_value=30000, mandatory=True
        )
    ],
    'outflow:percolation': [
        ParamSpec(
            name='percolation_rate', unit='mm/d', aliases=['percol'],
            min_value=0, max_value=10, mandatory=True
        )
    ],
    # Snow/rain transition (pseudo-process)
    'transition:snow_rain': [
        ParamSpec(
            name='transition_start', unit='°C', aliases=['prec_t_start'],
            min_value=-2, max_value=2, default_value=0, mandatory=False
        ),
        ParamSpec(
            name='transition_end', unit='°C', aliases=['prec_t_end'],
            min_value=0, max_value=4, default_value=2, mandatory=False
        ),
    ],
    # Melt processes (snow + glacier unified specs)
    'melt:degree_day': [
        ParamSpec(
            name='degree_day_factor', unit='mm/d/°C', aliases=None,
            min_value=2, max_value=20, mandatory=True  # (snow 2-12, glacier 5-20)
        ),
        ParamSpec(
            name='melting_temperature', unit='°C', aliases=None,
            min_value=0, max_value=5, default_value=0, mandatory=False
        ),
    ],
    'melt:degree_day_aspect': [
        ParamSpec(
            name='degree_day_factor_n', unit='mm/d/°C', aliases=None,
            min_value=0, max_value=20, mandatory=True  # (snow 0-12, glacier 1-20)
        ),
        ParamSpec(
            name='degree_day_factor_s', unit='mm/d/°C', aliases=None,
            min_value=2, max_value=20, mandatory=True  # (snow 2-12, glacier 5-20)
        ),
        ParamSpec(
            name='degree_day_factor_ew', unit='mm/d/°C', aliases=None,
            min_value=2, max_value=20, mandatory=True  # (snow 2-12, glacier 5-20)
        ),
        ParamSpec(
            name='melting_temperature', unit='°C', aliases=None,
            min_value=0, max_value=5, default_value=0, mandatory=False
        ),
    ],
    'melt:temperature_index': [
        ParamSpec(
            name='melt_factor', unit='mm/d/°C', aliases=None,
            min_value=0, max_value=12, mandatory=True
        ),
        ParamSpec(
            name='radiation_coefficient', unit='m2/W*mm/d/°C', aliases=None,
            min_value=0, max_value=1, mandatory=True
        ),
        ParamSpec(
            name='melting_temperature', unit='°C', aliases=None,
            min_value=0, max_value=5, default_value=0, mandatory=False
        ),
    ],
    # Snow/ice transformation processes (dynamic aliases per glacier snowpack)
    'transform:snow_ice_constant': [
        ParamSpec(
            name='snow_ice_transformation_rate', unit='mm/d', aliases=None,
            min_value=0, max_value=10, default_value=0.5, mandatory=True
        ),
    ],
    'transform:snow_ice_swat': [
        ParamSpec(
            name='snow_ice_transformation_basal_acc_coeff', unit='-', aliases=None,
            min_value=0.001, max_value=0.006, default_value=0.0014, mandatory=False
        ),
        ParamSpec(
            name='north_hemisphere', unit='-', aliases=None,
            min_value=0, max_value=1, default_value=1, mandatory=False
        ),
    ],
    # Snow redistribution processes
    'transport:snow_slide': [
        ParamSpec(
            name='coeff', unit='-', aliases=['snow_slide_coeff'],
            min_value=0, max_value=10000, default_value=3178.4, mandatory=False
        ),
        ParamSpec(
            name='exp', unit='-', aliases=['snow_slide_exp'],
            min_value=-5, max_value=0, default_value=-1.998, mandatory=False
        ),
        ParamSpec(
            name='min_slope', unit='°', aliases=['snow_slide_min_slope'],
            min_value=0, max_value=45, default_value=10, mandatory=False
        ),
        ParamSpec(
            name='max_slope', unit='°', aliases=['snow_slide_max_slope'],
            min_value=45, max_value=90, default_value=75, mandatory=False
        ),
        ParamSpec(
            name='min_snow_holding_depth', unit='mm',
            aliases=['snow_slide_min_snow_depth'],
            min_value=0, max_value=1000, default_value=50, mandatory=False
        ),
        ParamSpec(
            name='max_snow_depth', unit='mm', aliases=['snow_slide_max_snow_depth'],
            min_value=-1, max_value=50000, default_value=20000, mandatory=False
        ),
    ],
}

BRICK_PARAM_SPECS: dict[str, ParamSpec] = {
    'capacity': ParamSpec(
        name='capacity', unit='mm', aliases=[], min_value=0, max_value=3000,
        mandatory=True
    ),
}


# -----------------------------------------------------------------------------
# Validation & helper utilities for parameter specs
# -----------------------------------------------------------------------------
def validate_process_param_specs(specs: dict[str, list[ParamSpec]] | None = None):
    """Validate static process parameter specs.

    Checks:
    - No duplicate (process, parameter name) pairs.
    Alias duplication across different processes is allowed because processes
    (e.g., alternative melt formulations) are mutually exclusive at runtime
    and runtime registration still enforces global alias uniqueness.
    """
    if specs is None:
        specs = PROCESS_PARAM_SPECS

    seen_pairs: set[tuple[str, str]] = set()
    for proc, spec_list in specs.items():
        for spec in spec_list:
            pair = (proc, spec.name)
            if pair in seen_pairs:
                raise ValueError(
                    f'Duplicate parameter name "{spec.name}" in process "{proc}".')
            seen_pairs.add(pair)


def get_process_param_specs() -> dict[str, list[dict]]:
    """Return a JSON-serializable snapshot of the process parameter specs."""
    catalog: dict[str, list[dict]] = {}
    for proc, spec_list in PROCESS_PARAM_SPECS.items():
        catalog[proc] = [spec.to_kwargs() for spec in spec_list]
    return catalog


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
    def allow_changing(self, allow_changing: list[str]):
        """
        Set the list of parameters to assess.

        Parameters
        ----------
        allow_changing
            A list of parameters to assess. Only the parameters in this list will be
            changed. If a parameter is related to data forcing, the spatialization
            will be performed again.
        """
        self._allow_changing = allow_changing

    def define_parameter(
            self,
            component: str,
            name: str,
            unit: str | None = None,
            aliases: str | list[str] | None = None,
            min_value: float | list[float] | None = None,
            max_value: float | list[float] | None = None,
            default_value: float | list[float] | None = None,
            mandatory: bool = True
    ):
        """
        Define a parameter by setting its properties.

        Parameters
        ----------
        component
            The component (brick) name to which the parameter refer (e.g., snowpack,
            glacier, surface_runoff). It can be a string of a list of components when
            the parameter is shared between components (e.g., melt_factor in the
            temperature index method).
        name
            The name of the parameter in the C++ code of hydrobricks (e.g.,
            degree_day_factor, response_factor).
        unit
            The unit of the parameter.
        aliases
            Aliases to the parameter name, such as names used in other implementations
            (e.g., kgl, an). Aliases must be unique.
        min_value
            Minimum value allowed for the parameter.
        max_value
            Maximum value allowed for the parameter.
        default_value
            The parameter default value.
        mandatory
            If the parameter needs to be defined or if it can silently use the
            default value.
        """
        value = None
        if not mandatory and default_value is not None:
            value = default_value

        self._check_aliases_uniqueness(aliases)
        self._check_min_max_consistency(min_value, max_value)

        new_row = pd.Series({
            'component': component,
            'name': name,
            'unit': unit,
            'aliases': aliases,
            'value': value,
            'min': min_value,
            'max': max_value,
            'default_value': default_value,
            'mandatory': mandatory,
            'prior': None
        })

        self.parameters = pd.concat([self.parameters, new_row.to_frame().T],
                                    ignore_index=True)

    def add_aliases(self, parameter_name: str, aliases: list[str] | str):
        """
        Add aliases to a parameter.

        Parameters
        ----------
        parameter_name
            The name of the parameter with the related component (e.g.,
            snowpack:degree_day_factor).
        aliases
            Aliases to the parameter name, such as names used in other implementations
            (e.g., kgl, an). Aliases must be unique.
        """
        if not isinstance(aliases, list):
            aliases = [aliases]
        index = self._get_parameter_index(parameter_name)
        self.parameters.loc[index, 'aliases'] += aliases

    def change_range(self, parameter: str, min_value: float, max_value: float):
        """
        Change the value range of a parameter.

        Parameters
        ----------
        parameter
            Name (or alias) of the parameter
        min_value
            New minimum value
        max_value
            New maximum value
        """
        index = self._get_parameter_index(parameter)
        self.parameters.loc[index, 'min'] = min_value
        self.parameters.loc[index, 'max'] = max_value

    def set_prior(self, parameter: str, prior: spotpy.parameter):
        """
        Change the value range of a parameter.

        Parameters
        ----------
        parameter
            Name (or alias) of the parameter
        prior
            The prior distribution (instance of spotpy.parameter)
        """
        if not HAS_SPOTPY:
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

    def define_constraint(self, parameter_1: str, operator: str, parameter_2: str):
        """
        Defines a constraint between 2 parameters (e.g., paramA > paramB)

        Parameters
        ----------
        parameter_1
            The name of the first parameter.
        operator
            The operator (e.g. '<=').
        parameter_2
            The name of the second parameter.

        Examples
        --------
        parameter_set.define_constraint('paramA', '>=', 'paramB')
        """
        constraint = [parameter_1, operator, parameter_2]
        self.constraints.append(constraint)

    def remove_constraint(self, parameter_1: str, operator: str, parameter_2: str):
        """
        Removes a constraint between 2 parameters (e.g., paramA > paramB)

        Parameters
        ----------
        parameter_1
            The name of the first parameter.
        operator
            The operator (e.g. '<=').
        parameter_2
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

    def set_values(
            self,
            values: dict,
            check_range: bool = True,
            allow_adapt: bool = False
    ):
        """
        Set the parameter values.

        Parameters
        ----------
        values
            The values must be provided as a dictionary with the parameter name with the
            related component or one of its aliases as the key.
            Example: {'k': 32, 'A': 300} or {'slow_reservoir:capacity': 300}
        check_range
            Check that the parameter value falls into the allowed range.
        allow_adapt
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

    def has(self, name: str) -> bool:
        """
        Check if a parameter exists.

        Parameters
        ----------
        name
            The name of the parameter.

        Returns
        ------
        True if found, False otherwise.
        """
        index = self._get_parameter_index(name, raise_exception=False)
        return index is not None

    def get(self, name: str) -> float:
        """
        Get the value of a parameter by name.

        Parameters
        ----------
        name
            The name of the parameter.

        Returns
        ------
        The parameter value.
        """
        index = self._get_parameter_index(name)
        return self.parameters.loc[index, 'value']

    def is_ok(self) -> bool:
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

    def get_undefined(self) -> list[str]:
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

    def get_model_parameters(self) -> pd.DataFrame:
        """
        Get the model-only parameters (excluding data-related parameters).
        """
        return self.parameters[self.parameters['component'] != 'data']

    def add_data_parameter(
            self,
            name: str,
            value: float | list[float] | None = None,
            min_value: float | list[float] | None = None,
            max_value: float | list[float] | None = None,
            unit: str | None = None
    ):
        """
        Add a parameter related to the data.

        Parameters
        ----------
        name
            The name of the parameter.
        value
            The parameter value.
        min_value
            Minimum value allowed for the parameter.
        max_value
            Maximum value allowed for the parameter.
        unit
            The unit of the parameter.
        """
        aliases = [name]

        self._check_aliases_uniqueness(aliases)
        self._check_min_max_consistency(min_value, max_value)

        new_row = pd.Series({
            'component': 'data',
            'name': name,
            'unit': unit,
            'aliases': aliases,
            'value': value,
            'min': min_value,
            'max': max_value,
            'default_value': value,
            'mandatory': False
        })

        self.parameters = pd.concat([self.parameters, new_row.to_frame().T],
                                    ignore_index=True)

    def is_for_forcing(self, parameter_name: str) -> bool:
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

    def set_random_values(self, parameters: list[str]) -> pd.DataFrame:
        """
        Set the provided parameter to random values.

        Parameters
        ----------
        parameters
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

    def needs_random_forcing(self) -> bool:
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

    def get_for_spotpy(self) -> list[spotpy.parameter]:
        """
        Get the parameters to assess ready to be used in spotpy.

        Returns
        -------
        A list of the parameters as spotpy objects.
        """
        if not HAS_SPOTPY:
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
                    spotpy.parameter.Uniform(
                        param_name,
                        low=param['min'],
                        high=param['max']
                    )
                )

        return spotpy_params

    def save_as(self, directory: str, name: str, file_type: str = 'both'):
        """
        Create a configuration file containing the parameter values.

        Such a file can be used when using the command-line version of hydrobricks. It
        contains the model parameter values.

        Parameters
        ----------
        directory
            The directory to write the file.
        name
            The name of the generated file.
        file_type
            The type of file to generate: 'json', 'yaml', or 'both'.
        """
        grouped_params = self.parameters.groupby('component', sort=False)
        file_content = {}

        for group_name, group in grouped_params:
            group_content = {}
            for _, row in group.iterrows():
                group_content.update({row['name']: row['value']})
            file_content.update({group_name: group_content})

        dump_config_file(file_content, directory, name, file_type)

    def generate_parameters(
            self,
            land_cover_types: list[str],
            land_cover_names: list[str],
            options: dict,
            structure: dict
    ):
        """
        Generate a parameters object for the provided model options and structure.

        Parameters
        ----------
        land_cover_types
            The land cover types.
        land_cover_names
            The land cover names.
        options
            The model options.
        structure
            The model structure.
        """
        # General parameters
        self._generate_snow_parameters(
            options,
            land_cover_types,
            land_cover_names
        )

        # Parameters for the glaciers
        self._generate_glacier_parameters(
            land_cover_types,
            land_cover_names,
            structure
        )

        # Parameters for the different bricks
        for key, brick in structure.items():
            self._generate_brick_parameters(key, brick)
            self._generate_process_parameters(key, brick)

    def _register(self, component: str, spec: ParamSpec, **overrides):
        """Register a parameter based on a ParamSpec.

        Parameters
        ----------
        component: str
            Component name used in define_parameter.
        spec: ParamSpec
            The static specification.
        overrides: dict
            Any field accepted by define_parameter to override spec values.
        """
        kwargs = spec.to_kwargs()
        # Apply overrides if supplied
        for key, val in overrides.items():
            if key in kwargs:
                kwargs[key] = val
        self.define_parameter(component=component, **kwargs)

    def _generate_process_parameters(self, key: str, brick: dict):
        if 'processes' not in brick:
            return

        skip = {
            # No parameters
            'infiltration:socont',
            'outflow:rest_direct',
            'outflow:direct',
            'et:socont',
            'overflow',
            # Defined elsewhere (glacier/snow generation logic)
            'melt:degree_day',
            'melt:degree_day_aspect',
            'melt:temperature_index'
        }

        for _, process in brick['processes'].items():
            kind = process['kind']
            if kind in skip:
                continue
            if kind in PROCESS_PARAM_SPECS:
                for spec in PROCESS_PARAM_SPECS[kind]:
                    self._register(component=key, spec=spec)
            else:
                raise RuntimeError(
                    f"The process {kind} is not recognised in parameters generation."
                )

    def _generate_brick_parameters(self, key: str, brick: dict):
        if 'parameters' not in brick:
            return

        skip = {'no_melt_when_snow_cover', 'infinite_storage'}

        for param_name, _ in brick['parameters'].items():
            if param_name in skip:
                continue
            if param_name in BRICK_PARAM_SPECS:
                self._register(component=key, spec=BRICK_PARAM_SPECS[param_name])
            else:
                raise RuntimeError(
                    f"Parameter {param_name} is not recognised in params generation."
                )

    def _generate_glacier_parameters(
            self,
            land_cover_types: list[str],
            land_cover_names: list[str],
            structure: dict
    ):
        if 'glacier' not in land_cover_types:
            return

        glacier_names = [
            cover_name for cover_type, cover_name in
            zip(land_cover_types, land_cover_names)
            if cover_type == 'glacier'
        ]

        for i, cover_name in enumerate(glacier_names):
            melt_method = structure[cover_name]['processes']['melt']['kind']

            if len(glacier_names) == 1:
                t_aliases = ['melt_t_ice']
            else:
                t_aliases = [
                    f'melt_t_ice_{cover_name.replace("-", "_")}',
                    f'melt_t_ice_{i}'
                ]

            if melt_method not in PROCESS_PARAM_SPECS:
                raise RuntimeError(f"The glacier melt method {melt_method} is not "
                                   f"recognised in parameters generation.")

            # Build dynamic alias mapping per parameter name
            alias_map: dict[str, list[str]] = {}
            if melt_method == 'melt:degree_day':
                if len(glacier_names) == 1:
                    a_aliases = ['a_ice']
                else:
                    a_aliases = [
                        f'a_ice_{cover_name.replace("-", "_")}',
                        f'a_ice_{i}'
                    ]
                alias_map = {
                    'degree_day_factor': a_aliases,
                    'melting_temperature': t_aliases,
                }
            elif melt_method == 'melt:degree_day_aspect':
                if len(glacier_names) == 1:
                    a_n_aliases = ['a_ice_n']
                    a_s_aliases = ['a_ice_s']
                    a_ew_aliases = ['a_ice_ew']
                else:
                    a_n_aliases = [
                        f'a_ice_n_{cover_name.replace("-", "_")}',
                        f'a_ice_n_{i}'
                    ]
                    a_s_aliases = [
                        f'a_ice_s_{cover_name.replace("-", "_")}',
                        f'a_ice_s_{i}'
                    ]
                    a_ew_aliases = [
                        f'a_ice_ew_{cover_name.replace("-", "_")}',
                        f'a_ice_ew_{i}'
                    ]
                alias_map = {
                    'degree_day_factor_n': a_n_aliases,
                    'degree_day_factor_s': a_s_aliases,
                    'degree_day_factor_ew': a_ew_aliases,
                    'melting_temperature': t_aliases,
                }
            elif melt_method == 'melt:temperature_index':
                if len(glacier_names) == 1:
                    r_aliases = ['r_ice']
                else:
                    r_aliases = [
                        f'r_ice_{cover_name.replace("-", "_")}',
                        f'r_ice_{i}'
                    ]
                alias_map = {
                    'radiation_coefficient': r_aliases,
                    'melting_temperature': t_aliases,
                }

            # Register parameters from specs with aliases
            for spec in PROCESS_PARAM_SPECS[melt_method]:
                if spec.name == 'melt_factor':  # already registered for snow & glacier
                    continue
                self._register(component=cover_name, spec=spec,
                               aliases=alias_map.get(spec.name, []))

            with_glacier_debris = (len(glacier_names) > 1 and
                                   cover_name == 'glacier_debris')
            # Constraints
            if melt_method == 'melt:degree_day':
                if with_glacier_debris:
                    self.define_constraint(
                        'a_ice_glacier_debris', '<', 'a_ice_glacier_ice')
                self.define_constraint(
                    'a_snow', '<', alias_map['degree_day_factor'][0])
            elif melt_method == 'melt:degree_day_aspect':
                if with_glacier_debris:
                    self.define_constraint(
                        'a_ice_n_glacier_debris', '<', 'a_ice_n_glacier_ice')
                    self.define_constraint(
                        'a_ice_s_glacier_debris', '<', 'a_ice_s_glacier_ice')
                    self.define_constraint(
                        'a_ice_ew_glacier_debris', '<', 'a_ice_ew_glacier_ice')
                self.define_constraint(
                    'a_snow', '<', alias_map['degree_day_factor_n'][0])
                self.define_constraint(
                    'a_snow', '<', alias_map['degree_day_factor_s'][0])
                self.define_constraint(
                    'a_snow', '<', alias_map['degree_day_factor_ew'][0])
                self.define_constraint(
                    'a_snow_n', '<', alias_map['degree_day_factor_n'][0])
                self.define_constraint(
                    'a_snow_s', '<', alias_map['degree_day_factor_s'][0])
                self.define_constraint(
                    'a_snow_ew', '<', alias_map['degree_day_factor_ew'][0])
            elif melt_method == 'melt:temperature_index':
                if with_glacier_debris:
                    self.define_constraint(
                        'r_ice_glacier_debris', '<', 'r_ice_glacier_ice')
                self.define_constraint(
                    'r_snow', '<', alias_map['radiation_coefficient'][0])

    def _generate_snow_parameters(
            self,
            options: dict,
            land_cover_types: list[str],
            land_cover_names: list[str]
    ):
        if 'snow_melt_process' in options or 'with_snow' in options:
            # Snow/rain transition specs (pseudo-process)
            for spec in PROCESS_PARAM_SPECS['transition:snow_rain']:
                self._register(component='snow_rain_transition', spec=spec)

            if 'snow_melt_process' in options:
                smp = options['snow_melt_process']
                if smp is None:
                    return
                if smp in PROCESS_PARAM_SPECS:
                    snow_alias_map: dict[str, list[str]] = {}
                    if smp == 'melt:degree_day':
                        snow_alias_map = {
                            'degree_day_factor': ['a_snow'],
                            'melting_temperature': ['melt_t_snow'],
                        }
                    elif smp == 'melt:degree_day_aspect':
                        snow_alias_map = {
                            'degree_day_factor_n': ['a_snow_n'],
                            'degree_day_factor_s': ['a_snow_s'],
                            'degree_day_factor_ew': ['a_snow_ew'],
                            'melting_temperature': ['melt_t_snow'],
                        }
                    elif smp == 'melt:temperature_index':
                        snow_alias_map = {
                            'melt_factor': ['melt_factor', 'mf'],
                            'radiation_coefficient': ['r_snow'],
                            'melting_temperature': ['melt_t_snow'],
                        }
                    for spec in PROCESS_PARAM_SPECS[smp]:
                        component = 'type:snowpack'
                        if spec.name == 'melt_factor':
                            # Shared between snowpack & glacier
                            component = 'type:snowpack,type:glacier'
                        self._register(component=component, spec=spec,
                                       aliases=snow_alias_map.get(spec.name, []))
                else:
                    raise RuntimeError(
                        f"The snow melt process option {smp} is not recognised.")

            # Snow/ice transformation
            if 'snow_ice_transformation' in options:
                algo = options['snow_ice_transformation']
                if algo is None:
                    return
                if algo not in PROCESS_PARAM_SPECS:
                    raise RuntimeError(
                        f"The snow/ice transformation option {algo} is not recognised.")
                glacier_names = [
                    cover_name for cover_type, cover_name in
                    zip(land_cover_types, land_cover_names)
                    if cover_type == 'glacier'
                ]
                for i, cover_name in enumerate(glacier_names):
                    # Dynamic aliases per glacier snowpack
                    alias_map: dict[str, list[str]] = {}
                    multi = len(glacier_names) > 1
                    if algo == 'transform:snow_ice_constant':
                        if multi:
                            a_rate = [
                                f'snow_ice_rate_{cover_name.replace("-", "_")}',
                                f'snow_ice_rate_{i}'
                            ]
                        else:
                            a_rate = ['snow_ice_rate']
                        alias_map = {
                            'snow_ice_transformation_rate': a_rate,
                        }
                    elif algo == 'transform:snow_ice_swat':
                        if multi:
                            a_coeff = [
                                f'snow_ice_basal_acc_coeff_{cover_name.replace("-", "_")}',
                                f'snow_ice_basal_acc_coeff_{i}'
                            ]
                        else:
                            a_coeff = ['snow_ice_basal_acc_coeff']
                        alias_map = {
                            'snow_ice_transformation_basal_acc_coeff': a_coeff,
                            'north_hemisphere': ['north_hemisphere'],
                        }
                    for spec in PROCESS_PARAM_SPECS[algo]:
                        self._register(component=f'{cover_name}_snowpack', spec=spec,
                                       aliases=alias_map.get(spec.name, []))

            # Snow redistribution
            if 'snow_redistribution' in options:
                red = options['snow_redistribution']
                if red is None:
                    return
                if red in PROCESS_PARAM_SPECS:
                    for spec in PROCESS_PARAM_SPECS[red]:
                        self._register(component='type:snowpack', spec=spec,
                                       aliases=spec.aliases or [])
                elif red is not None:
                    raise RuntimeError(
                        f"The snow redistribution option {red} is not recognised.")

    @staticmethod
    def _check_min_max_consistency(min_value: float, max_value: float):
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

    def _check_aliases_uniqueness(self, aliases: list[str] | None):
        if aliases is None:
            return

        existing_aliases = self.parameters.explode('aliases')['aliases'].tolist()
        for alias in aliases:
            if alias in existing_aliases:
                raise ValueError(f'The alias "{alias}" already exists. '
                                 f'It must be unique.')

    def _check_value_range(
            self,
            index: int,
            key: str,
            value: float,
            allow_adapt: bool = False
    ) -> float:
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

    def _get_parameter_index(
            self,
            name: str,
            raise_exception: bool = True
    ) -> int | Hashable | None:
        for index, row in self.parameters.iterrows():
            if row['aliases'] is not None and name in row['aliases'] \
                    or name == row['component'] + ':' + row['name']:
                return index

        if raise_exception:
            raise ValueError(f'The parameter "{name}" was not found')

        return None
