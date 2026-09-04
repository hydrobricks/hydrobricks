"""Load a ready-to-run model setup from a declarative YAML project file.

A *project file* describes the canonical station-CSV, elevation-band workflow as
data: which model to build, where the hydro units / meteo / discharge files are,
how to spatialize the forcing, the modelling periods and the parameter values.
:func:`load_project` validates the whole file up front (reporting every problem
at once, with file and column checks) and returns the wired-up objects — the
same ones the step-by-step API produces — so anything the file does not cover
can still be done in Python on the returned :class:`Project`.

Example project file::

    model:
      name: socont
      options:
        soil_storage_nb: 2
        surface_runoff: linear_storage

    hydro_units:
      file: hydro_units.csv

    forcing:
      file: meteo.csv
      time: {column: date, format: "%d/%m/%Y"}
      columns:
        precipitation: precip(mm/day)
        temperature: temp(C)
      ref_elevation: 1253
      precipitation: {correction_factor: 0.75, gradient: 0.05}
      pet: {method: Oudin, lat: 47.3}

    observations:
      file: discharge.csv
      time: {column: Date, format: "%d/%m/%Y"}
      column: Discharge (mm/d)

    periods:
      calibration: [1981-01-01, 2000-12-31]
      validation: [2001-01-01, 2020-12-31]
      spinup: 2y

    parameters:
      A: 458
      a_snow: 2

    calibration:
      algorithm: sceua
      repetitions: 300
      objective: kge_2012
      transform: power(0.2)
      parameters: [a_snow, A]

The optional ``calibration`` section declares how to optimize the parameters
(see :meth:`Project.calibrate`); ``observations`` and a calibration period are
then required.

The forcing can also come from gridded netCDF data — per variable, mixable
with the station CSV — using a ``gridded`` section (the hydro units then need
a ``unit_ids_raster`` to aggregate the grid cells, and optionally an
``outline`` + ``dem`` to derive elevation gradients from the data)::

    hydro_units:
      file: hydro_units.csv
      unit_ids_raster: unit_ids.tif

    forcing:
      gridded:
        precipitation:
          path: RhiresD_v2.0_swiss.lv95
          file_pattern: "RhiresD_*.nc"
          var_name: RhiresD
          data_crs: 2056
          dim_x: E
          dim_y: N

Land covers beyond the single default soil cover are declared on the model
(the ``land_cover_types`` / ``land_cover_names`` options); the data they are
initialized from goes in ``hydro_units.land_covers``. A glacier extent (an
outline shapefile or an ice-thickness raster) sets the glacier fraction of every
hydro unit, optionally split at the equilibrium line altitude into ice (below)
and firn (above), the PREVAH-style two-cover setup::

    model:
      name: socont
      options:
        land_cover_types: [ground, glacier]
        land_cover_names: [ground, glacier]

    hydro_units:
      discretization: {method: equal_intervals, distance: 100}
      outline: catchment.shp
      dem: dem.tif
      land_covers:
        glacier: {outline: glaciers.shp}    # or ice_thickness: thickness.tif

A model that declares no glacier cover ignores the glacier source (the
glacierized area stays in the soil cover), so one catchment section serves every
model of a comparison.

The lateral processes (e.g. the snow redistribution) need to know how the hydro
units drain into each other, and the per-unit slope. Both come from the
preprocessing: the connectivity table produced by
``Catchment.calculate_connectivity`` is declared as
``hydro_units.connectivity``, and the slope is always part of the hydro units
(computed at discretization, and read back from a hydro units CSV)::

    hydro_units:
      file: hydro_units.csv
      unit_ids_raster: unit_ids.tif
      connectivity: connectivity.csv

A glacier evolution can be replayed during the simulation from a precomputed
lookup table (``GlacierEvolutionDeltaH`` / ``GlacierEvolutionAreaScaling``, see
the preprocessing examples), through the optional ``actions`` section::

    actions:
      glacier_evolution:
        method: delta_h            # or 'area_scaling'
        lookup_tables: glacier/    # the directory holding the two lookup CSVs
        land_cover: glacier        # default: 'glacier'
        update_month: October      # default: 'October'

The model can also be a custom structure declared as data (see
:class:`~hydrobricks.models.custom.CustomModel`): use
``model: {structure: my_structure.yaml}`` instead of a pre-built ``name``.

Relative paths are resolved against the project file location. For anything
beyond these canonical cases (custom calibration logic, land cover changes)
use the Python API, starting from the objects returned by
:func:`load_project`.
"""

from __future__ import annotations

import difflib
import logging
import numbers
import re
from dataclasses import dataclass, field
from pathlib import Path
from typing import Any

import pandas as pd
import yaml

from hydrobricks._exceptions import (
    ConfigurationError,
    DependencyError,
    HydroBricksError,
)
from hydrobricks.evaluation.discharge import DischargeObservations
from hydrobricks.forcing import Forcing
from hydrobricks.hydro_units import HydroUnits
from hydrobricks.models.model import Model
from hydrobricks.parameters import ParameterSet
from hydrobricks.periods import Period, Periods

logger = logging.getLogger(__name__)

_TOP_LEVEL_KEYS = {
    "model",
    "hydro_units",
    "forcing",
    "observations",
    "periods",
    "parameters",
    "data_parameters",
    "actions",
    "calibration",
    "output",
    "cache",
}
_REQUIRED_TOP_LEVEL_KEYS = ("model", "hydro_units", "forcing", "periods")


def _model_registry() -> dict[str, type[Model]]:
    """The available model classes, keyed by normalized name (e.g. 'socont')."""
    import hydrobricks.models as models

    registry = {}
    for attr in models.__all__:
        obj = getattr(models, attr)
        if (
            isinstance(obj, type)
            and issubclass(obj, Model)
            and obj is not Model
            and getattr(obj, "registrable", True)
        ):
            registry[attr.lower()] = obj
    return registry


@dataclass
class Project:
    """The wired-up objects built from a project file by :func:`load_project`.

    Attributes
    ----------
    model
        The model instance, already ``setup()`` over the full simulation span
        (with the project spin-up). Call :meth:`run` or ``model.run(...)``.
    forcing
        The :class:`~hydrobricks.forcing.Forcing` with its spatialization
        operations defined (applied lazily at run time).
    parameters
        The generated :class:`~hydrobricks.parameters.ParameterSet`, with the
        values from the project file applied. If the file does not value every
        parameter, set the remaining ones before running.
    observations
        The loaded observed discharge, or ``None`` when the project file has no
        ``observations`` section.
    periods
        The :class:`~hydrobricks.periods.Periods` (calibration / validation /
        simulation and spin-up policy) declared in the project file.
    config
        The raw configuration mapping the project was built from.
    path
        The project file path, or ``None`` when built from a dict.
    output_dir
        The resolved output directory the model writes to.
    hydro_units
        The :class:`~hydrobricks.hydro_units.HydroUnits` (loaded or delineated).
    catchment
        The :class:`~hydrobricks.catchment.Catchment`, when the project
        declares an ``outline``/``dem``; otherwise ``None``.
    calibration
        The validated ``calibration`` section (algorithm, repetitions,
        objective, transform, parameters), or ``None`` when the project file
        has none. Used as the defaults of :meth:`calibrate`.
    base_dir
        The directory the configuration paths were resolved against.
    """

    model: Model
    forcing: Forcing
    parameters: ParameterSet
    observations: DischargeObservations | None
    periods: Periods
    config: dict = field(repr=False)
    path: Path | None = None
    output_dir: Path | None = None
    hydro_units: HydroUnits | None = None
    catchment: Any | None = None
    calibration: dict | None = field(default=None, repr=False)
    base_dir: Path | None = None
    actions: list = field(default_factory=list, repr=False)

    def setup(self, period: Period | tuple | str | None = None) -> None:
        """Set the model up, over the full simulation span or a given period.

        Only needed when the project was loaded with ``setup=False`` (e.g. to
        configure recordings for auxiliary observations first), or to set the
        model up over one declared period instead of the full span (e.g. the
        calibration period for a calibration run).

        Parameters
        ----------
        period
            ``None`` (default) for the full simulation span with the declared
            spin-up; a declared period name (``'calibration'``, ``'validation'``,
            ``'simulation'``); or a :class:`~hydrobricks.periods.Period` /
            ``(start, end)`` pair. Named or explicit periods use the spin-up
            policy clamped to the period (``periods.spinup_days_for``).
        """
        if period is None:
            span = self.periods.full_span
            spinup: int | str = self.periods.spinup
        else:
            if isinstance(period, str):
                declared = self.periods.defined_periods()
                if period not in declared:
                    raise ConfigurationError(
                        f"Unknown period '{period}'. Declared periods: "
                        f"{', '.join(declared)}.",
                        item_name="period",
                        item_value=period,
                        reason="Unknown period name",
                    )
                span = declared[period]
            else:
                span = Period.coerce(period)
            spinup = self.periods.spinup_days_for(span)
        start_date, end_date = span.bounds
        self.model.setup(
            spatial_structure=self.hydro_units,
            output_path=str(self.output_dir),
            start_date=start_date,
            end_date=end_date,
            spinup=spinup,
        )
        # The actions can only be registered on a model that is set up.
        for action in self.actions:
            self.model.add_action(action)

    def run(self) -> pd.Series:
        """Run the model over the simulation span and return the discharge.

        Returns
        -------
        The simulated outlet discharge as a date-indexed series.

        Raises
        ------
        ConfigurationError
            If some parameters still have no value (they are listed with their
            valid ranges).
        """
        if not self.parameters.is_valid():
            missing = []
            for _, row in self.parameters.parameters.iterrows():
                if row["value"] is not None:
                    continue
                missing.append(f"{_parameter_label(row)} {_parameter_range(row)}")
            raise ConfigurationError(
                "Some parameters have no value; add them to the 'parameters' "
                "section of the project file or set them on project.parameters "
                "before running:\n- " + "\n- ".join(missing),
                item_name="parameters",
                reason="Undefined parameter values",
            )
        self.model.run(parameters=self.parameters, forcing=self.forcing)
        discharge = self.model.get_outlet_discharge()
        time = self.model.get_recorded_time()
        return pd.Series(discharge, index=time, name="discharge")

    def calibrate(
        self,
        algorithm: str | None = None,
        repetitions: int | None = None,
        objective: str | None = None,
        transform: Any = None,
        parameters: list[str] | None = None,
        dbname: str | None = None,
        dbformat: str = "ram",
        parallel: str = "seq",
        **calibrate_kwargs: Any,
    ) -> dict:
        """Calibrate the project parameters on the calibration period.

        The settings come from the project file's ``calibration`` section;
        every argument given here overrides its file counterpart. The best
        parameter values are applied to :attr:`parameters` on return, so a
        subsequent :meth:`run` simulates the calibrated model over the full
        span (the recommended flow: calibrate, run, then
        :func:`~hydrobricks.periods.evaluate_periods`).

        The model cannot be set up twice, so the calibration runs on a fresh
        internal build of the project over the calibration period; this
        project's model stays set up over the full simulation span.

        Parameters
        ----------
        algorithm
            SPOTPY algorithm name (e.g. ``'sceua'``, ``'mc'``).
            Default: the file value, or ``'sceua'``.
        repetitions
            Number of runs the algorithm may use. Required here or in the file.
        objective
            Metric name to optimize (e.g. ``'kge_2012'``). Default: the file
            value, or the trainer default (non-parametric KGE).
        transform
            Discharge transformation applied within the objective — anything
            :meth:`DischargeTransform.from_spec
            <hydrobricks.evaluation.transforms.DischargeTransform.from_spec>`
            accepts (e.g. ``'power(0.2)'``). Default: the file value, or none.
        parameters
            Names of the parameters to calibrate (model parameters or
            data_parameters). Required here or in the file.
        dbname, dbformat, parallel, **calibrate_kwargs
            Forwarded to :func:`hydrobricks.trainer.calibrate`.

        Returns
        -------
        dict
            The :func:`~hydrobricks.trainer.get_best` record (``score``,
            ``parameters``, ``index``) extended with ``sampler``,
            ``algorithm`` and ``repetitions``.
        """
        import hydrobricks.trainer as trainer

        if self.observations is None:
            raise ConfigurationError(
                "The project has no observed discharge; add an 'observations' "
                "section to calibrate.",
                item_name="observations",
                reason="Missing observations",
            )
        if self.periods.calibration is None:
            raise ConfigurationError(
                "The project declares no calibration period; add one to the "
                "'periods' section to calibrate.",
                item_name="periods.calibration",
                reason="Missing calibration period",
            )

        defaults = self.calibration or {}
        algorithm = algorithm or defaults.get("algorithm") or "sceua"
        repetitions = repetitions or defaults.get("repetitions")
        if not repetitions:
            raise ConfigurationError(
                "The number of repetitions is required: pass repetitions= or "
                "set calibration.repetitions in the project file.",
                item_name="calibration.repetitions",
                reason="Missing repetitions",
            )
        objective = objective or defaults.get("objective")
        if transform is None:
            transform = defaults.get("transform")
        parameters = parameters or defaults.get("parameters")
        if not parameters:
            raise ConfigurationError(
                "The parameters to calibrate are required: pass parameters= or "
                "set calibration.parameters in the project file.",
                item_name="calibration.parameters",
                reason="Missing parameters to calibrate",
            )
        errors: list[str] = []
        _check_parameter_names(
            self.parameters, parameters, errors, section="calibration.parameters"
        )
        _raise_if_errors(errors, self.path)

        # Fresh build over the calibration period (a model cannot be re-setup).
        calib = load_project(self.config, base_dir=self.base_dir, setup=False)
        calib.setup(period="calibration")
        calib.parameters.allow_changing = list(parameters)

        spot_setup = trainer.SpotpySetup(
            calib.model,
            calib.parameters,
            calib.forcing,
            calib.observations,
            obj_func=objective,
            transform=transform,
            periods=calib.periods,
        )
        sampler = trainer.calibrate(
            spot_setup,
            algorithm,
            repetitions,
            dbname=dbname,
            dbformat=dbformat,
            parallel=parallel,
            **calibrate_kwargs,
        )
        best = trainer.get_best(sampler)
        self.parameters.set_values(best["parameters"])
        return {
            **best,
            "sampler": sampler,
            "algorithm": algorithm,
            "repetitions": repetitions,
        }


def load_project(
    source: str | Path | dict,
    base_dir: str | Path | None = None,
    setup: bool = True,
) -> Project:
    """Build a ready-to-run model setup from a YAML project file (or dict).

    The configuration is validated as a whole before anything is built: unknown
    keys (with 'did you mean' suggestions), missing files, missing CSV columns,
    wrong types and unknown model or parameter names are all reported together
    in a single :class:`~hydrobricks._exceptions.ConfigurationError`.

    Parameters
    ----------
    source
        Path to a YAML project file, or an equivalent (already parsed) mapping.
    base_dir
        Directory used to resolve the relative paths in the configuration.
        Defaults to the project file directory (or the current working
        directory when ``source`` is a dict).
    setup
        Whether to ``setup()`` the model over the full simulation span
        (default). Pass ``False`` when something must happen between the model
        construction and its setup — e.g. configuring recordings for auxiliary
        observations — then call :meth:`Project.setup` yourself.

    Returns
    -------
    Project
        The wired ``(model, forcing, parameters, observations, periods)``
        bundle.
    """
    path: Path | None = None
    if isinstance(source, (str, Path)):
        path = Path(source)
        if not path.is_file():
            raise ConfigurationError(
                f"The project file '{path}' does not exist.",
                item_name="source",
                item_value=str(path),
                reason="File not found",
            )
        try:
            with open(path, encoding="utf-8") as f:
                config = yaml.safe_load(f)
        except yaml.YAMLError as err:
            raise ConfigurationError(
                f"The project file '{path}' is not valid YAML: {err}",
                item_name="source",
                reason="Invalid YAML",
            ) from None
        base = Path(base_dir) if base_dir is not None else path.parent
    elif isinstance(source, dict):
        config = source
        base = Path(base_dir) if base_dir is not None else Path.cwd()
    else:
        raise ConfigurationError(
            "The project source must be a path to a YAML file or a dict, "
            f"not {type(source).__name__}.",
            item_name="source",
            reason="Invalid type",
        )

    if not isinstance(config, dict):
        raise ConfigurationError(
            "The project file must contain a mapping of sections "
            "(model, hydro_units, forcing, periods, ...).",
            item_name="source",
            reason="Not a mapping",
        )

    errors: list[str] = []
    cfg = _validate_config(config, base, errors)
    _raise_if_errors(errors, path)

    return _build_project(cfg, config, path, errors, setup)


# --- Validation -------------------------------------------------------------


def _raise_if_errors(errors: list[str], path: Path | None) -> None:
    if not errors:
        return
    where = f" '{path}'" if path is not None else ""
    plural = "s" if len(errors) > 1 else ""
    raise ConfigurationError(
        f"The project file{where} has {len(errors)} problem{plural}:\n- "
        + "\n- ".join(errors),
        item_name="project",
        reason="Invalid project configuration",
    )


def _suggest(key: str, valid: set[str]) -> str:
    matches = difflib.get_close_matches(key, sorted(valid), n=1)
    return f" (did you mean '{matches[0]}'?)" if matches else ""


def _check_keys(section: dict, valid: set[str], where: str, errors: list[str]) -> None:
    for key in section:
        if key not in valid:
            errors.append(
                f"{where}: unknown key '{key}'{_suggest(str(key), valid)}. "
                f"Valid keys: {', '.join(sorted(valid))}."
            )


def _get_mapping(
    config: dict, key: str, errors: list[str], required: bool
) -> dict | None:
    if key not in config or config[key] is None:
        if required:
            errors.append(f"{key}: this section is required.")
        return None
    if not isinstance(config[key], dict):
        errors.append(f"{key}: this section must be a mapping.")
        return None
    return config[key]


def _get_str(section: dict, key: str, where: str, errors: list[str]) -> str | None:
    value = section.get(key)
    if value is None:
        return None
    if not isinstance(value, str):
        errors.append(f"{where}.{key}: expected a string, got {value!r}.")
        return None
    return value


def _get_bool(section: dict, key: str, where: str, errors: list[str]) -> bool | None:
    value = section.get(key)
    if value is None:
        return None
    if not isinstance(value, bool):
        errors.append(f"{where}.{key}: expected a boolean, got {value!r}.")
        return None
    return value


def _get_number(section: dict, key: str, where: str, errors: list[str]) -> float | None:
    value = section.get(key)
    if value is None:
        return None
    if isinstance(value, bool) or not isinstance(value, numbers.Real):
        errors.append(f"{where}.{key}: expected a number, got {value!r}.")
        return None
    return float(value)


def _get_number_or_param(
    section: dict, key: str, where: str, errors: list[str]
) -> float | str | None:
    """A number, or a 'param:<name>' reference to a calibratable data parameter."""
    value = section.get(key)
    if value is None:
        return None
    if isinstance(value, str):
        if re.fullmatch(r"param:\w+", value):
            return value
        errors.append(
            f"{where}.{key}: expected a number or a 'param:<name>' reference, "
            f"got {value!r}."
        )
        return None
    if isinstance(value, bool) or not isinstance(value, numbers.Real):
        errors.append(
            f"{where}.{key}: expected a number or a 'param:<name>' reference, "
            f"got {value!r}."
        )
        return None
    return float(value)


def _resolve_file(
    section: dict, base: Path, where: str, errors: list[str]
) -> Path | None:
    name = _get_str(section, "file", where, errors)
    if name is None:
        if "file" not in section:
            errors.append(f"{where}.file: a file path is required.")
        return None
    file = Path(name)
    if not file.is_absolute():
        file = base / file
    if not file.is_file():
        errors.append(f"{where}.file: '{file}' does not exist.")
        return None
    return file


def _csv_columns(file: Path, where: str, errors: list[str]) -> list[str] | None:
    try:
        return list(pd.read_csv(file, nrows=0).columns)
    except Exception as err:
        errors.append(f"{where}.file: cannot read '{file}' as CSV ({err}).")
        return None


def _check_columns(
    needed: dict[str, str],
    available: list[str] | None,
    file: Path | None,
    errors: list[str],
) -> None:
    if available is None or file is None:
        return
    for where, column in needed.items():
        if column not in available:
            errors.append(
                f"{where}: column '{column}' not found in '{file.name}'"
                f"{_suggest(column, set(available))}. Available columns: "
                f"{', '.join(available)}."
            )


def _validate_time_section(
    section: dict, where: str, errors: list[str]
) -> tuple[str, str]:
    time = section.get("time")
    if time is None:
        return "date", "%Y-%m-%d"
    if not isinstance(time, dict):
        errors.append(f"{where}.time: expected a mapping (column, format).")
        return "date", "%Y-%m-%d"
    _check_keys(time, {"column", "format"}, f"{where}.time", errors)
    column = _get_str(time, "column", f"{where}.time", errors) or "date"
    fmt = _get_str(time, "format", f"{where}.time", errors) or "%Y-%m-%d"
    return column, fmt


def _validate_model(config: dict, base: Path, errors: list[str]) -> dict:
    out: dict[str, Any] = {}
    section = _get_mapping(config, "model", errors, required=True)
    if section is None:
        return out
    _check_keys(section, {"name", "structure", "options"}, "model", errors)

    structure = section.get("structure")
    if structure is not None and "name" in section:
        errors.append(
            "model: 'name' and 'structure' are mutually exclusive (use 'name' "
            "for a pre-built model, 'structure' for a custom one)."
        )
    if isinstance(structure, str):
        # A path to a structure file, validated/loaded by CustomModel.
        path = Path(structure)
        if not path.is_absolute():
            path = base / path
        if not path.is_file():
            errors.append(f"model.structure: '{path}' does not exist.")
        else:
            out["structure"] = path
    elif isinstance(structure, dict):
        # An inline structure definition.
        out["structure"] = structure
    elif structure is not None:
        errors.append(
            "model.structure: expected a path to a structure file or an "
            "inline structure mapping."
        )

    registry = _model_registry()
    name = _get_str(section, "name", "model", errors)
    if name is None:
        if "name" not in section and structure is None:
            errors.append(
                "model: a pre-built model 'name' or a custom 'structure' is "
                "required."
            )
    else:
        normalized = name.lower().replace("-", "").replace("_", "")
        if normalized not in registry:
            errors.append(
                f"model.name: unknown model '{name}'"
                f"{_suggest(normalized, set(registry))}. Available models: "
                f"{', '.join(sorted(registry))}."
            )
        else:
            out["class"] = registry[normalized]

    options = section.get("options", {}) or {}
    if not isinstance(options, dict):
        errors.append("model.options: expected a mapping of model options.")
        options = {}
    out["options"] = options
    out["land_cover_types"], out["land_cover_names"] = _validate_land_cover_options(
        options, errors
    )
    return out


def _validate_land_cover_options(
    options: dict, errors: list[str]
) -> tuple[list[str], list[str]]:
    """The land covers declared in the model options (a single soil cover by default).

    Only used for the cross-checks against the hydro units; the options themselves
    are passed to the model class, which validates the cover types it allows.
    """
    declared: dict[str, list[str]] = {}
    for key in ("land_cover_types", "land_cover_names"):
        value = options.get(key)
        if value is None:
            continue
        if not isinstance(value, list) or not all(isinstance(v, str) for v in value):
            errors.append(f"model.options.{key}: expected a list of names.")
            continue
        declared[key] = list(value)

    types = declared.get("land_cover_types")
    names = declared.get("land_cover_names")
    if types is None and names is None:
        return ["open"], ["open"]
    if types is None or names is None:
        errors.append(
            "model.options: 'land_cover_types' and 'land_cover_names' must be "
            "declared together (one type and one name per land cover)."
        )
        return ["open"], ["open"]
    if len(types) != len(names):
        errors.append(
            "model.options: 'land_cover_types' and 'land_cover_names' must have "
            f"the same length (got {len(types)} and {len(names)})."
        )
        return ["open"], ["open"]
    return types, names


def _resolve_optional_path(
    section: dict, key: str, base: Path, where: str, errors: list[str]
) -> Path | None:
    """Resolve an optional file path in a section (error if given but missing)."""
    name = _get_str(section, key, where, errors)
    if name is None:
        return None
    path = Path(name)
    if not path.is_absolute():
        path = base / path
    if not path.exists():
        errors.append(f"{where}.{key}: '{path}' does not exist.")
        return None
    return path


def _validate_discretization(section: dict, errors: list[str]) -> dict | None:
    discretization = section.get("discretization")
    if discretization is None:
        return None
    if not isinstance(discretization, dict):
        errors.append(
            "hydro_units.discretization: expected a mapping (method, "
            "distance/number)."
        )
        return None
    where = "hydro_units.discretization"
    valid = {
        "method",
        "distance",
        "number",
        "min_elevation",
        "max_elevation",
        "split_discontinuous",
        "min_patch_area",
        "connectivity",
    }
    _check_keys(discretization, valid, where, errors)

    out: dict[str, Any] = {}
    method = _get_str(discretization, "method", where, errors) or "equal_intervals"
    if method not in ("equal_intervals", "quantiles"):
        errors.append(
            f"{where}.method: expected 'equal_intervals' or 'quantiles', got "
            f"'{method}'."
        )
        method = "equal_intervals"
    out["method"] = method
    for key in ("distance", "number", "min_elevation", "max_elevation"):
        out[key] = _get_number(discretization, key, where, errors)

    # Spatially discontinuous hydro units (several patches sharing an id) are the
    # default; they can optionally be split into one hydro unit per patch.
    out["split_discontinuous"] = _get_bool(
        discretization, "split_discontinuous", where, errors
    )
    out["min_patch_area"] = _get_number(discretization, "min_patch_area", where, errors)
    connectivity = _get_number(discretization, "connectivity", where, errors)
    if connectivity is not None and connectivity not in (4, 8):
        errors.append(f"{where}.connectivity: expected 4 or 8, got {connectivity!r}.")
        connectivity = None
    out["connectivity"] = None if connectivity is None else int(connectivity)
    return out


def _validate_land_covers(section: dict, base: Path, errors: list[str]) -> dict:
    """Validate the data sources the land cover fractions are initialized from.

    Only the glacier cover can be derived from data (a glacier outline or an
    ice-thickness raster); the other covers are given as area columns of the hydro
    units CSV ('columns_areas').
    """
    covers = section.get("land_covers")
    if covers is None:
        return {}
    if not isinstance(covers, dict) or not covers:
        errors.append(
            "hydro_units.land_covers: expected a mapping of land cover type to "
            "its data source, e.g. {glacier: {outline: glaciers.shp}}."
        )
        return {}

    out: dict[str, dict] = {}
    for cover_type, spec in covers.items():
        where = f"hydro_units.land_covers.{cover_type}"
        if cover_type != "glacier":
            errors.append(
                f"{where}: unsupported land cover source '{cover_type}'; only "
                "'glacier' can be initialized from data (give the other covers "
                "as area columns of the hydro units CSV, see 'columns_areas')."
            )
            continue
        if not isinstance(spec, dict):
            errors.append(
                f"{where}: expected a mapping (outline or ice_thickness, ela)."
            )
            continue
        _check_keys(spec, {"outline", "ice_thickness", "ela"}, where, errors)
        if ("outline" in spec) == ("ice_thickness" in spec):
            errors.append(
                f"{where}: provide either 'outline' (a glacier outline "
                "shapefile) or 'ice_thickness' (a thickness raster), not both."
            )
        out[cover_type] = {
            "outline": _resolve_optional_path(spec, "outline", base, where, errors),
            "ice_thickness": _resolve_optional_path(
                spec, "ice_thickness", base, where, errors
            ),
            "ela": _get_number(spec, "ela", where, errors),
        }
    return out


def _validate_hydro_units(config: dict, base: Path, errors: list[str]) -> dict:
    out: dict[str, Any] = {}
    section = _get_mapping(config, "hydro_units", errors, required=True)
    if section is None:
        return out
    valid = {
        "file",
        "columns",
        "columns_areas",
        "unit_ids_raster",
        "connectivity",
        "outline",
        "dem",
        "discretization",
        "land_covers",
    }
    _check_keys(section, valid, "hydro_units", errors)

    out["discretization"] = _validate_discretization(section, errors)
    out["land_covers"] = _validate_land_covers(section, base, errors)

    file = None
    if "file" in section:
        file = _resolve_file(section, base, "hydro_units", errors)
        if out["discretization"] is not None:
            errors.append(
                "hydro_units: 'file' and 'discretization' are mutually "
                "exclusive; load the hydro units from the CSV or delineate "
                "them from the DEM, not both."
            )
    elif out["discretization"] is None:
        errors.append(
            "hydro_units: provide either 'file' (a CSV of hydro units) or "
            "'discretization' (elevation bands delineated from the DEM)."
        )
    if out["discretization"] is not None:
        if "outline" not in section or "dem" not in section:
            errors.append(
                "hydro_units.discretization: requires 'outline' and 'dem' "
                "(the catchment to delineate)."
            )
        for key in ("columns", "columns_areas"):
            if key in section:
                errors.append(
                    f"hydro_units.{key}: only applies when the hydro units "
                    "are loaded from a CSV 'file'."
                )
        if "unit_ids_raster" in section:
            errors.append(
                "hydro_units.unit_ids_raster: not needed with "
                "'discretization' (the raster is generated in the output "
                "directory)."
            )
    out["file"] = file

    out["unit_ids_raster"] = _resolve_optional_path(
        section, "unit_ids_raster", base, "hydro_units", errors
    )
    out["connectivity"] = _resolve_optional_path(
        section, "connectivity", base, "hydro_units", errors
    )
    out["outline"] = _resolve_optional_path(
        section, "outline", base, "hydro_units", errors
    )
    out["dem"] = _resolve_optional_path(section, "dem", base, "hydro_units", errors)
    if ("outline" in section) != ("dem" in section):
        errors.append(
            "hydro_units: 'outline' and 'dem' must be provided together (they "
            "define the catchment used to derive elevation gradients)."
        )

    if out["land_covers"]:
        # The fractions are computed on the DEM grid, per hydro unit.
        if out["outline"] is None or out["dem"] is None:
            errors.append(
                "hydro_units.land_covers: requires 'outline' and 'dem' (the "
                "cover fractions are rasterized on the catchment DEM)."
            )
        if out["discretization"] is None and out["unit_ids_raster"] is None:
            errors.append(
                "hydro_units.land_covers: requires 'discretization' or "
                "'unit_ids_raster' (the hydro unit of every DEM cell must be "
                "known to compute the cover fractions)."
            )

    columns = section.get("columns", {}) or {}
    if not isinstance(columns, dict):
        errors.append("hydro_units.columns: expected a mapping.")
        columns = {}
    out["column_elevation"] = columns.get("elevation", "elevation")
    out["column_area"] = columns.get("area", "area")
    out["other_columns"] = {
        prop: col for prop, col in columns.items() if prop not in ("elevation", "area")
    }

    columns_areas = section.get("columns_areas")
    if columns_areas is not None and not isinstance(columns_areas, dict):
        errors.append(
            "hydro_units.columns_areas: expected a mapping of land cover "
            "name to area column."
        )
        columns_areas = None
    out["columns_areas"] = columns_areas

    if file is not None:
        available = _csv_columns(file, "hydro_units", errors)
        needed = {"hydro_units.columns.elevation": out["column_elevation"]}
        if columns_areas is None:
            needed["hydro_units.columns.area"] = out["column_area"]
        else:
            # With per-land-cover areas the total area column is not used.
            for name, col in columns_areas.items():
                needed[f"hydro_units.columns_areas.{name}"] = col
        for prop, col in out["other_columns"].items():
            needed[f"hydro_units.columns.{prop}"] = col
        _check_columns(needed, available, file, errors)
    return out


def _validate_station_forcing(section: dict, base: Path, errors: list[str]) -> dict:
    out: dict[str, Any] = {}
    file = _resolve_file(section, base, "forcing", errors)
    out["file"] = file
    out["time_column"], out["time_format"] = _validate_time_section(
        section, "forcing", errors
    )

    columns = section.get("columns")
    if not isinstance(columns, dict) or not columns:
        errors.append(
            "forcing.columns: a mapping of variable to CSV column is required, "
            "e.g. {precipitation: 'precip(mm/day)', temperature: 'temp(C)'}."
        )
        columns = {}
    out["columns"] = columns

    out["ref_elevation"] = _get_number(section, "ref_elevation", "forcing", errors)

    temperature = section.get("temperature", {}) or {}
    if not isinstance(temperature, dict):
        errors.append("forcing.temperature: expected a mapping (gradient).")
        temperature = {}
    _check_keys(temperature, {"gradient"}, "forcing.temperature", errors)
    gradient = _get_number_or_param(
        temperature, "gradient", "forcing.temperature", errors
    )
    out["temperature_gradient"] = -0.6 if gradient is None else gradient

    precipitation = section.get("precipitation", {}) or {}
    if not isinstance(precipitation, dict):
        errors.append(
            "forcing.precipitation: expected a mapping "
            "(correction_factor, gradient)."
        )
        precipitation = {}
    _check_keys(
        precipitation,
        {"correction_factor", "gradient"},
        "forcing.precipitation",
        errors,
    )
    out["precip_correction_factor"] = _get_number_or_param(
        precipitation, "correction_factor", "forcing.precipitation", errors
    )
    out["precip_gradient"] = _get_number_or_param(
        precipitation, "gradient", "forcing.precipitation", errors
    )

    needs_ref = "temperature" in columns or out["precip_gradient"] is not None
    if needs_ref and out["ref_elevation"] is None:
        errors.append(
            "forcing.ref_elevation: the station elevation is required to "
            "spatialize with elevation gradients."
        )

    if file is not None:
        available = _csv_columns(file, "forcing", errors)
        needed = {"forcing.time.column": out["time_column"]}
        for variable, col in columns.items():
            needed[f"forcing.columns.{variable}"] = col
        _check_columns(needed, available, file, errors)
    return out


def _validate_gridded_forcing(gridded: Any, base: Path, errors: list[str]) -> dict:
    if gridded is None:
        return {}
    if not isinstance(gridded, dict) or not gridded:
        errors.append(
            "forcing.gridded: expected a mapping of variable to netCDF source, "
            "e.g. {precipitation: {path: precip.nc, var_name: RhiresD}}."
        )
        return {}

    valid = {
        "path",
        "file_pattern",
        "var_name",
        "data_crs",
        "dim_time",
        "dim_x",
        "dim_y",
        "apply_data_gradient",
        "gradient_type",
    }
    out: dict[str, dict] = {}
    for variable, spec in gridded.items():
        where = f"forcing.gridded.{variable}"
        if not isinstance(spec, dict):
            errors.append(f"{where}: expected a mapping (path, var_name, ...).")
            continue
        _check_keys(spec, valid, where, errors)
        cfg: dict[str, Any] = {}

        name = _get_str(spec, "path", where, errors)
        path = None
        if name is None:
            if "path" not in spec:
                errors.append(f"{where}.path: a netCDF file or folder is required.")
        else:
            path = Path(name)
            if not path.is_absolute():
                path = base / path
            if not path.exists():
                errors.append(f"{where}.path: '{path}' does not exist.")
                path = None
        cfg["path"] = path

        cfg["file_pattern"] = _get_str(spec, "file_pattern", where, errors)
        if path is not None and path.is_dir():
            if cfg["file_pattern"] is None:
                errors.append(
                    f"{where}.file_pattern: required when 'path' is a folder."
                )
            elif not any(path.glob(cfg["file_pattern"])):
                errors.append(
                    f"{where}.file_pattern: no file in '{path}' matches "
                    f"'{cfg['file_pattern']}'."
                )

        var_name = _get_str(spec, "var_name", where, errors)
        if var_name is None and "var_name" not in spec:
            errors.append(f"{where}.var_name: the netCDF variable name is required.")
        cfg["var_name"] = var_name

        data_crs = spec.get("data_crs")
        if data_crs is not None and (
            isinstance(data_crs, bool) or not isinstance(data_crs, int)
        ):
            errors.append(
                f"{where}.data_crs: expected an EPSG integer, got {data_crs!r}."
            )
            data_crs = None
        cfg["data_crs"] = data_crs

        for dim in ("dim_time", "dim_x", "dim_y"):
            cfg[dim] = _get_str(spec, dim, where, errors)

        apply_data_gradient = spec.get("apply_data_gradient", False)
        if not isinstance(apply_data_gradient, bool):
            errors.append(f"{where}.apply_data_gradient: expected true or false.")
            apply_data_gradient = False
        cfg["apply_data_gradient"] = apply_data_gradient

        gradient_type = _get_str(spec, "gradient_type", where, errors)
        if gradient_type is not None and gradient_type not in (
            "additive",
            "multiplicative",
        ):
            errors.append(
                f"{where}.gradient_type: expected 'additive' or 'multiplicative', "
                f"got '{gradient_type}'."
            )
            gradient_type = None
        cfg["gradient_type"] = gradient_type

        out[str(variable)] = cfg
    return out


def _validate_forcing(config: dict, base: Path, errors: list[str]) -> dict:
    out: dict[str, Any] = {
        "station": None,
        "gridded": {},
        "pet_method": "Oudin",
        "pet_lat": None,
        "variables": set(),
    }
    section = _get_mapping(config, "forcing", errors, required=True)
    if section is None:
        return out
    valid = {
        "file",
        "time",
        "columns",
        "ref_elevation",
        "temperature",
        "precipitation",
        "pet",
        "gridded",
    }
    _check_keys(section, valid, "forcing", errors)

    has_station = "file" in section or "columns" in section
    if has_station:
        out["station"] = _validate_station_forcing(section, base, errors)
    out["gridded"] = _validate_gridded_forcing(section.get("gridded"), base, errors)

    if not has_station and not out["gridded"]:
        errors.append(
            "forcing: provide a station CSV ('file' and 'columns') and/or a "
            "'gridded' section with netCDF sources."
        )

    pet = section.get("pet", {}) or {}
    if not isinstance(pet, dict):
        errors.append("forcing.pet: expected a mapping (method, lat).")
        pet = {}
    _check_keys(pet, {"method", "lat"}, "forcing.pet", errors)
    out["pet_method"] = _get_str(pet, "method", "forcing.pet", errors) or "Oudin"
    out["pet_lat"] = _get_number(pet, "lat", "forcing.pet", errors)

    station_vars = set((out["station"] or {}).get("columns", {}))
    gridded_vars = set(out["gridded"])
    for variable in sorted(station_vars & gridded_vars):
        errors.append(
            f"forcing: '{variable}' is defined both in 'columns' (station) and "
            "in 'gridded'; pick one source per variable."
        )
    out["variables"] = station_vars | gridded_vars

    if out["variables"] and "precipitation" not in out["variables"]:
        errors.append(
            "forcing: a 'precipitation' source is required (in 'columns' or "
            "'gridded')."
        )
    if out["variables"] and "pet" not in out["variables"]:
        if "temperature" not in out["variables"]:
            errors.append(
                "forcing: a 'temperature' source is required to compute the "
                "PET, since no 'pet' source is given."
            )
    return out


def _validate_observations(config: dict, base: Path, errors: list[str]) -> dict | None:
    section = _get_mapping(config, "observations", errors, required=False)
    if section is None:
        return None
    _check_keys(section, {"file", "time", "column"}, "observations", errors)

    out: dict[str, Any] = {}
    file = _resolve_file(section, base, "observations", errors)
    out["file"] = file
    out["time_column"], out["time_format"] = _validate_time_section(
        section, "observations", errors
    )
    column = _get_str(section, "column", "observations", errors)
    if column is None and "column" not in section:
        errors.append("observations.column: the discharge column name is required.")
    out["column"] = column

    if file is not None and column is not None:
        available = _csv_columns(file, "observations", errors)
        needed = {
            "observations.time.column": out["time_column"],
            "observations.column": column,
        }
        _check_columns(needed, available, file, errors)
    return out


def _validate_periods(config: dict, errors: list[str]) -> Periods | None:
    section = config.get("periods")
    if section is None:
        errors.append(
            "periods: this section is required (declare at least a "
            "'simulation: [start, end]' period)."
        )
        return None

    if isinstance(section, (list, tuple)) and len(section) == 2:
        section = {"simulation": list(section)}
    if not isinstance(section, dict):
        errors.append(
            "periods: expected a mapping (calibration/validation/simulation/"
            "spinup) or a [start, end] pair."
        )
        return None
    valid = {"calibration", "validation", "simulation", "spinup"}
    _check_keys(section, valid, "periods", errors)
    kwargs = {key: section[key] for key in valid & set(section)}
    if "spinup" not in kwargs:
        kwargs["spinup"] = 0
    try:
        return Periods(**kwargs)
    except ConfigurationError as err:
        errors.append(f"periods: {err.args[0]}")
        return None


def _validate_parameters(config: dict, errors: list[str]) -> dict:
    section = config.get("parameters")
    if section is None:
        return {}
    if not isinstance(section, dict):
        errors.append("parameters: expected a mapping of parameter name to value.")
        return {}
    values = {}
    for name, value in section.items():
        if isinstance(value, bool) or not isinstance(value, numbers.Real):
            errors.append(f"parameters.{name}: expected a number, got {value!r}.")
            continue
        values[str(name)] = float(value)
    return values


def _validate_data_parameters(config: dict, errors: list[str]) -> dict:
    """Validate the data_parameters section (forcing 'param:' references)."""
    section = config.get("data_parameters")
    if section is None:
        return {}
    if not isinstance(section, dict):
        errors.append(
            "data_parameters: expected a mapping of parameter name to a value "
            "or a {value, min, max} mapping."
        )
        return {}
    out: dict[str, dict] = {}
    for name, spec in section.items():
        where = f"data_parameters.{name}"
        if not isinstance(spec, bool) and isinstance(spec, numbers.Real):
            out[str(name)] = {"value": float(spec), "min": None, "max": None}
            continue
        if not isinstance(spec, dict):
            errors.append(f"{where}: expected a number or a mapping (value, min, max).")
            continue
        _check_keys(spec, {"value", "min", "max"}, where, errors)
        value = _get_number(spec, "value", where, errors)
        if value is None and "value" not in spec:
            errors.append(f"{where}.value: a value is required.")
        out[str(name)] = {
            "value": value,
            "min": _get_number(spec, "min", where, errors),
            "max": _get_number(spec, "max", where, errors),
        }
    return out


def _validate_calibration(config: dict, errors: list[str]) -> dict | None:
    """Validate the calibration section (how to calibrate the parameters)."""
    section = config.get("calibration")
    if section is None:
        return None
    if not isinstance(section, dict):
        errors.append(
            "calibration: expected a mapping (algorithm, repetitions, "
            "objective, transform, parameters)."
        )
        return None
    valid = {"algorithm", "repetitions", "objective", "transform", "parameters"}
    _check_keys(section, valid, "calibration", errors)

    out = {
        "algorithm": section.get("algorithm", "sceua"),
        "repetitions": section.get("repetitions"),
        "objective": section.get("objective"),
        "transform": section.get("transform"),
        "parameters": section.get("parameters"),
    }
    if not isinstance(out["algorithm"], str):
        errors.append(
            f"calibration.algorithm: expected an algorithm name (e.g. 'sceua'), "
            f"got {out['algorithm']!r}."
        )
    reps = out["repetitions"]
    if reps is not None and (
        isinstance(reps, bool) or not isinstance(reps, int) or reps < 1
    ):
        errors.append(
            f"calibration.repetitions: expected a positive integer, got {reps!r}."
        )
    if out["objective"] is not None and not isinstance(out["objective"], str):
        errors.append(
            f"calibration.objective: expected a metric name (e.g. 'kge_2012'), "
            f"got {out['objective']!r}."
        )
    if out["transform"] is not None:
        from hydrobricks.evaluation.transforms import DischargeTransform

        try:
            DischargeTransform.from_spec(out["transform"])
        except (HydroBricksError, ValueError, TypeError) as err:
            message = err.args[0] if getattr(err, "args", None) else str(err)
            errors.append(f"calibration.transform: {message}")
    params = out["parameters"]
    if params is not None and (
        not isinstance(params, list) or not all(isinstance(p, str) for p in params)
    ):
        errors.append(
            "calibration.parameters: expected a list of parameter names to "
            "calibrate."
        )
        out["parameters"] = None
    return out


def _check_lateral_processes(cfg: dict, errors: list[str]) -> None:
    """A lateral process needs to know how the hydro units are connected.

    Without the connectivity table the process has no target unit and silently
    redistributes nothing, which is worse than an error: the run looks fine and
    the process is simply absent.
    """
    options = (cfg.get("model") or {}).get("options") or {}
    lateral = sorted(
        str(value)
        for value in options.values()
        if isinstance(value, str) and value.startswith("transport:")
    )
    if not lateral:
        return
    if (cfg.get("hydro_units") or {}).get("connectivity") is None:
        errors.append(
            f"hydro_units.connectivity: required by the lateral process(es) "
            f"{', '.join(lateral)}, which move water between hydro units. "
            "Compute the table once with Catchment.calculate_connectivity and "
            "declare it here (without it the process has no target unit and "
            "does nothing)."
        )


def _validate_cross_checks(cfg: dict, errors: list[str]) -> None:
    """Checks spanning several sections (gridded forcing vs hydro units, PET)."""
    hu = cfg["hydro_units"]
    fc = cfg["forcing"]
    _check_land_covers(cfg, errors)
    _check_lateral_processes(cfg, errors)
    gridded = fc.get("gridded") or {}
    has_catchment = hu.get("outline") is not None and hu.get("dem") is not None

    if (
        gridded
        and hu.get("unit_ids_raster") is None
        and hu.get("discretization") is None
    ):
        errors.append(
            "hydro_units.unit_ids_raster: required with gridded forcing (a "
            "raster of the hydro unit ids, used to aggregate the grid cells)."
        )
    for variable, spec in gridded.items():
        if spec.get("apply_data_gradient") and not has_catchment:
            errors.append(
                f"forcing.gridded.{variable}.apply_data_gradient: requires "
                "'outline' and 'dem' in the hydro_units section (a DEM is "
                "needed to derive the gradients from the data)."
            )

    variables = fc.get("variables") or set()
    if variables and "pet" not in variables:
        if fc.get("pet_lat") is None and not has_catchment:
            errors.append(
                "forcing.pet.lat: required to compute the PET, since "
                "there is no 'pet' forcing source (provide a PET source, the "
                "catchment latitude, or an outline/dem to derive it from)."
            )

    # Every 'param:' forcing reference needs a data_parameters entry.
    station = fc.get("station") or {}
    references = [
        value
        for value in (
            station.get("temperature_gradient"),
            station.get("precip_gradient"),
            station.get("precip_correction_factor"),
        )
        if isinstance(value, str)
    ]
    for reference in references:
        name = reference.split(":", 1)[1]
        if name not in cfg["data_parameters"]:
            errors.append(
                f"forcing: '{reference}' has no matching entry in the "
                "data_parameters section (define its value and range there)."
            )


def _check_land_covers(cfg: dict, errors: list[str]) -> None:
    """Check the land covers the model declares against the hydro units data.

    The covers are declared on the model (its ``land_cover_types`` /
    ``land_cover_names`` options) and their areas come either from the hydro units
    CSV ('columns_areas') or from a data source ('land_covers').
    """
    hu = cfg["hydro_units"]
    names = cfg["model"].get("land_cover_names") or []
    types = cfg["model"].get("land_cover_types") or []
    glacier_names = [name for name, kind in zip(names, types) if kind == "glacier"]
    glacier = (hu.get("land_covers") or {}).get("glacier")
    columns_areas = hu.get("columns_areas")

    if glacier is not None:
        if len(glacier_names) > 2:
            errors.append(
                "hydro_units.land_covers.glacier: a glacier extent initializes "
                "one glacier cover, or two split at the equilibrium line (ice "
                f"and firn); the model declares {len(glacier_names)}."
            )
        elif len(glacier_names) == 2 and glacier["ela"] is None:
            errors.append(
                "hydro_units.land_covers.glacier.ela: required to split the "
                "glacier extent between the two declared glacier covers "
                f"('{glacier_names[0]}' below the equilibrium line altitude, "
                f"'{glacier_names[1]}' above it)."
            )
    elif glacier_names and columns_areas is None:
        errors.append(
            "hydro_units.land_covers.glacier: the model declares the glacier "
            f"cover(s) {', '.join(glacier_names)}, but no glacier extent "
            "('outline' or 'ice_thickness') and no 'columns_areas' give their "
            "area."
        )

    if columns_areas is not None and names:
        unknown = [cover for cover in columns_areas if cover not in names]
        missing = [cover for cover in names if cover not in columns_areas]
        if unknown or missing:
            errors.append(
                "hydro_units.columns_areas: one area column per land cover of "
                f"the model is required ({', '.join(names)}); "
                + (f"unknown: {', '.join(unknown)}. " if unknown else "")
                + (f"missing: {', '.join(missing)}." if missing else "")
            )


_GLACIER_EVOLUTION_METHODS = {
    "delta_h": "ActionGlacierEvolutionDeltaH",
    "area_scaling": "ActionGlacierEvolutionAreaScaling",
}
_MONTH_NAMES = (
    "January",
    "February",
    "March",
    "April",
    "May",
    "June",
    "July",
    "August",
    "September",
    "October",
    "November",
    "December",
)


def _validate_actions(config: dict, base: Path, errors: list[str]) -> dict:
    """Validate the optional 'actions' section (dynamic changes during the run)."""
    section = config.get("actions")
    if section is None:
        return {}
    if not isinstance(section, dict):
        errors.append(
            "actions: expected a mapping of action name to its settings, e.g. "
            "{glacier_evolution: {method: delta_h, lookup_tables: <dir>}}."
        )
        return {}
    _check_keys(section, {"glacier_evolution"}, "actions", errors)

    out: dict[str, Any] = {}
    glacier_evolution = section.get("glacier_evolution")
    if glacier_evolution is not None:
        out["glacier_evolution"] = _validate_glacier_evolution(
            glacier_evolution, base, errors
        )
    return out


def _validate_glacier_evolution(spec: Any, base: Path, errors: list[str]) -> dict:
    """Validate 'actions.glacier_evolution' (a precomputed lookup table)."""
    where = "actions.glacier_evolution"
    out: dict[str, Any] = {
        "method": "delta_h",
        "lookup_tables": None,
        "land_cover": "glacier",
        "update_month": "October",
        "filename_area": "glacier_evolution_lookup_table_area.csv",
        "filename_volume": "glacier_evolution_lookup_table_volume.csv",
    }
    if not isinstance(spec, dict):
        errors.append(
            f"{where}: expected a mapping (method, lookup_tables, land_cover, "
            "update_month)."
        )
        return out
    _check_keys(spec, set(out), where, errors)

    method = _get_str(spec, "method", where, errors) or out["method"]
    if method not in _GLACIER_EVOLUTION_METHODS:
        errors.append(
            f"{where}.method: expected one of "
            f"{', '.join(sorted(_GLACIER_EVOLUTION_METHODS))}, got '{method}'."
        )
    out["method"] = method

    for key in ("filename_area", "filename_volume"):
        name = _get_str(spec, key, where, errors)
        if name is not None:
            out[key] = name

    if "lookup_tables" not in spec:
        errors.append(
            f"{where}.lookup_tables: the directory holding the precomputed "
            "lookup tables is required (produced by GlacierEvolutionDeltaH / "
            "GlacierEvolutionAreaScaling, see the preprocessing examples)."
        )
    else:
        tables = _resolve_optional_path(spec, "lookup_tables", base, where, errors)
        if tables is not None:
            if not tables.is_dir():
                errors.append(
                    f"{where}.lookup_tables: '{tables}' is not a directory (it "
                    "must hold the area and volume lookup table CSVs)."
                )
            else:
                for key in ("filename_area", "filename_volume"):
                    if not (tables / out[key]).is_file():
                        errors.append(
                            f"{where}.lookup_tables: '{out[key]}' is missing from "
                            f"'{tables}'."
                        )
            out["lookup_tables"] = tables

    land_cover = _get_str(spec, "land_cover", where, errors)
    if land_cover is not None:
        out["land_cover"] = land_cover

    month = spec.get("update_month")
    if month is not None:
        if isinstance(month, bool) or not isinstance(month, (str, int)):
            errors.append(
                f"{where}.update_month: expected a month name or number (1-12), "
                f"got {month!r}."
            )
        elif isinstance(month, int) and not 1 <= month <= 12:
            errors.append(
                f"{where}.update_month: expected a number in 1-12, got {month}."
            )
        elif isinstance(month, str) and month.capitalize() not in _MONTH_NAMES:
            errors.append(
                f"{where}.update_month: unknown month '{month}'; expected an "
                "English month name or a number in 1-12."
            )
        else:
            out["update_month"] = month

    return out


def _validate_config(config: dict, base: Path, errors: list[str]) -> dict:
    _check_keys(config, _TOP_LEVEL_KEYS, "project", errors)
    for key in _REQUIRED_TOP_LEVEL_KEYS:
        if key not in config:
            errors.append(f"{key}: this section is required.")

    cfg = {
        "model": _validate_model(config, base, errors),
        "hydro_units": _validate_hydro_units(config, base, errors),
        "forcing": _validate_forcing(config, base, errors),
        "observations": _validate_observations(config, base, errors),
        "periods": _validate_periods(config, errors),
        "parameters": _validate_parameters(config, errors),
        "data_parameters": _validate_data_parameters(config, errors),
        "actions": _validate_actions(config, base, errors),
        "calibration": _validate_calibration(config, errors),
    }
    _validate_cross_checks(cfg, errors)

    if cfg["calibration"] is not None:
        if cfg["observations"] is None:
            errors.append(
                "calibration: an 'observations' section is required to calibrate."
            )
        if cfg["periods"] is not None and cfg["periods"].calibration is None:
            errors.append(
                "calibration: declare a calibration period in the 'periods' " "section."
            )

    output = config.get("output")
    if output is not None and not isinstance(output, str):
        errors.append(f"output: expected a directory path, got {output!r}.")
        output = None
    out_dir = Path(output) if output is not None else Path("output")
    if not out_dir.is_absolute():
        out_dir = base / out_dir
    cfg["output"] = out_dir

    cache = config.get("cache")
    if cache is not None and not isinstance(cache, str):
        errors.append(f"cache: expected a directory path, got {cache!r}.")
        cache = None
    cache_dir = Path(cache) if cache is not None else out_dir / "cache"
    if not cache_dir.is_absolute():
        cache_dir = base / cache_dir
    cfg["cache"] = cache_dir

    cfg["base_dir"] = base
    return cfg


# --- Build ------------------------------------------------------------------


def _parameter_label(row: pd.Series) -> str:
    aliases = row["aliases"]
    if aliases:
        return aliases[0]
    component = row["component"]
    if isinstance(component, list):
        component = "/".join(component)
    return f"{component}:{row['name']}"


def _parameter_range(row: pd.Series) -> str:
    return f"[{row['min']}..{row['max']}]"


def _check_parameter_names(
    parameter_set: ParameterSet,
    values: dict | list,
    errors: list[str],
    section: str = "parameters",
) -> None:
    known: set[str] = set()
    labels = []
    for _, row in parameter_set.get_model_parameters().iterrows():
        known.add(f"{row['component']}:{row['name']}")
        for alias in row["aliases"] or []:
            known.add(alias)
        labels.append(f"{_parameter_label(row)} {_parameter_range(row)}")
    for name in values:
        if not parameter_set.has(name):
            errors.append(
                f"{section}.{name}: unknown parameter for this model"
                f"{_suggest(name, known)}. Model parameters: "
                f"{', '.join(labels)}."
            )


def _check_catchment_dependencies() -> None:
    """Fail early with the package list if a Catchment cannot be built."""
    from hydrobricks._optional import (
        HAS_GEOPANDAS,
        HAS_PYPROJ,
        HAS_RASTERIO,
        HAS_SHAPELY,
    )

    if not (HAS_GEOPANDAS and HAS_SHAPELY and HAS_RASTERIO and HAS_PYPROJ):
        raise DependencyError(
            "Using a catchment outline/DEM (delineation or gradients from "
            "gridded data) requires the optional packages geopandas, shapely, "
            "rasterio and pyproj.",
            operation="hydro_units.outline/dem",
            install_command="pip install geopandas shapely rasterio pyproj",
        )


def _check_gridded_dependencies() -> None:
    """Fail early with the package list if gridded forcing cannot be read."""
    from hydrobricks._optional import (
        HAS_NETCDF,
        HAS_RASTERIO,
        HAS_RIOXARRAY,
        HAS_XARRAY,
    )

    if not (HAS_XARRAY and HAS_RIOXARRAY and HAS_RASTERIO and HAS_NETCDF):
        raise DependencyError(
            "Gridded forcing requires the optional packages xarray, rioxarray, "
            "rasterio and netCDF4.",
            operation="forcing.gridded",
            install_command="pip install xarray rioxarray rasterio netCDF4",
        )


def _initialize_land_covers(catchment: Any, land_covers: dict, model: Model) -> None:
    """Set the land cover fractions of the hydro units from their data sources.

    A glacier extent (an outline or an ice-thickness raster) initializes the glacier
    cover of every hydro unit, split at the equilibrium line altitude when the model
    declares two glacier covers (ice below, firn above). A model without a glacier
    cover ignores the source: its glacierized area stays in the soil cover, so the
    same catchment description serves every model of a comparison.
    """
    glacier = (land_covers or {}).get("glacier")
    if glacier is None:
        return

    glacier_covers = [
        name
        for name, kind in zip(model.land_cover_names, model.land_cover_types)
        if kind == "glacier"
    ]
    if not glacier_covers:
        logger.warning(
            "The model declares no glacier land cover: the glacier extent of "
            "hydro_units.land_covers is ignored (the glacierized area stays in "
            "the soil cover)."
        )
        return

    from hydrobricks.preprocessing import (
        initialize_glacier_cover_from_extent,
        initialize_glacier_covers_split_by_elevation,
    )

    if glacier["outline"] is not None:
        source: dict[str, Any] = {"glacier_outline": glacier["outline"]}
    else:
        source = {"ice_thickness": glacier["ice_thickness"]}

    if len(glacier_covers) == 1:
        initialize_glacier_cover_from_extent(
            catchment, land_cover=glacier_covers[0], **source
        )
    else:
        # The validation guarantees the ELA and at most two glacier covers here.
        initialize_glacier_covers_split_by_elevation(
            catchment,
            ela=glacier["ela"],
            ice_cover=glacier_covers[0],
            firn_cover=glacier_covers[1],
            **source,
        )


def _build_project(
    cfg: dict, config: dict, path: Path | None, errors: list[str], setup: bool
) -> Project:
    periods: Periods = cfg["periods"]
    start_date, end_date = periods.full_span.bounds

    cfg["output"].mkdir(parents=True, exist_ok=True)

    # Model: a pre-built model by name, or a custom structure. It is built first
    # because its land covers are the ones the hydro units carry.
    try:
        if "structure" in cfg["model"]:
            from hydrobricks.models.custom import CustomModel

            model = CustomModel(cfg["model"]["structure"], **cfg["model"]["options"])
        else:
            model = cfg["model"]["class"](**cfg["model"]["options"])
    except (TypeError, RuntimeError, HydroBricksError) as err:
        # The message is args[0]; str(err) would render the whole args tuple.
        message = err.args[0] if getattr(err, "args", None) else str(err)
        errors.append(f"model: {message}")
        _raise_if_errors(errors, path)
        raise AssertionError("unreachable")  # pragma: no cover

    # Hydro units: loaded from a CSV or delineated from the DEM, optionally
    # within a Catchment (needed for the delineation and to derive elevation
    # gradients from gridded data).
    hu_cfg = cfg["hydro_units"]
    catchment = None
    if hu_cfg["outline"] is not None and hu_cfg["dem"] is not None:
        _check_catchment_dependencies()
        from hydrobricks.catchment import Catchment

        catchment = Catchment(
            hu_cfg["outline"],
            land_cover_types=list(model.land_cover_types),
            land_cover_names=list(model.land_cover_names),
        )
        catchment.extract_dem(hu_cfg["dem"])

    unit_ids_raster = hu_cfg["unit_ids_raster"]
    if hu_cfg["discretization"] is not None:
        # The validation guarantees the catchment here.
        discretization = hu_cfg["discretization"]
        kwargs: dict[str, Any] = {"method": discretization["method"]}
        if discretization["distance"] is not None:
            kwargs["distance"] = discretization["distance"]
        if discretization["number"] is not None:
            kwargs["number"] = int(discretization["number"])
        for key in ("min_elevation", "max_elevation"):
            if discretization[key] is not None:
                kwargs[key] = discretization[key]
        for key in ("split_discontinuous", "min_patch_area", "connectivity"):
            if discretization[key] is not None:
                kwargs[key] = discretization[key]
        catchment.create_elevation_bands(**kwargs)
        _initialize_land_covers(catchment, hu_cfg["land_covers"], model)
        hydro_units = catchment.hydro_units
        if cfg["forcing"]["gridded"]:
            # The gridded aggregation needs the unit ids as a raster.
            catchment.save_unit_ids_raster(cfg["output"])
            unit_ids_raster = cfg["output"] / "unit_ids.tif"
    else:
        hydro_units = (
            catchment.hydro_units
            if catchment
            else HydroUnits(
                land_cover_types=list(model.land_cover_types),
                land_cover_names=list(model.land_cover_names),
            )
        )
        if hu_cfg["columns_areas"] is not None:
            hydro_units.load_from_csv(
                hu_cfg["file"],
                column_elevation=hu_cfg["column_elevation"],
                columns_areas=hu_cfg["columns_areas"],
                other_columns=hu_cfg["other_columns"] or None,
            )
        else:
            hydro_units.load_from_csv(
                hu_cfg["file"],
                column_elevation=hu_cfg["column_elevation"],
                column_area=hu_cfg["column_area"],
                other_columns=hu_cfg["other_columns"] or None,
            )
        if catchment is not None and unit_ids_raster is not None:
            catchment.load_unit_ids_from_raster(str(unit_ids_raster))
            _initialize_land_covers(catchment, hu_cfg["land_covers"], model)

    # Lateral connectivity between the hydro units (needed by the lateral
    # processes, e.g. the snow redistribution). Set last: re-populating the basin
    # settings clears the connections.
    if hu_cfg["connectivity"] is not None:
        hydro_units.set_connectivity(hu_cfg["connectivity"])

    # Forcing: station CSV and/or gridded netCDF sources. Expensive gridded
    # spatializations are cached under <output>/cache (created on first write)
    # and reloaded when the same setup is reused. Note: in the discretization
    # path, unit_ids.tif is regenerated into the output dir on each load, but
    # the cache key hashes the raster's bytes (deterministic for identical
    # inputs), not its mtime, so the cache is not invalidated.
    fc = cfg["forcing"]
    station = fc["station"]
    forcing = Forcing(
        catchment if catchment is not None else hydro_units,
        cache_dir=cfg["cache"],
    )

    if station is not None:
        forcing.load_station_data_from_csv(
            station["file"],
            column_time=station["time_column"],
            time_format=station["time_format"],
            content=dict(station["columns"]),
        )

        # The station data must cover the simulation span (gridded sources are
        # read lazily, so they are checked at run time).
        time = pd.DatetimeIndex(forcing.data1D.time)
        if len(time) > 0 and (
            periods.full_span.start < time[0] or periods.full_span.end > time[-1]
        ):
            errors.append(
                f"periods: the simulation span ({start_date}..{end_date}) is "
                f"not covered by the forcing data ({time[0].date()}.."
                f"{time[-1].date()})."
            )

        if "temperature" in station["columns"]:
            forcing.spatialize_from_station_data(
                variable="temperature",
                method="additive_elevation_gradient",
                ref_elevation=station["ref_elevation"],
                gradient=station["temperature_gradient"],
            )
        if "precipitation" in station["columns"]:
            if station["precip_correction_factor"] is not None:
                forcing.correct_station_data(
                    variable="precipitation",
                    method="multiplicative",
                    correction_factor=station["precip_correction_factor"],
                )
            if station["precip_gradient"] is not None:
                forcing.spatialize_from_station_data(
                    variable="precipitation",
                    method="multiplicative_elevation_gradient",
                    ref_elevation=station["ref_elevation"],
                    gradient=station["precip_gradient"],
                )
            else:
                forcing.spatialize_from_station_data(
                    variable="precipitation", method="constant"
                )
        if "pet" in station["columns"]:
            forcing.spatialize_from_station_data(variable="pet", method="constant")
        # Any further variables (e.g. solar radiation) are used as constant
        # fields.
        for variable in station["columns"]:
            if variable not in ("precipitation", "temperature", "pet"):
                forcing.spatialize_from_station_data(
                    variable=variable, method="constant"
                )

    if fc["gridded"]:
        _check_gridded_dependencies()
        for variable, spec in fc["gridded"].items():
            forcing.spatialize_from_gridded_data(
                variable=variable,
                path=spec["path"],
                file_pattern=spec["file_pattern"],
                data_crs=spec["data_crs"],
                var_name=spec["var_name"],
                dim_time=spec["dim_time"],
                dim_x=spec["dim_x"],
                dim_y=spec["dim_y"],
                raster_hydro_units=unit_ids_raster,
                apply_data_gradient=spec["apply_data_gradient"],
                gradient_type=spec["gradient_type"],
            )

    if "pet" not in fc["variables"]:
        pet_lat = fc["pet_lat"]
        if pet_lat is None and not hydro_units.has("latitude"):
            # The PET needs a latitude: the per-unit one when the hydro units
            # carry it (delineation, or a CSV holding the column), the catchment
            # mean otherwise. The validation guarantees a catchment here.
            pet_lat = float(catchment.extract_unit_mean_lat_lon(catchment.dem_data)[0])
        forcing.compute_pet(method=fc["pet_method"], use=["t", "lat"], lat=pet_lat)

    parameter_set = model.generate_parameters()
    for name, spec in cfg["data_parameters"].items():
        parameter_set.add_data_parameter(
            name, spec["value"], min_val=spec["min"], max_val=spec["max"]
        )
    _check_parameter_names(parameter_set, cfg["parameters"], errors)
    calibration = cfg["calibration"]
    if calibration is not None and calibration["parameters"]:
        _check_parameter_names(
            parameter_set,
            calibration["parameters"],
            errors,
            section="calibration.parameters",
        )
    _raise_if_errors(errors, path)
    if cfg["parameters"]:
        parameter_set.set_values(cfg["parameters"])

    if setup:
        model.setup(
            spatial_structure=hydro_units,
            output_path=str(cfg["output"]),
            start_date=start_date,
            end_date=end_date,
            spinup=periods.spinup,
        )

    # Optional observed discharge, over the full simulation span.
    observations = None
    obs_cfg = cfg["observations"]
    if obs_cfg is not None:
        observations = DischargeObservations(start_date, end_date)
        observations.load_from_csv(
            obs_cfg["file"],
            column_time=obs_cfg["time_column"],
            time_format=obs_cfg["time_format"],
            content={"discharge": obs_cfg["column"]},
        )

    actions = _build_actions(cfg["actions"], model, errors)
    _raise_if_errors(errors, path)
    if setup:
        for action in actions:
            model.add_action(action)

    return Project(
        model=model,
        forcing=forcing,
        parameters=parameter_set,
        observations=observations,
        periods=periods,
        config=config,
        path=path,
        output_dir=cfg["output"],
        hydro_units=hydro_units,
        catchment=catchment,
        calibration=calibration,
        base_dir=cfg["base_dir"],
        actions=actions,
    )


def _build_actions(cfg_actions: dict, model: Model, errors: list[str]) -> list:
    """Build the action objects declared in the 'actions' section.

    The actions are only registered on a model that is set up, so they are
    returned here and attached by :meth:`Project.setup` (and by
    :func:`load_project` when it sets the model up itself).
    """
    actions: list = []
    glacier_evolution = (cfg_actions or {}).get("glacier_evolution")
    if glacier_evolution is None:
        return actions

    if not any(kind == "glacier" for kind in model.land_cover_types):
        errors.append(
            "actions.glacier_evolution: the model declares no glacier land "
            "cover; add one (model.options.land_cover_types) or drop the action."
        )
        return actions
    if model.options.get("glacier_infinite_storage"):
        errors.append(
            "actions.glacier_evolution: the glacier evolution tracks the ice "
            "volume, which an infinite ice storage cannot hold; set "
            "model.options.glacier_infinite_storage to false."
        )
        return actions
    if glacier_evolution["land_cover"] not in model.land_cover_names:
        errors.append(
            f"actions.glacier_evolution.land_cover: unknown land cover "
            f"'{glacier_evolution['land_cover']}'; the model declares "
            f"{', '.join(model.land_cover_names)}."
        )
        return actions

    import hydrobricks.actions as hb_actions

    action_class = getattr(
        hb_actions, _GLACIER_EVOLUTION_METHODS[glacier_evolution["method"]]
    )
    action = action_class()
    action.load_from_csv(
        glacier_evolution["lookup_tables"],
        land_cover=glacier_evolution["land_cover"],
        filename_area=glacier_evolution["filename_area"],
        filename_volume=glacier_evolution["filename_volume"],
        update_month=glacier_evolution["update_month"],
    )
    actions.append(action)

    return actions
