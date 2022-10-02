from hydrobricks import utils


class Model:
    """Base class for the models"""

    def __init__(self, name=None, **kwargs):
        self.name = name
        self.solver = "HeunExplicit"

        # Properties passed by via keyword arguments to avoid repetitions for all children.
        allowed_kwargs = {
            "solver",
        }
        # Validate optional keyword arguments.
        utils.validate_kwargs(kwargs, allowed_kwargs)

        # Check solver option
        if "solver" in kwargs:
            # Backwards compatibility: alias 'input_dim' to 'input_shape'.
            self.solver = kwargs["solver"]

    def get_name(self):
        return self.name
