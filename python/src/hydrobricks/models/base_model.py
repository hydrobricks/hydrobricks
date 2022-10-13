import os

import _hydrobricks as hb
from _hydrobricks import ModelHydro, SettingsModel
from hydrobricks import utils


class Model:
    """Base class for the models"""

    def __init__(self, name=None, **kwargs):
        self.name = name
        self.settings = SettingsModel()
        self.model = ModelHydro()
        self.spatial_structure = None
        self.allowed_kwargs = {'solver', 'record_all', 'surface_types', 'surface_names'}
        self.initialized = False

        # Default options
        self.solver = 'HeunExplicit'
        self.record_all = True
        self.surface_types = ['ground']
        self.surface_names = ['ground']

        self._set_options(kwargs)

        # Setting base settings
        self.settings.log_all(self.record_all)
        self.settings.set_solver(self.solver)

    def get_name(self):
        """Get the name of the model"""
        return self.name

    def setup(self, spatial_structure, output_path, start_date, end_date):
        """
        Setup and run the model.

        Parameters
        ----------
        spatial_structure : HydroUnits
            The spatial structure of the catchment.
        output_path: str
            Path to save the results.
        start_date: str
            Starting date of the computation
        end_date: str
            Ending date of the computation

        Return
        ------
        The predicted discharge time series
        """
        if self.initialized:
            raise RuntimeError('The model has already been initialized. '
                               'Please create a new instance.')

        try:
            if not os.path.isdir(output_path):
                os.mkdir(output_path)

            self.spatial_structure = spatial_structure

            # Initialize log
            hb.init_log(output_path)

            # Modelling period
            self.settings.set_timer(start_date, end_date, 1, "Day")

            # Initialize the model (with sub basin creation)
            if not self.model.init_with_basin(
                    self.settings, spatial_structure.settings):
                raise RuntimeError('Basin creation failed.')

            self.initialized = True

        except RuntimeError:
            print("A runtime exception occurred.")
        except TypeError:
            print("A type error exception occurred.")
        except Exception:
            print("An exception occurred.")

    def run(self, parameters, forcing=None):
        """
        Setup and run the model.

        Parameters
        ----------
        parameters : ParameterSet
            The parameters for the given model.
        forcing : Forcing
            The forcing data.

        Return
        ------
        The predicted discharge time series
        """
        if not self.initialized:
            raise RuntimeError('The model has not been initialized. '
                               'Please run setup() first.')

        try:
            self.model.reset()

            self._set_parameters(parameters)
            self._set_forcing(forcing)

            if not self.model.is_ok():
                raise RuntimeError('Model is not OK.')

            timer = utils.Timer()
            timer.start()

            if not self.model.run():
                raise RuntimeError('Model run failed.')

            timer.stop()

        except RuntimeError:
            print("A runtime exception occurred.")
        except TypeError:
            print("A type error exception occurred.")
        except Exception:
            print("An exception occurred.")

    def set_forcing(self, forcing):
        """
        Set the forcing data.

        Parameters
        ----------
        forcing : Forcing
            The forcing data.
        """
        self.model.clear_time_series()
        time = utils.date_as_mjd(forcing.time.to_numpy())
        ids = self.spatial_structure.get_ids().to_numpy()
        for data_name, data in zip(forcing.data_name, forcing.data_spatialized):
            if data is None:
                raise RuntimeError(f'The forcing {data_name} has not '
                                   f'been spatialized.')
            if not self.model.create_time_series(data_name, time, ids, data):
                raise RuntimeError('Failed adding time series.')

        if not self.model.attach_time_series_to_hydro_units():
            raise RuntimeError('Attaching time series failed.')

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
        settings = {
            'base': self.name,
            'solver': self.solver,
            'options': self._get_specific_options(),
            'surfaces': {
                'names': self.surface_names,
                'types': self.surface_types
            },
            'logger': 'all' if self.record_all else ''
        }
        utils.dump_config_file(settings, directory, name, file_type)

    def get_outlet_discharge(self):
        """
        Get the computed outlet discharge.
        """
        return self.model.get_outlet_discharge()

    def dump_outputs(self, path):
        self.model.dump_outputs(path)

    def generate_parameters(self):
        raise Exception('Parameters cannot be generated for the base model.')

    def _set_options(self, kwargs):
        if 'solver' in kwargs:
            self.solver = kwargs['solver']
        if 'record_all' in kwargs:
            self.record_all = kwargs['record_all']
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

    def _set_parameters(self, parameters):
        model_params = parameters.get_model_parameters()
        for _, param in model_params.iterrows():
            if not self.settings.set_parameter(param['component'], param['name'],
                                               param['value']):
                raise RuntimeError('Failed setting parameter values.')
        self.model.update_parameters(self.settings)

    def _set_forcing(self, forcing):
        if forcing is not None:
            self.set_forcing(forcing)
        elif not self.model.forcing_loaded():
            raise RuntimeError('Please provide the forcing data at least once.')
