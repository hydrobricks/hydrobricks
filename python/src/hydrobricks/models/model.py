import importlib
import os
from abc import ABC, abstractmethod

import HydroErr
import pandas as pd

import _hydrobricks as _hb
from _hydrobricks import ModelHydro, SettingsModel
from hydrobricks import utils


class Model(ABC):
    """Base class for the models"""

    @abstractmethod
    def __init__(self, name=None, **kwargs):
        self.name = name
        self.settings = SettingsModel()
        self.model = ModelHydro()
        self.spatial_structure = None
        self.allowed_kwargs = {'solver', 'record_all', 'land_cover_types',
                               'land_cover_names'}
        self.initialized = False

        # Default options
        self.solver = 'heun_explicit'
        self.record_all = False
        self.land_cover_types = ['ground']
        self.land_cover_names = ['ground']

        self._set_options(kwargs)

        # Setting base settings
        self.settings.log_all(self.record_all)
        self.settings.set_solver(self.solver)

    @property
    def name(self):
        return self._name

    @name.setter
    def name(self, name):
        self._name = name

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
            if isinstance(output_path, str) and not os.path.isdir(output_path):
                os.mkdir(output_path)

            self.spatial_structure = spatial_structure

            # Initialize log
            _hb.init_log(str(output_path))

            # Modelling period
            self.settings.set_timer(start_date, end_date, 1, "day")

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

            timer.stop(show_time=False)

        except RuntimeError:
            print("A runtime exception occurred.")
        except TypeError:
            print("A type error exception occurred.")
        except Exception:
            print("An exception occurred.")

    def cleanup(self):
        _hb.close_log()

    def initialize_state_variables(self, parameters, forcing=None):
        """
        Run the model and save the state variables as initial values.

        Parameters
        ----------
        parameters : ParameterSet
            The parameters for the given model.
        forcing : Forcing
            The forcing data.
        """
        self.run(parameters, forcing)
        self.model.save_as_initial_state()

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

    def add_behaviour(self, behaviour) -> bool:
        """
        Add a behaviour to the model.

        Parameters
        ----------
        behaviour : Behaviour
            The behaviour object. The dates must be sorted.
        """
        return self.model.add_behaviour(behaviour.behaviour)

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
            'land_covers': {
                'names': self.land_cover_names,
                'types': self.land_cover_types
            },
            'logger': 'all' if self.record_all else ''
        }
        utils.dump_config_file(settings, directory, name, file_type)

    def get_outlet_discharge(self):
        """
        Get the computed outlet discharge.
        """
        return self.model.get_outlet_discharge()

    def get_total_outlet_discharge(self):
        """
        Get the outlet discharge total.
        """
        return self.model.get_total_outlet_discharge()

    def get_total_et(self):
        """
        Get the total amount of water lost by evapotranspiration.
        """
        return self.model.get_total_et()

    def get_total_water_storage_changes(self):
        """
        Get the total change in water storage.
        """
        return self.model.get_total_water_storage_changes()

    def get_total_snow_storage_changes(self):
        """
        Get the total change in snow storage.
        """
        return self.model.get_total_snow_storage_changes()

    def dump_outputs(self, path):
        """
        Write the model outputs to a netcdf file.

        Parameters
        ----------
        path: str
            Path to the target file.
        """
        self.model.dump_outputs(path)

    def eval(self, metric, observations):
        """
        Evaluate the simulation using the provided metric (goodness of fit).

        Parameters
        ----------
        metric
            The abbreviation of the function as defined in HydroErr
            (https://hydroerr.readthedocs.io/en/stable/list_of_metrics.html)
            Examples: nse, kge_2012, ...
        observations
            The time series of the observations with dates matching the simulated
            series.

        Returns
        -------
        The value of the selected metric.
        """
        sim_ts = self.get_outlet_discharge()
        eval_fct = getattr(importlib.import_module('HydroErr'), metric)
        return eval_fct(sim_ts, observations)

    @abstractmethod
    def generate_parameters(self):
        raise Exception(f'Parameters cannot be generated for the base model '
                        f'(named {self.name}).')

    def _set_options(self, kwargs):
        if 'solver' in kwargs:
            self.solver = kwargs['solver']
        if 'record_all' in kwargs:
            self.record_all = kwargs['record_all']
        if 'land_cover_types' in kwargs:
            self.land_cover_types = kwargs['land_cover_types']
        if 'land_cover_names' in kwargs:
            self.land_cover_names = kwargs['land_cover_names']

    def _add_allowed_kwargs(self, kwargs):
        self.allowed_kwargs.update(kwargs)

    def _validate_kwargs(self, kwargs):
        # Validate optional keyword arguments.
        utils.validate_kwargs(kwargs, self.allowed_kwargs)

    @abstractmethod
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
