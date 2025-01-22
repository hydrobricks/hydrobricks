import importlib
import os
from abc import ABC, abstractmethod

import HydroErr

import _hydrobricks as _hb
import hydrobricks as hb
from _hydrobricks import ModelHydro

from .model_settings import ModelSettings


class Model(ABC):
    """Base class for the models"""

    @abstractmethod
    def __init__(self, name=None, **kwargs):
        self.name = name
        self.model = ModelHydro()
        self.spatial_structure = None
        self.allowed_kwargs = {'solver', 'record_all', 'land_cover_types',
                               'land_cover_names'}
        self._is_initialized = False

        # Default options
        self.options = dict()
        self.solver = 'heun_explicit'
        self.record_all = False
        self.land_cover_types = ['ground']
        self.land_cover_names = ['ground']
        self.allowed_land_cover_types = ['ground']

        self._set_basic_options(kwargs)

        # Structure
        self.structure = dict()
        self.parameter_aliases = dict()
        self.parameter_constraints = []

        # Setting base settings
        self.settings = ModelSettings(
            solver=self.solver,
            record_all=self.record_all)

    def __del__(self):
        self.cleanup()

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
        if self._is_initialized:
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
                    self.settings.settings,
                    spatial_structure.settings):
                raise RuntimeError('Basin creation failed.')

            self._is_initialized = True

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
        if not self._is_initialized:
            raise RuntimeError('The model has not been initialized. '
                               'Please run setup() first.')

        if not parameters.is_ok():
            raise RuntimeError('Some parameters were not defined: '
                               f'{",".join(parameters.get_undefined())}.')

        try:
            self.model.reset()

            if forcing is not None and not forcing.is_initialized():
                forcing.apply_operations(parameters)

            self._set_parameter_values(parameters)
            self._set_forcing(forcing)

            if not self.model.is_ok():
                raise RuntimeError('Model is not OK.')

            timer = hb.utils.Timer()
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

    @staticmethod
    def cleanup():
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
        time = forcing.data2D.time.to_numpy()
        time = hb.utils.date_as_mjd(time)
        ids = self.spatial_structure.get_ids().to_numpy().flatten()
        for data_name, data in zip(forcing.data2D.data_name, forcing.data2D.data):
            data_name = str(data_name)
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

    def get_behaviours_nb(self) -> int:
        """
        Get the number of behaviours (types of behaviours) registered in the model.
        """
        return self.model.get_behaviours_nb()

    def get_behaviour_items_nb(self) -> int:
        """
        Get the number of behaviour items (individual triggers) registered in the model.
        """
        return self.model.get_behaviour_items_nb()

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
            'options': self.options,
            'land_covers': {
                'names': self.land_cover_names,
                'types': self.land_cover_types
            },
            'logger': 'all' if self.record_all else ''
        }
        hb.utils.dump_config_file(settings, directory, name, file_type)

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

    def eval(self, metric, observations, warmup=0):
        """
        Evaluate the simulation using the provided metric (goodness of fit).

        Parameters
        ----------
        metric : str
            The abbreviation of the function as defined in HydroErr
            (https://hydroerr.readthedocs.io/en/stable/list_of_metrics.html)
            Examples: nse, kge_2012, ...
        observations : np.array
            The time series of the observations with dates matching the simulated
            series.
        warmup : int
            The number of days of warmup period. This option is used to
            discard the warmup period from the evaluation. It is useful when
            conducting a run with a specific parameter set and comparing
            its score with those from the calibration. By setting the 'warmup'
            value, you can ensure fair assessments by discarding outputs
            from the specified warmup period (as is done automatically during
            calibration).

        Returns
        -------
        The value of the selected metric.
        """
        return hb.evaluate(self.get_outlet_discharge()[warmup:],
                           observations[warmup:],
                           metric)

    def generate_parameters(self):
        ps = hb.ParameterSet()
        ps.generate_parameters(self.land_cover_types, self.land_cover_names,
                               self.options, self.structure)

        for alias_key, alias_value in self.parameter_aliases.items():
            if ps.has(alias_key):
                ps.add_aliases(alias_key, alias_value)

        for constraint in self.parameter_constraints:
            ps.define_constraint(*constraint)

        return ps

    @abstractmethod
    def _define_structure(self):
        raise RuntimeError(f'The structure has to be defined by the child class '
                           f'(named {self.name}).')

    @abstractmethod
    def _set_specific_options(self, kwargs):
        raise RuntimeError(f'The specific options have to be defined by the child '
                           f'class (named {self.name}).')

    def _set_options(self, kwargs):
        self._add_allowed_kwargs(self.options.keys())
        self._validate_kwargs(kwargs)

        for key, value in kwargs.items():
            if key in ['solver', 'record_all', 'land_cover_types', 'land_cover_names']:
                continue
            self.options[key] = value

        self._set_specific_options(kwargs)
        self._check_cover_types()

    def _check_cover_types(self):
        if len(self.land_cover_names) != len(self.land_cover_types):
            raise RuntimeError('The length of the land cover names '
                               'and types do not match.')

        # Check allowed land cover types: ground, glacier
        for cover_type in self.land_cover_types:
            if cover_type not in self.allowed_land_cover_types:
                raise RuntimeError(
                    f'The land cover {cover_type} is not used in Socont')

    def _set_basic_options(self, kwargs):
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
        hb.utils.validate_kwargs(kwargs, self.allowed_kwargs)

    def _generate_structure(self):
        """
        Generate the model structure.
        """
        # Generate basic elements
        self._set_structure_basics()

        # Generate the structure
        for key, brick in self.structure.items():

            # Select or add the brick
            self._set_structure_brick(brick, key)

            # Add brick parameters if any
            if 'parameters' in brick:
                for param, value in brick['parameters'].items():
                    self.settings.add_brick_parameter(param, value)

            # Add brick processes if any
            if 'processes' in brick:
                for process, process_data in brick['processes'].items():
                    self._set_structure_process(key, process, process_data)

        self.settings.add_logging_to('outlet')

    def _set_structure_basics(self):
        with_snow = True
        snow_melt_process = 'melt:degree_day'
        if 'with_snow' in self.options:
            with_snow = self.options['with_snow']
        if 'snow_melt_process' in self.options:
            with_snow = True
            snow_melt_process = self.options['snow_melt_process']
        self.settings.generate_base_structure(
            self.land_cover_names, self.land_cover_types, with_snow=with_snow,
            snow_melt_process=snow_melt_process)

    def _set_structure_brick(self, brick, key):
        if brick['kind'] == 'land_cover':
            self.settings.select_hydro_unit_brick(key)
        else:
            if brick['attach_to'] == 'hydro_unit':
                self.settings.add_hydro_unit_brick(key, brick['kind'])
            elif brick['attach_to'] == 'sub_basin':
                self.settings.add_sub_basin_brick(key, brick['kind'])
            else:
                raise RuntimeError(f'Brick {key} has an invalid "attach_to" value.')

    def _set_structure_process(self, key, process, process_data):
        instantaneous = False
        if 'instantaneous' in process_data:
            instantaneous = process_data['instantaneous']

        log = False
        if 'log' in process_data:
            log = process_data['log']

        target = ''
        if 'target' in process_data:
            target = process_data['target']
        else:
            if not process_data['kind'].startswith('et:'):
                raise RuntimeError(f'Brick {key} has a process '
                                   f'({process}) without a target.')

        self.settings.add_brick_process(
            process, process_data['kind'], target,
            log=log, instantaneous=instantaneous)

    def _set_parameter_values(self, parameters):
        model_params = parameters.get_model_parameters()
        for _, param in model_params.iterrows():
            if not self.settings.set_parameter_value(
                    param['component'], param['name'], param['value']):
                raise RuntimeError('Failed setting parameter values.')
        self.model.update_parameters(self.settings.settings)

    def _set_forcing(self, forcing):
        if forcing is not None:
            self.set_forcing(forcing)
        elif not self.model.forcing_loaded():
            raise RuntimeError('Please provide the forcing data at least once.')
