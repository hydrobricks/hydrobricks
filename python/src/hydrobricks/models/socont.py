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
            if not self.structure.generate_socont_structure(
                    self.surface_types, self.surface_names,
                    self.soil_storage_nb, self.surface_runoff):
                raise Exception('Socont model initialization failed.')
        except RuntimeError as err:
            raise Exception(f'Socont model initialization raised an exception: {err}')

    def _set_options(self, kwargs):
        super()._set_options(kwargs)
        if 'soil_storage_nb' in kwargs:
            self.soil_storage_nb = kwargs['soil_storage_nb']
        if 'surface_runoff' in kwargs:
            self.surface_runoff = kwargs['surface_runoff']

    def _get_specific_options(self):
        return {
            'soil_storage_nb': self.soil_storage_nb,
            'surface_runoff': self.surface_runoff
        }
