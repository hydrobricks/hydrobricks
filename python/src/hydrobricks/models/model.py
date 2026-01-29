from __future__ import annotations

import logging
import os
from abc import ABC, abstractmethod
from typing import TYPE_CHECKING, Any

import HydroErr
import numpy as np

from hydrobricks._exceptions import ConfigurationError, ModelError
from hydrobricks._hydrobricks import ModelHydro, close_log, init_log
from hydrobricks._utils import Timer, date_as_mjd, dump_config_file, validate_kwargs
from hydrobricks.actions.action import Action
from hydrobricks.forcing import Forcing
from hydrobricks.hydro_units import HydroUnits
from hydrobricks.models.model_settings import ModelSettings
from hydrobricks.parameters import ParameterSet
from hydrobricks.trainer import evaluate

if TYPE_CHECKING:
    from pathlib import Path

logger = logging.getLogger(__name__)


class Model(ABC):
    """Base abstract class for hydrological models"""

    @abstractmethod
    def __init__(self, name: str | None = None, **kwargs: Any) -> None:
        """
        Initialize the Model base class.

        Parameters
        ----------
        name
            Name identifier for the model instance. Default: None
        **kwargs
            Additional keyword arguments for model configuration.
            Allowed keys: 'solver', 'record_all', 'land_cover_types', 'land_cover_names'

        Raises
        ------
        TypeError
            If unsupported keyword arguments are provided.
        """
        self.name: str | None = name
        self.model: ModelHydro = ModelHydro()
        self.spatial_structure: HydroUnits | None = None
        self.allowed_kwargs: set[str] = {
            "solver",
            "record_all",
            "land_cover_types",
            "land_cover_names",
        }
        self._is_initialized: bool = False

        # Default options
        self.options: dict[str, Any] = dict()
        self.solver: str = "heun_explicit"
        self.record_all: bool = False
        self.land_cover_types: list[str] = ["ground"]
        self.land_cover_names: list[str] = ["ground"]
        self.allowed_land_cover_types: list[str] = ["ground"]

        self._set_basic_options(kwargs)

        # Structure
        self.structure: dict[str, Any] = dict()
        self.parameter_aliases: dict[str, str] = dict()
        self.parameter_constraints: list[tuple[str, ...]] = []

        # Setting base settings
        self.settings: ModelSettings = ModelSettings(
            solver=self.solver, record_all=self.record_all
        )

    def __del__(self) -> None:
        """Clean up resources when model is deleted."""
        self.cleanup()

    @property
    def name(self) -> str | None:
        """Get the model name."""
        return self._name

    @name.setter
    def name(self, name: str | None) -> None:
        """Set the model name."""
        self._name = name

    def setup(
        self,
        spatial_structure: HydroUnits,
        output_path: str | Path,
        start_date: str,
        end_date: str | None = None,
    ) -> None:
        """
        Setup and initialize the model for simulation.

        Parameters
        ----------
        spatial_structure
            The spatial structure of the catchment (hydro units).
        output_path
            Path to directory where results will be saved.
        start_date
            Starting date of the computation (format: 'YYYY-MM-DD').
        end_date
            Ending date of the computation (format: 'YYYY-MM-DD').
            If None, uses last date in time series. Default: None

        Raises
        ------
        RuntimeError
            If the model has already been initialized.
        TypeError
            If arguments have incorrect types.
        FileNotFoundError
            If the output path cannot be created.

        Examples
        --------
        >>> model = SomeModel()
        >>> model.setup(hydro_units, './output', '2020-01-01', '2020-12-31')
        """
        if self._is_initialized:
            raise ModelError(
                "The model has already been initialized. Please create a new instance.",
                is_initialized=True,
            )

        try:
            if isinstance(output_path, str) and not os.path.isdir(output_path):
                os.mkdir(output_path)

            self.spatial_structure = spatial_structure

            # Initialize log
            init_log(str(output_path))

            # Modelling period
            self.settings.set_timer(start_date, end_date, 1, "day")

            # Initialize the model (with sub basin creation)
            if not self.model.init_with_basin(
                self.settings.settings, spatial_structure.settings
            ):
                raise ModelError("Basin creation failed.", is_initialized=False)

            self._is_initialized = True

        except ModelError:
            logger.error("Model initialization failed", exc_info=True)
            raise
        except OSError as e:
            logger.error(
                f"Error creating output directory {output_path}: {e}", exc_info=True
            )
            raise ModelError(f"Failed to create output directory: {e}") from e
        except (TypeError, ValueError) as e:
            logger.error(
                f"Invalid argument type or value in model setup: {e}", exc_info=True
            )
            raise ConfigurationError(f"Invalid configuration: {e}") from e

    def run(self, parameters: ParameterSet, forcing: Forcing | None = None) -> None:
        """
        Setup and run the model.

        Parameters
        ----------
        parameters
            The parameters for the given model.
        forcing
            The forcing data.
        """
        logger.debug(f"Running model: {self.name}")

        if not self._is_initialized:
            raise ModelError(
                "The model has not been initialized. Please run setup() first.",
                is_initialized=False,
            )

        if not parameters.is_valid():
            undefined = parameters.get_undefined()
            logger.debug(f"Invalid parameters: {undefined}")
            raise ConfigurationError(
                f"Some parameters were not defined: " f'{",".join(undefined)}.'
            )

        try:
            logger.debug("Resetting model state")
            self.model.reset()

            if forcing is not None and not forcing.is_initialized():
                logger.debug("Applying forcing operations")
                forcing.apply_operations(parameters)

            logger.debug("Setting parameter values")
            self._set_parameter_values(parameters)

            logger.debug("Setting forcing data")
            self._set_forcing(forcing)

            if not self.model.is_valid():
                raise ConfigurationError("The model is not properly configured.")

            logger.debug("Starting model simulation")
            timer = Timer()
            timer.start()

            if not self.model.run():
                raise ModelError("Model run failed.")

            timer.stop(show_time=False)
            logger.debug(
                f"Model simulation completed in {timer.elapsed_time:.2f} seconds"
            )

        except ModelError:
            logger.error("Model execution failed", exc_info=True)
            raise
        except ConfigurationError:
            logger.error("Configuration error during model run", exc_info=True)
            raise
        except (TypeError, ValueError) as e:
            logger.error(
                f"Invalid argument type or value in model run: {e}", exc_info=True
            )
            raise ConfigurationError(
                f"Model run failed due to invalid input: {e}"
            ) from e

    @staticmethod
    def cleanup() -> None:
        close_log()

    def initialize_state_variables(
        self, parameters: ParameterSet, forcing: Forcing | None = None
    ) -> None:
        """
        Run the model and save the state variables as initial values.

        Parameters
        ----------
        parameters
            The parameters for the given model.
        forcing
            The forcing data.
        """
        self.run(parameters, forcing)
        self.model.save_as_initial_state()

    def set_forcing(self, forcing: Forcing) -> None:
        """
        Set the forcing data.

        Parameters
        ----------
        forcing
            The forcing data.
        """
        self.model.clear_time_series()
        time = forcing.data2D.time.to_numpy()
        time = date_as_mjd(time)
        ids = self.spatial_structure.get_ids().to_numpy().flatten()
        for data_name, data in zip(forcing.data2D.data_name, forcing.data2D.data):
            data_name = str(data_name)
            if data is None:
                raise ModelError(
                    f"The forcing {data_name} has not been spatialized.",
                    is_initialized=True,
                )
            if not self.model.create_time_series(data_name, time, ids, data):
                raise ModelError("Failed adding time series.")

        if not self.model.attach_time_series_to_hydro_units():
            raise ModelError("Attaching time series failed.")

    def add_action(self, action: Action) -> bool:
        """
        Add an action to the model.

        Parameters
        ----------
        action
            The action object. The dates must be sorted.
        """
        if not action.is_initialized:
            raise ModelError(f"The action {action.name} has not been initialized.")

        if not self._is_initialized:
            raise ModelError(
                "The model has not been initialized. "
                "Please run setup() before adding actions.",
                is_initialized=False,
            )

        return self.model.add_action(action.action)

    def get_action_count(self) -> int:
        """
        Get the number of actions (types of actions) registered in the model.
        """
        return self.model.get_action_count()

    def get_sporadic_action_item_count(self) -> int:
        """
        Get the number of sporadic (non-recursive) action items (individual triggers)
        registered in the model.
        """
        return self.model.get_sporadic_action_item_count()

    def create_config_file(
        self, directory: str, name: str, file_type: str = "both"
    ) -> None:
        """
        Create a configuration file describing the model structure.

        Such a file can be used when using the command-line version of hydrobricks. It
        contains the options to generate the corresponding model structure.

        Parameters
        ----------
        directory
            The directory to write the file.
        name
            The name of the generated file.
        file_type
            The type of file to generate: 'json', 'yaml', or 'both'.
        """
        settings = {
            "base": self.name,
            "solver": self.solver,
            "options": self.options,
            "land_covers": {
                "names": self.land_cover_names,
                "types": self.land_cover_types,
            },
            "logger": "all" if self.record_all else "",
        }
        dump_config_file(settings, directory, name, file_type)

    def get_outlet_discharge(self) -> np.ndarray:
        """
        Get the computed outlet discharge.
        """
        return self.model.get_outlet_discharge()

    def get_total_outlet_discharge(self) -> float:
        """
        Get the outlet discharge total.
        """
        return self.model.get_total_outlet_discharge()

    def get_total_et(self) -> float:
        """
        Get the total amount of water lost by evapotranspiration.
        """
        return self.model.get_total_et()

    def get_total_water_storage_changes(self) -> float:
        """
        Get the total change in water storage.
        """
        return self.model.get_total_water_storage_changes()

    def get_total_snow_storage_changes(self) -> float:
        """
        Get the total change in snow storage.
        """
        return self.model.get_total_snow_storage_changes()

    def dump_outputs(self, path: str) -> None:
        """
        Write the model outputs to a netcdf file.

        Parameters
        ----------
        path
            Path to the target file.
        """
        self.model.dump_outputs(path)

    def eval(self, metric: str, observations: np.ndarray, warmup: int = 0) -> float:
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
        warmup
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
        return evaluate(
            self.get_outlet_discharge()[warmup:], observations[warmup:], metric
        )

    def generate_parameters(self) -> ParameterSet:
        """
        Generate a ParameterSet for the model based on its structure.

        Automatically creates parameter definitions based on the model structure,
        applies any defined aliases, and applies any defined constraints.

        Returns
        -------
        ParameterSet
            A ParameterSet object with all model parameters defined.
        """
        ps = ParameterSet()
        ps.generate_parameters(
            self.land_cover_types, self.land_cover_names, self.options, self.structure
        )

        for alias_key, alias_value in self.parameter_aliases.items():
            if ps.has(alias_key):
                ps.add_aliases(alias_key, alias_value)

        for constraint in self.parameter_constraints:
            ps.define_constraint(*constraint)

        return ps

    @abstractmethod
    def _define_structure(self) -> None:
        raise ConfigurationError(
            f"The structure has to be defined by the child class (named {self.name}).",
            reason="Abstract method not implemented",
        )

    @abstractmethod
    def _set_specific_options(self, kwargs: dict[str, Any]) -> None:
        raise ConfigurationError(
            f"The specific options have to be defined by "
            f"the child class (named {self.name}).",
            reason="Abstract method not implemented",
        )

    def _set_options(self, kwargs: dict[str, Any]) -> None:
        """
        Internal method to configure model options.

        Processes keyword arguments, validates them against allowed options,
        applies basic and specific options, and validates cover types.

        Parameters
        ----------
        kwargs
            Keyword arguments containing model configuration options.

        Raises
        ------
        RuntimeError
            If land cover names and types don't match or invalid types are provided.
        """
        self._add_allowed_kwargs(self.options.keys())
        self._validate_kwargs(kwargs)

        for key, value in kwargs.items():
            if key in ["solver", "record_all", "land_cover_types", "land_cover_names"]:
                continue
            self.options[key] = value

        self._set_specific_options(kwargs)
        self._check_cover_types()

    def _check_cover_types(self) -> None:
        """
        Validate that land cover names and types are consistent.

        Verifies that:
        - Names and types lists have same length
        - All cover types are in the allowed list for this model

        Raises
        ------
        RuntimeError
            If validation fails.
        """
        if len(self.land_cover_names) != len(self.land_cover_types):
            raise ConfigurationError(
                "The length of the land cover names and types do not match.",
                reason="Mismatched array sizes",
            )

        # Check allowed land cover types: ground, glacier
        for cover_type in self.land_cover_types:
            if cover_type not in self.allowed_land_cover_types:
                raise ConfigurationError(
                    f"The land cover {cover_type} is not used in Socont",
                    item_name="land_cover_types",
                    item_value=cover_type,
                    reason="Invalid land cover type",
                )

    def _set_basic_options(self, kwargs: dict[str, Any]) -> None:
        """
        Set basic solver and model configuration options from kwargs.

        Extracts and applies the following options if present:
        - 'solver': Numerical solver name
        - 'record_all': Whether to record all state/flux values
        - 'land_cover_types': List of land cover types
        - 'land_cover_names': List of land cover names

        Parameters
        ----------
        kwargs
            Keyword arguments containing basic options.
        """
        if "solver" in kwargs:
            self.solver = kwargs["solver"]
        if "record_all" in kwargs:
            self.record_all = kwargs["record_all"]
        if "land_cover_types" in kwargs:
            self.land_cover_types = kwargs["land_cover_types"]
        if "land_cover_names" in kwargs:
            self.land_cover_names = kwargs["land_cover_names"]

    def _add_allowed_kwargs(self, kwargs: Any) -> None:
        """
        Add keys to the set of allowed keyword arguments.

        Parameters
        ----------
        kwargs
            Keys (as iterable) to add to allowed_kwargs set.
        """
        self.allowed_kwargs.update(kwargs)

    def _validate_kwargs(self, kwargs: dict[str, Any]) -> None:
        """
        Validate that all provided keyword arguments are allowed.

        Parameters
        ----------
        kwargs
            Keyword arguments to validate against allowed_kwargs.

        Raises
        ------
        TypeError
            If any kwargs are not in the allowed set.
        """
        # Validate optional keyword arguments.
        validate_kwargs(kwargs, self.allowed_kwargs)

    def _generate_structure(self) -> None:
        """
        Generate the complete model structure.

        Creates all bricks (land covers, storages, etc.) and their processes
        based on the model's structure definition. This includes:
        - Creating basic structure elements
        - Adding all defined bricks
        - Adding brick parameters
        - Adding brick processes
        - Setting up logging

        Raises
        ------
        RuntimeError
            If structure creation fails.
        """
        # Generate basic elements
        self._set_structure_basics()

        # Generate the structure
        for key, brick in self.structure.items():

            # Select or add the brick
            self._set_structure_brick(brick, key)

            # Add brick parameters if any
            if "parameters" in brick:
                for param, value in brick["parameters"].items():
                    self.settings.add_brick_parameter(param, value)

            # Add brick processes if any
            if "processes" in brick:
                for process, process_data in brick["processes"].items():
                    self._set_structure_process(key, process, process_data)

        self.settings.add_logging_to("outlet")

    def _set_structure_basics(self) -> None:
        """
        Generate basic model structure elements.

        Sets up the fundamental structure including precipitation splitting,
        land covers, and snowpack processes based on model options.

        This method extracts relevant options and calls settings methods
        to generate the base structure.
        """
        with_snow = True
        snow_melt_process = "melt:degree_day"
        snow_ice_transformation = None
        snow_redistribution = None

        if "with_snow" in self.options:
            with_snow = self.options["with_snow"]
        if "snow_melt_process" in self.options:
            with_snow = True
            snow_melt_process = self.options["snow_melt_process"]
        if "snow_ice_transformation" in self.options:
            snow_ice_transformation = self.options["snow_ice_transformation"]
        if "snow_redistribution" in self.options:
            snow_redistribution = self.options["snow_redistribution"]

        self.settings.generate_base_structure(
            self.land_cover_names,
            self.land_cover_types,
            with_snow=with_snow,
            snow_melt_process=snow_melt_process,
            snow_ice_transformation=snow_ice_transformation,
            snow_redistribution=snow_redistribution,
        )

    def _set_structure_brick(self, brick: dict[str, Any], key: str) -> None:
        """
        Add or select a brick in the model structure.

        Parameters
        ----------
        brick
            Brick definition dictionary containing 'kind' and 'attach_to' keys.
        key
            Name/identifier for the brick.

        Raises
        ------
        RuntimeError
            If brick has an invalid 'attach_to' value.
        """
        if brick["kind"] == "land_cover":
            self.settings.select_hydro_unit_brick(key)
        else:
            if brick["attach_to"] == "hydro_unit":
                self.settings.add_hydro_unit_brick(key, brick["kind"])
            elif brick["attach_to"] == "sub_basin":
                self.settings.add_sub_basin_brick(key, brick["kind"])
            else:
                raise ConfigurationError(
                    f'Brick {key} has an invalid "attach_to" value.',
                    item_name="attach_to",
                    item_value=brick.get("attach_to"),
                    reason="Invalid value",
                )

    def _set_structure_process(
        self, key: str, process: str, process_data: dict[str, Any]
    ) -> None:
        """
        Add a process to a brick in the model structure.

        Parameters
        ----------
        key
            Name of the brick containing this process.
        process
            Name/identifier for the process.
        process_data
            Process definition dictionary containing 'kind', 'target', and optional
            'log' and 'instantaneous' keys.

        Raises
        ------
        RuntimeError
            If process lacks a required target (unless it's an ET process).
        """
        instantaneous = False
        if "instantaneous" in process_data:
            instantaneous = process_data["instantaneous"]

        log = False
        if "log" in process_data:
            log = process_data["log"]

        target = ""
        if "target" in process_data:
            target = process_data["target"]
        else:
            if not process_data["kind"].startswith("et:"):
                raise ConfigurationError(
                    f"Brick {key} has a process ({process}) without a target.",
                    item_name="target",
                    reason="Missing required target",
                )

        self.settings.add_brick_process(
            process, process_data["kind"], target, log=log, instantaneous=instantaneous
        )

    def _set_parameter_values(self, parameters: ParameterSet) -> None:
        """
        Apply parameter values to the model.

        Iterates through model parameters and sets them in the model settings,
        then updates the underlying C++ model with the new parameters.

        Parameters
        ----------
        parameters
            ParameterSet containing the parameter values to apply.

        Raises
        ------
        RuntimeError
            If setting parameter values fails.
        """
        model_params = parameters.get_model_parameters()
        for _, param in model_params.iterrows():
            if not self.settings.set_parameter_value(
                param["component"], param["name"], param["value"]
            ):
                raise ModelError("Failed setting parameter values.")
        self.model.update_parameters(self.settings.settings)

    def _set_forcing(self, forcing: Forcing | None) -> None:
        """
        Attach forcing data to the model.

        If forcing is provided, spatializes it and attaches it to hydro units.
        If no forcing is provided, verifies that forcing was previously loaded.

        Parameters
        ----------
        forcing
            Forcing object with meteorological data, or None if already set.

        Raises
        ------
        RuntimeError
            If no forcing data is available.
        """
        if forcing is not None:
            self.set_forcing(forcing)
        elif not self.model.forcing_loaded():
            raise ModelError(
                "Please provide the forcing data at least once.", is_initialized=False
            )
