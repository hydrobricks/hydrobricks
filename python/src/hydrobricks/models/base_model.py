from _hydrobricks import ModelStructure


class Model:
    """Base class for the models"""

    def __init__(self, name=None, **kwargs):
        self.name = name
        self.solver = 'HeunExplicit'
        self.structure = ModelStructure()

        # Properties passed by via keyword arguments to avoid repetitions for children.
        # allowed_kwargs = {
        #     'solver',
        # }
        # Validate optional keyword arguments.
        # utils.validate_kwargs(kwargs, allowed_kwargs)

        # Check solver option
        if 'solver' in kwargs:
            self.solver = kwargs['solver']

    def get_name(self):
        return self.name
