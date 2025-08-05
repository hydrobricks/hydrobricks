from __future__ import annotations

from hydrobricks.models.model import Model


class Socont(Model):
    """Socont model implementation"""

    def __init__(self, name: str = 'socont', **kwargs):
        super().__init__(name=name, **kwargs)

        # Default options
        self.options['soil_storage_nb'] = 1
        self.options['surface_runoff'] = 'socont_runoff'
        self.options['snow_melt_process'] = 'melt:degree_day'
        self.options['snow_ice_transformation'] = None
        self.options['snow_redistribution'] = None
        self.options['glacier_infinite_storage'] = True
        self.allowed_land_cover_types = ['ground', 'glacier']

        self._set_options(kwargs)

        try:
            self._define_structure()
            self._generate_structure()
            self._define_parameter_aliases()
            self._define_parameter_constraints()

        except RuntimeError as err:
            raise RuntimeError(f'Socont model initialization raised '
                               f'an exception: {err}')

    def _define_structure(self):
        # Add surface-related processes
        for cover_type, cover_name in zip(self.land_cover_types, self.land_cover_names):
            if cover_type == 'glacier':
                self.structure[cover_name] = {
                    'attach_to': 'hydro_unit',
                    'kind': 'land_cover',
                    'parameters': {
                        'no_melt_when_snow_cover': True,
                        'infinite_storage': self.options['glacier_infinite_storage']
                    },
                    'processes': {
                        'outflow_rain_snowmelt': {
                            'kind': 'outflow:direct',
                            'target': 'glacier_area_rain_snowmelt_storage',
                            'instantaneous': True
                        },
                        'melt': {
                            'kind': self.options['snow_melt_process'],
                            'target': 'glacier_area_icemelt_storage',
                            'instantaneous': True
                        }
                    }
                }

        if 'glacier' in self.land_cover_types:
            # Basin storages for contributions from the glacierized area
            self.structure['glacier_area_rain_snowmelt_storage'] = {
                'attach_to': 'sub_basin',
                'kind': 'storage',
                'processes': {
                    'outflow': {
                        'kind': 'outflow:linear',
                        'target': 'outlet'
                    }
                }
            }
            self.structure['glacier_area_icemelt_storage'] = {
                'attach_to': 'sub_basin',
                'kind': 'storage',
                'processes': {
                    'outflow': {
                        'kind': 'outflow:linear',
                        'target': 'outlet'
                    }
                }
            }

        # Infiltration and overflow
        self.structure['ground'] = {
            'attach_to': 'hydro_unit',
            'kind': 'land_cover',
            'processes': {
                'infiltration': {
                    'kind': 'infiltration:socont',
                    'target': 'slow_reservoir'
                },
                'runoff': {
                    'kind': 'outflow:rest_direct',
                    'target': 'surface_runoff'
                }
            }
        }

        # Add other bricks
        self.structure['slow_reservoir'] = {
            'attach_to': 'hydro_unit',
            'kind': 'storage',
            'parameters': {
                'capacity': 200
            },
            'processes': {
                'et': {
                    'kind': 'et:socont'
                },
                'outflow': {
                    'kind': 'outflow:linear',
                    'target': 'outlet'
                },
                'overflow': {
                    'kind': 'overflow',
                    'target': 'outlet'
                }
            }
        }

        if self.options['soil_storage_nb'] == 2:
            print("Using 2 soil storages.")
            self.structure['slow_reservoir']['processes']['percolation'] = {
                'kind': 'outflow:percolation',
                'target': 'slow_reservoir_2'
            }
            self.structure['slow_reservoir_2'] = {
                'attach_to': 'hydro_unit',
                'kind': 'storage',
                'processes': {
                    'outflow': {
                        'kind': 'outflow:linear',
                        'target': 'outlet'
                    }
                }
            }

        # Add surface runoff
        if self.options['surface_runoff'] == 'socont_runoff':
            surface_runoff_kind = 'runoff:socont'
        elif self.options['surface_runoff'] == 'linear_storage':
            print("Using a linear storage for the quick flow.")
            surface_runoff_kind = 'outflow:linear'
        else:
            raise RuntimeError(
                f"The surface runoff option {self.options['surface_runoff']} is "
                f"not recognised in Socont.")

        self.structure['surface_runoff'] = {
            'attach_to': 'hydro_unit',
            'kind': 'storage',
            'processes': {
                'runoff': {
                    'kind': surface_runoff_kind,
                    'target': 'outlet'
                }
            }
        }

    def _define_parameter_aliases(self):
        self.parameter_aliases = {
            'slow_reservoir:capacity': 'A',
            'slow_reservoir:response_factor': ['k_slow', 'k_slow_1', 'k_slow1'],
            'slow_reservoir_2:response_factor': ['k_slow_2', 'k_slow2'],
            'glacier_area_rain_snowmelt_storage:response_factor': 'k_snow',
            'glacier_area_icemelt_storage:response_factor': 'k_ice',
            'surface_runoff:response_factor': 'k_quick'
        }

    def _define_parameter_constraints(self):
        self.parameter_constraints = [
            ['a_snow', '<', 'a_ice'],
            ['k_slow_1', '<', 'k_quick'],
            ['k_slow_2', '<', 'k_quick'],
            ['k_slow_2', '<', 'k_slow_1'],
        ]

    def _set_specific_options(self, kwargs):
        if 'soil_storage_nb' in kwargs:
            self.options['soil_storage_nb'] = int(kwargs['soil_storage_nb'])
            if (self.options['soil_storage_nb'] < 1 or
                    self.options['soil_storage_nb'] > 2):
                raise ValueError('The option "soil_storage_nb" can only be 1 or 2')
