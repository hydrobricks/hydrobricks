from _hydrobricks import ModelStructure
from hydrobricks import utils


class Model:
    """Base class for the models"""

    def __init__(self, name=None, **kwargs):
        self.name = name
        self.solver = 'HeunExplicit'
        self.structure = ModelStructure()
        self.allowed_kwargs = {
            'solver'
        }

        self.set_options(kwargs)

    def set_options(self, kwargs):
        if 'solver' in kwargs:
            self.solver = kwargs['solver']

    def validate_kwargs(self, kwargs):
        # Validate optional keyword arguments.
        utils.validate_kwargs(kwargs, self.allowed_kwargs)

    def get_name(self):
        return self.name
