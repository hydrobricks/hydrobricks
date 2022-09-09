from .base_model import Model


class Socont(Model):

    def __init__(self,
                 name=None,
                 **kwargs):
        super(Socont, self).__init__(
            name=name,
            **kwargs)

