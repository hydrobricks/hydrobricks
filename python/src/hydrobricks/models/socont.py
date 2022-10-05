from .base_model import Model


class Socont(Model):
    """Socont model implementation"""

    def __init__(self, name='Socont', **kwargs):
        super().__init__(name=name, **kwargs)

        self.allowed_kwargs.update(['surface_types', 'surface_names',
                                    'soil_storage_nb', 'surface_runoff'])
        self.validate_kwargs(kwargs)

        # Default options
        self.surface_types = ['ground', 'glacier']
        self.surface_names = ['ground', 'glacier']
        self.soil_storage_nb = 1
        self.surface_runoff = 'socont-runoff'

        self.set_options(kwargs)

        self.structure.generate_socont_structure(self.surface_types,
                                                 self.surface_names,
                                                 self.soil_storage_nb,
                                                 self.surface_runoff)

    def set_options(self, kwargs):
        if 'surface_types' in kwargs:
            self.surface_types = kwargs['surface_types']
        if 'surface_names' in kwargs:
            self.surface_names = kwargs['surface_names']
        if 'soil_storage_nb' in kwargs:
            self.soil_storage_nb = kwargs['soil_storage_nb']
        if 'surface_runoff' in kwargs:
            self.surface_runoff = kwargs['surface_runoff']
