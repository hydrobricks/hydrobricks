from hydrobricks import ParameterSet

from .model import Model


class Socont(Model):
    """Socont model implementation"""

    def __init__(self, name='socont', **kwargs):
        super().__init__(name=name, **kwargs)

        # Default options
        self.soil_storage_nb = 1
        self.surface_runoff = 'socont_runoff'

        self._add_allowed_kwargs(['soil_storage_nb', 'surface_runoff'])
        self._validate_kwargs(kwargs)
        self._set_options(kwargs)

        try:
            if not self.settings.generate_socont_structure(
                    self.land_cover_types, self.land_cover_names,
                    self.soil_storage_nb, self.surface_runoff):
                raise RuntimeError('Socont model initialization failed.')

        except RuntimeError as err:
            raise RuntimeError(f'Socont model initialization raised '
                               f'an exception: {err}')

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
                aliases = ['a_ice']
                if self.land_cover_types.count('glacier') > 1:
                    i_glacier += 1
                    aliases = [f'a_ice_{cover_name.replace("-", "_")}',
                               f'a_ice_{i_glacier}']

                ps.define_constraint('a_snow', '<', aliases[0])
                ps.define_constraint('k_snow', '<', 'k_ice')

                ps.define_parameter(
                    component=cover_name, name='degree_day_factor',
                    unit='mm/d/°C', aliases=aliases, min_value=5, max_value=20,
                    mandatory=True)

                ps.define_parameter(
                    component=cover_name, name='melting_temperature',
                    unit='°C', aliases=['melt_t_ice'], min_value=0, max_value=5,
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
