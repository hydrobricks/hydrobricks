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
      pet: {method: Oudin, latitude: 47.3}

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
          crs: 2056
          dim_x: E
          dim_y: N

The model can also be a custom structure declared as data (see
:class:`~hydrobricks.models.custom.CustomModel`): use
``model: {structure: my_structure.yaml}`` instead of a pre-built ``name``.

Relative paths are resolved against the project file location. For anything
beyond these canonical cases (glacier evolution, custom calibration logic)
use the Python API, starting from the objects returned by
:func:`load_project`.
"""

from __future__ import annotations

import difflib
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

_TOP_LEVEL_KEYS = {
    "model",
    "hydro_units",
    "forcing",
    "observations",
    "periods",
    "parameters",
    "data_parameters",
    "output",
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
    return out


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
    valid = {"method", "distance", "number", "min_elevation", "max_elevation"}
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
    return out


def _validate_hydro_units(config: dict, base: Path, errors: list[str]) -> dict:
    out: dict[str, Any] = {}
    section = _get_mapping(config, "hydro_units", errors, required=True)
    if section is None:
        return out
    valid = {
        "file",
        "columns",
        "land_cover_areas",
        "unit_ids_raster",
        "outline",
        "dem",
        "discretization",
    }
    _check_keys(section, valid, "hydro_units", errors)

    out["discretization"] = _validate_discretization(section, errors)

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
        for key in ("columns", "land_cover_areas"):
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
    out["outline"] = _resolve_optional_path(
        section, "outline", base, "hydro_units", errors
    )
    out["dem"] = _resolve_optional_path(section, "dem", base, "hydro_units", errors)
    if ("outline" in section) != ("dem" in section):
        errors.append(
            "hydro_units: 'outline' and 'dem' must be provided together (they "
            "define the catchment used to derive elevation gradients)."
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

    land_cover_areas = section.get("land_cover_areas")
    if land_cover_areas is not None and not isinstance(land_cover_areas, dict):
        errors.append(
            "hydro_units.land_cover_areas: expected a mapping of land cover "
            "name to area column."
        )
        land_cover_areas = None
    out["land_cover_areas"] = land_cover_areas

    if file is not None:
        available = _csv_columns(file, "hydro_units", errors)
        needed = {"hydro_units.columns.elevation": out["column_elevation"]}
        if land_cover_areas is None:
            needed["hydro_units.columns.area"] = out["column_area"]
        else:
            # With per-land-cover areas the total area column is not used.
            for name, col in land_cover_areas.items():
                needed[f"hydro_units.land_cover_areas.{name}"] = col
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
        "crs",
        "dim_time",
        "dim_x",
        "dim_y",
        "elevation_gradient",
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

        crs = spec.get("crs")
        if crs is not None and (isinstance(crs, bool) or not isinstance(crs, int)):
            errors.append(f"{where}.crs: expected an EPSG integer, got {crs!r}.")
            crs = None
        cfg["crs"] = crs

        for dim in ("dim_time", "dim_x", "dim_y"):
            cfg[dim] = _get_str(spec, dim, where, errors)

        elevation_gradient = spec.get("elevation_gradient", False)
        if not isinstance(elevation_gradient, bool):
            errors.append(f"{where}.elevation_gradient: expected true or false.")
            elevation_gradient = False
        cfg["elevation_gradient"] = elevation_gradient

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
        "pet_latitude": None,
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
        errors.append("forcing.pet: expected a mapping (method, latitude).")
        pet = {}
    _check_keys(pet, {"method", "latitude"}, "forcing.pet", errors)
    out["pet_method"] = _get_str(pet, "method", "forcing.pet", errors) or "Oudin"
    out["pet_latitude"] = _get_number(pet, "latitude", "forcing.pet", errors)

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


def _validate_cross_checks(cfg: dict, errors: list[str]) -> None:
    """Checks spanning several sections (gridded forcing vs hydro units, PET)."""
    hu = cfg["hydro_units"]
    fc = cfg["forcing"]
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
        if spec.get("elevation_gradient") and not has_catchment:
            errors.append(
                f"forcing.gridded.{variable}.elevation_gradient: requires "
                "'outline' and 'dem' in the hydro_units section (a DEM is "
                "needed to derive the gradients from the data)."
            )

    variables = fc.get("variables") or set()
    if variables and "pet" not in variables:
        if fc.get("pet_latitude") is None and not has_catchment:
            errors.append(
                "forcing.pet.latitude: required to compute the PET, since "
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
    }
    _validate_cross_checks(cfg, errors)

    output = config.get("output")
    if output is not None and not isinstance(output, str):
        errors.append(f"output: expected a directory path, got {output!r}.")
        output = None
    out_dir = Path(output) if output is not None else Path("output")
    if not out_dir.is_absolute():
        out_dir = base / out_dir
    cfg["output"] = out_dir
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
    parameter_set: ParameterSet, values: dict, errors: list[str]
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
                f"parameters.{name}: unknown parameter for this model"
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


def _build_project(
    cfg: dict, config: dict, path: Path | None, errors: list[str], setup: bool
) -> Project:
    periods: Periods = cfg["periods"]
    start_date, end_date = periods.full_span.bounds

    cfg["output"].mkdir(parents=True, exist_ok=True)

    # Hydro units: loaded from a CSV or delineated from the DEM, optionally
    # within a Catchment (needed for the delineation and to derive elevation
    # gradients from gridded data).
    hu_cfg = cfg["hydro_units"]
    catchment = None
    if hu_cfg["outline"] is not None and hu_cfg["dem"] is not None:
        _check_catchment_dependencies()
        from hydrobricks.catchment import Catchment

        catchment = Catchment(hu_cfg["outline"])
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
        catchment.create_elevation_bands(**kwargs)
        hydro_units = catchment.hydro_units
        if cfg["forcing"]["gridded"]:
            # The gridded aggregation needs the unit ids as a raster.
            catchment.save_unit_ids_raster(cfg["output"])
            unit_ids_raster = cfg["output"] / "unit_ids.tif"
    else:
        hydro_units = catchment.hydro_units if catchment else HydroUnits()
        if hu_cfg["land_cover_areas"] is not None:
            hydro_units.load_from_csv(
                hu_cfg["file"],
                column_elevation=hu_cfg["column_elevation"],
                columns_areas=hu_cfg["land_cover_areas"],
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
        cache_dir=cfg["output"] / "cache",
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
                data_crs=spec["crs"],
                var_name=spec["var_name"],
                dim_time=spec["dim_time"],
                dim_x=spec["dim_x"],
                dim_y=spec["dim_y"],
                raster_hydro_units=unit_ids_raster,
                apply_data_gradient=spec["elevation_gradient"],
                gradient_type=spec["gradient_type"],
            )

    if "pet" not in fc["variables"]:
        forcing.compute_pet(
            method=fc["pet_method"], use=["t", "lat"], lat=fc["pet_latitude"]
        )

    # Model and parameters: a pre-built model by name, or a custom structure.
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

    parameter_set = model.generate_parameters()
    for name, spec in cfg["data_parameters"].items():
        parameter_set.add_data_parameter(
            name, spec["value"], min_val=spec["min"], max_val=spec["max"]
        )
    _check_parameter_names(parameter_set, cfg["parameters"], errors)
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
    )
