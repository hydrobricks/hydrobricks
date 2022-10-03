from .base_model import Model


class Socont(Model):
    """Socont model implementation"""

    def __init__(self, name='Socont', **kwargs):
        super().__init__(name=name, **kwargs)

        # Default arguments
        surface_types = ['ground', 'glacier']
        surface_names = ['ground', 'glacier']
        soil_storage_nb = 1
        surface_runoff = 'socont-runoff'

        # Check options
        if 'surface_types' in kwargs:
            surface_types = kwargs['surface_types']
        if 'surface_names' in kwargs:
            surface_names = kwargs['surface_names']
        if 'soil_storage_nb' in kwargs:
            soil_storage_nb = kwargs['soil_storage_nb']
        if 'surface_runoff' in kwargs:
            surface_runoff = kwargs['surface_runoff']

        self.structure.generate_socont_structure(
            surface_types, surface_names, soil_storage_nb, surface_runoff)
