
class Model:
    """Base class for the models"""

    def __init__(self,
                 name=None,
                 **kwargs):
        self.name = name

    def get_name(self):
        return self.name

