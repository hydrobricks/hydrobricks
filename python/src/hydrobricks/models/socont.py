from hydrobricks import ParameterSet

from .base_model import Model


class Socont(Model):
    """Socont model implementation"""

    def __init__(self, name='Socont', **kwargs):
        super().__init__(name=name, **kwargs)

        # Default options
        self.soil_storage_nb = 1
        self.surface_runoff = 'socont-runoff'

        self._add_allowed_kwargs(['soil_storage_nb', 'surface_runoff'])
        self._validate_kwargs(kwargs)
        self._set_options(kwargs)

        try:
            if not self.settings.generate_socont_structure(
                    self.surface_types, self.surface_names,
                    self.soil_storage_nb, self.surface_runoff):
                raise Exception('Socont model initialization failed.')

        except RuntimeError as err:
            raise Exception(f'Socont model initialization raised an exception: {err}')

    def generate_parameters(self):
        ps = ParameterSet()

        ps.define_parameter(
            component='snow-rain-transition', name='transitionStart', unit='°C',
            min_value=-5, max_value=5, default_value=0, mandatory=False)

        ps.define_parameter(
            component='snow-rain-transition', name='transitionEnd', unit='°C',
            min_value=-5, max_value=5, default_value=2, mandatory=False)

        ps.define_parameter(
            component='snowpack', name='degreeDayFactor', unit='mm/d/°C',
            aliases=['an'], min_value=0, max_value=12, mandatory=True)

        ps.define_parameter(
            component='snowpack', name='meltingTemperature', unit='°C',
            min_value=0, max_value=5, default_value=0, mandatory=False)

        i_glacier = 0
        for surface_type, surface_name in zip(self.surface_types, self.surface_names):
            if surface_type == 'glacier':
                aliases = ['agl']
                if self.surface_types.count('glacier') > 1:
                    i_glacier += 1
                    aliases = [f'agl_{surface_name.replace("-", "_")}',
                               f'agl{i_glacier}']

                ps.define_parameter(
                    component=surface_name, name='degreeDayFactor',
                    unit='mm/d/°C', aliases=aliases, min_value=0, max_value=20,
                    mandatory=True)

                ps.define_parameter(
                    component=surface_name, name='meltingTemperature',
                    unit='°C', min_value=0, max_value=5, default_value=0,
                    mandatory=False)

        ps.define_parameter(
            component='glacier-area-rain-snowmelt-storage', name='responseFactor',
            unit='1/t', aliases=['kn'], min_value=0.001, max_value=1, mandatory=True)

        ps.define_parameter(
            component='glacier-area-icemelt-storage', name='responseFactor', unit='1/t',
            aliases=['kgl'], min_value=0.001, max_value=1, mandatory=True)

        ps.define_parameter(
            component='surface-runoff', name='responseFactor', unit='1/t',
            aliases=['kr'], min_value=0.001, max_value=1, mandatory=True)

        ps.define_parameter(
            component='slow-reservoir', name='capacity', unit='mm', aliases=['A'],
            min_value=0, max_value=3000, mandatory=True)

        if self.soil_storage_nb == 1:
            ps.define_parameter(
                component='slow-reservoir', name='responseFactor', unit='1/t',
                aliases=['ks'], min_value=0, max_value=3000, mandatory=True)

        elif self.soil_storage_nb == 2:
            ps.define_parameter(
                component='slow-reservoir', name='responseFactor', unit='1/t',
                aliases=['ks1'], min_value=0.001, max_value=1, mandatory=True)

            ps.define_parameter(
                component='slow-reservoir', name='percolationRate', unit='mm/d',
                aliases=['percol'], min_value=0, max_value=10, mandatory=True)

            ps.define_parameter(
                component='slow-reservoir-2', name='responseFactor', unit='1/t',
                aliases=['ks2'], min_value=0.001, max_value=1, mandatory=True)

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
