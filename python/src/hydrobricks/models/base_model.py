import os

import _hydrobricks as hb
from _hydrobricks import ModelHydro, SettingsModel, TimeSeries
from hydrobricks import utils


class Model:
    """Base class for the models"""

    def __init__(self, name=None, **kwargs):
        self.name = name
        self.settings = SettingsModel()
        self.model = ModelHydro()
        self.allowed_kwargs = {'solver', 'log_all', 'surface_types', 'surface_names'}

        # Default options
        self.solver = 'HeunExplicit'
        self.log_all = False
        self.surface_types = ['ground']
        self.surface_names = ['ground']

        self._set_options(kwargs)

    def get_name(self):
        """Get the name of the model"""
        return self.name

    def do_log_all(self):
        """Enable logging of all components."""
        self.log_all = True
        self.settings.log_all()

    def disable_log_all(self):
        """Disable logging of all components."""
        self.log_all = False
        self.settings.log_all(False)

    def setup_and_run(self, spatial_structure, parameters, forcing, output_path,
                      start_date, end_date):
        """
        Setup and run the model.

        Parameters
        ----------
        spatial_structure : HydroUnits
            The spatial structure of the catchment.
        parameters : ParameterSet
            The parameters for the given model.
        forcing : Forcing
            The forcing data.
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
        try:
            if not os.path.isdir(output_path):
                os.mkdir(output_path)

            # Initialize log
            hb.init_log(output_path)

            # Modelling period
            self.settings.set_timer(start_date, end_date, 1, "Day")

            # Parameters
            model_params = parameters.get_model_parameters()
            for _, param in model_params.iterrows():
                self.settings.set_parameter(param['component'], param['name'],
                                            param['value'])

            # Timer
            t = utils.Timer()
            t.start()

            # Initialize the model (with sub basin creation)
            if not self.model.init_with_basin(self.settings,
                                              spatial_structure.settings):
                raise RuntimeError('Basin creation failed.')

            # Add data
            for data_name, dates, data in zip(forcing.data_name, forcing.date,
                                              forcing.data_spatialized):
                time = utils.date_as_mjd(dates)
                ids = forcing.hydro_units.get_ids().to_numpy()
                ts = TimeSeries.create(data_name, time, ids, data)
                if not self.model.add_time_series(ts):
                    raise RuntimeError('Failed adding time series.')

            if not self.model.attach_time_series_to_hydro_units():
                raise RuntimeError('Attaching time series failed.')

            # Check
            if not self.model.is_ok():
                raise RuntimeError('Model is not OK.')

            # Do the work
            if not self.model.run():
                raise RuntimeError('Model run failed.')

            # Processing time
            t.stop()

        except RuntimeError:
            print("An exception occurred.")

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
            'logger': 'all' if self.log_all else ''
        }
        utils.dump_config_file(settings, directory, name, file_type)

    def generate_parameters(self):
        raise Exception('Parameters cannot be generated for the base model.')

    def _set_options(self, kwargs):
        if 'solver' in kwargs:
            self.solver = kwargs['solver']
        if 'log_all' in kwargs:
            self.log_all = kwargs['log_all']
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
