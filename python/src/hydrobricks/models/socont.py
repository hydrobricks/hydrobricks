from hydrobricks import ParameterSet

from .model import Model


class Socont(Model):
    """Socont model implementation"""

    def __init__(self, name='socont', **kwargs):
        super().__init__(name=name, **kwargs)

        # Default options
        self.soil_storage_nb = 1
        self.surface_runoff = 'socont_runoff'
        self.snow_melt_process = 'melt:degree_day'

        self._add_allowed_kwargs(['soil_storage_nb', 'surface_runoff'])
        self._validate_kwargs(kwargs)
        self._set_options(kwargs)

        try:
            if not self.generate_structure():
                raise RuntimeError('Socont model initialization failed.')

        except RuntimeError as err:
            raise RuntimeError(f'Socont model initialization raised '
                               f'an exception: {err}')

    def generate_structure(self):
        # Check allowed land cover types: ground, glacier
        for cover_type in self.land_cover_types:
            if cover_type not in ['ground', 'glacier']:
                raise RuntimeError(f'The land cover {cover_type} is not used in Socont')

        # Generate basic elements
        self.settings.generate_base_structure(
            self.land_cover_names, self.land_cover_types,
            snow_melt_process=self.snow_melt_process)

        # Add surface-related processes
        for cover_type, cover_name in zip(self.land_cover_types, self.land_cover_names):
            if cover_type == 'glacier':
                # Direct rain and snow melt to linear storage
                self.settings.select_hydro_unit_brick(cover_name)
                self.settings.add_brick_process(
                    'outflow_rain_snowmelt', 'outflow:direct',
                    'glacier_area_rain_snowmelt_storage')

                # Glacier melt process
                self.settings.add_brick_parameter('no_melt_when_snow_cover', True)
                self.settings.add_brick_parameter('infinite_storage', True)
                self.settings.add_brick_process(
                    'melt', 'melt:degree_day', 'glacier_area_icemelt_storage')

                # Set process outputs as instantaneous
                self.settings.set_process_outputs_as_instantaneous()

        # Basin storages for contributions from the glacierized area
        self.settings.add_sub_basin_brick(
            'glacier_area_rain_snowmelt_storage', 'storage')
        self.settings.add_brick_process(
            'outflow', 'outflow:linear', 'outlet')
        self.settings.add_sub_basin_brick(
            'glacier_area_icemelt_storage', 'storage')
        self.settings.add_brick_process(
            'outflow', 'outflow:linear', 'outlet')

        # Infiltration and overflow
        self.settings.select_hydro_unit_brick('ground')
        self.settings.add_brick_process(
            'infiltration', 'infiltration:socont', 'slow_reservoir')
        self.settings.add_brick_process(
            'runoff', 'outflow:rest_direct', 'surface_runoff')

        # Add other bricks
        if self.soil_storage_nb == 1:
            self.settings.add_hydro_unit_brick('slow_reservoir', 'storage')
            self.settings.add_brick_parameter('capacity', 200)
            self.settings.add_brick_process('et', 'et:socont')
            self.settings.add_brick_process('outflow', 'outflow:linear', 'outlet')
            self.settings.add_brick_process('overflow', 'overflow', 'outlet')
        elif self.soil_storage_nb == 2:
            print("Using 2 soil storages.")
            self.settings.add_hydro_unit_brick('slow_reservoir', 'storage')
            self.settings.add_brick_parameter('capacity', 200)
            self.settings.add_brick_process('et', 'et:socont')
            self.settings.add_brick_process('outflow', 'outflow:linear', 'outlet')
            self.settings.add_brick_process('percolation', 'outflow:constant',
                                            'slow_reservoir_2')
            self.settings.add_brick_process('overflow', 'overflow', 'outlet')
            self.settings.add_hydro_unit_brick('slow_reservoir_2', 'storage')
            self.settings.add_brick_process('outflow', 'outflow:linear', 'outlet')
        else:
            raise RuntimeError("The number of soil storages must be 1 or 2.")

        # Add surface runoff
        self.settings.add_hydro_unit_brick('surface_runoff', 'storage')
        if self.surface_runoff == 'socont_runoff':
            self.settings.add_brick_process('runoff', 'runoff:socont', 'outlet')
        elif self.surface_runoff == 'linear_storage':
            print("Using a linear storage for the quick flow.")
            self.settings.add_brick_process('outflow', 'outflow:linear', 'outlet')
        else:
            raise RuntimeError(f"The surface runoff option {self.surface_runoff} is "
                  f"not recognised in Socont.")

        self.settings.add_logging_to('outlet')

        return True

    def generate_parameters(self):
        ps = ParameterSet()

        ps.define_parameter(
            component='snow_rain_transition', name='transition_start', unit='°C',
            aliases=['prec_t_start'], min_value=-2, max_value=2, default_value=0,
            mandatory=False)

        ps.define_parameter(
            component='snow_rain_transition', name='transition_end', unit='°C',
            aliases=['prec_t_end'], min_value=0, max_value=4, default_value=2,
            mandatory=False)

        ps.define_parameter(
            component='snowpack', name='degree_day_factor', unit='mm/d/°C',
            aliases=['a_snow'], min_value=2, max_value=12, mandatory=True)

        ps.define_parameter(
            component='snowpack', name='melting_temperature', unit='°C',
            aliases=['melt_t_snow'], min_value=0, max_value=5, default_value=0,
            mandatory=False)

        i_glacier = 0
        has_glacier = False
        for cover_type, cover_name in zip(self.land_cover_types, self.land_cover_names):
            if cover_type == 'glacier':
                has_glacier = True
                a_aliases = ['a_ice']
                t_aliases = ['melt_t_ice']
                if self.land_cover_types.count('glacier') > 1:
                    i_glacier += 1
                    a_aliases = [f'a_ice_{cover_name.replace("-", "_")}',
                                 f'a_ice_{i_glacier}']
                    t_aliases = [f'melt_t_ice_{cover_name.replace("-", "_")}',
                                 f'melt_t_ice_{i_glacier}']

                ps.define_constraint('a_snow', '<', a_aliases[0])
                ps.define_constraint('k_snow', '<', 'k_ice')

                ps.define_parameter(
                    component=cover_name, name='degree_day_factor',
                    unit='mm/d/°C', aliases=a_aliases, min_value=5, max_value=20,
                    mandatory=True)

                ps.define_parameter(
                    component=cover_name, name='melting_temperature',
                    unit='°C', aliases=t_aliases, min_value=0, max_value=5,
                    default_value=0, mandatory=False)

        if has_glacier:
            ps.define_parameter(
                component='glacier_area_rain_snowmelt_storage', name='response_factor',
                unit='1/d', aliases=['k_snow'], min_value=0.05, max_value=0.25,
                mandatory=True)

            ps.define_parameter(
                component='glacier_area_icemelt_storage', name='response_factor',
                unit='1/d', aliases=['k_ice'], min_value=0.05, max_value=1,
                mandatory=True)

        if self.surface_runoff == 'socont_runoff':
            ps.define_parameter(
                component='surface_runoff', name='runoff_coefficient', unit='m^(4/3)/s',
                aliases=['beta'], min_value=100, max_value=30000, mandatory=True)
            ps.define_parameter(
                component='surface_runoff', name='slope', unit='°',
                aliases=['J'], min_value=0, max_value=90, mandatory=True)
        elif self.surface_runoff == 'linear_storage':
            ps.define_parameter(
                component='surface_runoff', name='response_factor', unit='1/d',
                aliases=['k_quick'], min_value=0.05, max_value=1, mandatory=True)

        ps.define_parameter(
            component='slow_reservoir', name='capacity', unit='mm', aliases=['A'],
            min_value=10, max_value=3000, mandatory=True)

        ps.define_parameter(
            component='slow_reservoir', name='response_factor', unit='1/d',
            aliases=['k_slow', 'k_slow_1', 'k_slow1'], min_value=0.001, max_value=1,
            mandatory=True)

        if self.soil_storage_nb == 2:
            ps.define_parameter(
                component='slow_reservoir', name='percolation_rate', unit='mm/d',
                aliases=['percol'], min_value=0, max_value=10, mandatory=True)

            ps.define_parameter(
                component='slow_reservoir_2', name='response_factor', unit='1/d',
                aliases=['k_slow_2', 'k_slow2'], min_value=0.001, max_value=1,
                mandatory=True)

            if self.surface_runoff == 'linear_storage':
                ps.define_constraint('k_slow_1', '<', 'k_quick')
                ps.define_constraint('k_slow_2', '<', 'k_quick')
            ps.define_constraint('k_slow_2', '<', 'k_slow_1')

        return ps

    def _set_options(self, kwargs):
        super()._set_options(kwargs)
        if 'soil_storage_nb' in kwargs:
            self.soil_storage_nb = int(kwargs['soil_storage_nb'])
            if self.soil_storage_nb < 1 or self.soil_storage_nb > 2:
                raise ValueError('The option "soil_storage_nb" can only be 1 or 2')
        if 'surface_runoff' in kwargs:
            self.surface_runoff = kwargs['surface_runoff']

    def _get_specific_options(self):
        return {
            'soil_storage_nb': self.soil_storage_nb,
            'surface_runoff': self.surface_runoff
        }
