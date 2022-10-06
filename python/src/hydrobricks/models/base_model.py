from _hydrobricks import ModelStructure
from hydrobricks import utils


class Model:
    """Base class for the models"""

    def __init__(self, name=None, **kwargs):
        self.name = name
        self.structure = ModelStructure()
        self.allowed_kwargs = {'solver', 'logger', 'surface_types', 'surface_names'}

        # Default options
        self.solver = 'HeunExplicit'
        self.logger = 'all'
        self.surface_types = ['ground']
        self.surface_names = ['ground']

        self._set_options(kwargs)

    def get_name(self):
        """Get the name of the model"""
        return self.name

    def create_config_file(self, directory, name, file_type='both'):
        """
        Create a configuration file describing the model structure.

        Such a file can be used when using the command-line version of hydrobricks. It
        contains the options to generate the corresponding model structure.

        Parameters
        ----------
        directory : str
            The directory to write the file.
        name : str
            The name of the generated file.
        file_type : file_type
            The type of file to generate: 'json', 'yaml', or 'both'.
        """
        structure = {
            'base': self.name,
            'solver': self.solver,
            'options': self._get_specific_options(),
            'surfaces': {
                'names': self.surface_names,
                'types': self.surface_types
            },
            'logger': self.logger
        }
        utils.dump_config_file(structure, directory, name, file_type)

    def generate_parameters(self):
        raise Exception('Parameters cannot be generated for the base model.')

    def _set_options(self, kwargs):
        if 'solver' in kwargs:
            self.solver = kwargs['solver']
        if 'logger' in kwargs:
            self.logger = kwargs['logger']
        if 'surface_types' in kwargs:
            self.surface_types = kwargs['surface_types']
        if 'surface_names' in kwargs:
            self.surface_names = kwargs['surface_names']

    def _add_allowed_kwargs(self, kwargs):
        self.allowed_kwargs.update(kwargs)

    def _validate_kwargs(self, kwargs):
        # Validate optional keyword arguments.
        utils.validate_kwargs(kwargs, self.allowed_kwargs)

    def _get_specific_options(self):
        return {}
