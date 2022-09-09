from hydrobricks import utils


class Model:
    """Base class for the models"""

    def __init__(self, name=None, **kwargs):
        self.name = name

        # Properties passed by via keyword arguments to avoid repetitions for all children.
        allowed_kwargs = {
            'solver',
        }
        # Validate optional keyword arguments.
        utils.validate_kwargs(kwargs, allowed_kwargs)

    def get_name(self):
        return self.name

