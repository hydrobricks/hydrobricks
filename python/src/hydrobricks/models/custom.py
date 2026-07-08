"""A model whose full structure is declared as data (a YAML file or a dict).

:class:`CustomModel` builds the same kind of structure the pre-built model
classes define in code — bricks (stores and land covers), their processes, and
the fluxes routing the water between bricks or to the outlet — from a
declarative *structure definition*. The parameters are derived from the
declared process kinds exactly as for the pre-built models
(``generate_parameters()``), so a custom structure is calibratable out of the
box.

Example structure file (a simplified SOCONT-like model)::

    name: my_model

    options:
      snow_melt_process: melt:degree_day

    bricks:
      open:                       # a land cover (must match land_cover_names)
        kind: land_cover
        processes:
          infiltration: {kind: infiltration:socont, target: slow_reservoir}
          runoff: {kind: outflow:rest, target: surface_runoff}
      slow_reservoir:
        kind: storage             # attached to each hydro unit by default
        parameters: {capacity: 200}
        processes:
          et: {kind: et:socont}   # ET needs no target (to the atmosphere)
          outflow: {kind: outflow:linear, target: outlet}
          overflow: {kind: overflow, target: outlet}
      surface_runoff:
        kind: storage
        processes:
          runoff: {kind: outflow:linear, target: outlet}

    aliases:
      slow_reservoir:capacity: A
      slow_reservoir:response_factor: k_slow
      surface_runoff:response_factor: k_quick

    constraints:
      - [k_slow, "<", k_quick]

The precipitation splitters (rain/snow) and the per-cover snowpacks are
generated from the ``options`` (the same snow options every model supports:
``with_snow``, ``snow_melt_process``, ``snow_redistribution``, ...), so the
declared bricks start where the snow routine ends. A process routes its output
with ``target`` (a declared brick, a generated one such as
``<cover>_snowpack``, or ``outlet``) or fans out with ``targets`` (a list);
``log: true`` records the process output, and ``instantaneous: true`` makes
the flux instantaneous. ``gate: <brick>`` names a brick whose state modulates
the process rate without receiving its flux (required by state-gated processes
such as ``percolation:prevah``). Bricks attach to each hydro unit by default;
use ``attach_to: sub_basin`` for catchment-level stores.
"""

from __future__ import annotations

import copy
import difflib
import numbers
from pathlib import Path
from typing import Any

import yaml

from hydrobricks._exceptions import ConfigurationError, ModelError
from hydrobricks.models.model import Model

_SPEC_KEYS = {
    "name",
    "allowed_land_cover_types",
    "options",
    "bricks",
    "aliases",
    "constraints",
}
_SNOW_OPTION_DEFAULTS: dict[str, Any] = {
    "with_snow": True,
    "snow_melt_process": "melt:degree_day",
    "snow_ice_transformation": None,
    "snow_redistribution": None,
    "snow_water_retention_process": None,
    "snow_refreezing_process": None,
    "snow_sublimation_process": None,
    "rain_to_snowpack": False,
    "forest_interception": False,
}
_BRICK_KEYS = {"kind", "attach_to", "computed_directly", "parameters", "processes"}
_PROCESS_KEYS = {"kind", "target", "targets", "log", "instantaneous", "gate"}
_CONSTRAINT_OPERATORS = ("<", "<=", ">", ">=")
_TARGETLESS_PREFIXES = ("et:", "interception:", "sublimation:")


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


def _load_structure_spec(structure: str | Path | dict) -> dict:
    """Load the structure definition from a YAML file path or a dict."""
    if isinstance(structure, dict):
        return copy.deepcopy(structure)
    if isinstance(structure, (str, Path)):
        path = Path(structure)
        if not path.is_file():
            raise ConfigurationError(
                f"The structure file '{path}' does not exist.",
                item_name="structure",
                item_value=str(path),
                reason="File not found",
            )
        try:
            with open(path, encoding="utf-8") as f:
                spec = yaml.safe_load(f)
        except yaml.YAMLError as err:
            raise ConfigurationError(
                f"The structure file '{path}' is not valid YAML: {err}",
                item_name="structure",
                reason="Invalid YAML",
            ) from None
        if not isinstance(spec, dict):
            raise ConfigurationError(
                f"The structure file '{path}' must contain a mapping "
                "(bricks, options, ...).",
                item_name="structure",
                reason="Not a mapping",
            )
        return spec
    raise ConfigurationError(
        "The structure must be a path to a YAML file or a dict, not "
        f"{type(structure).__name__}.",
        item_name="structure",
        reason="Invalid type",
    )


def _validate_process(process: Any, where: str, errors: list[str]) -> None:
    if not isinstance(process, dict):
        errors.append(f"{where}: expected a mapping (kind, target, ...).")
        return
    _check_keys(process, _PROCESS_KEYS, where, errors)
    kind = process.get("kind")
    if not isinstance(kind, str):
        errors.append(f"{where}.kind: the process kind is required (a string).")
        kind = ""
    if "target" in process and "targets" in process:
        errors.append(f"{where}: 'target' and 'targets' are mutually exclusive.")
    if "target" in process and not isinstance(process["target"], str):
        errors.append(f"{where}.target: expected a brick name or 'outlet'.")
    if "targets" in process and (
        not isinstance(process["targets"], list)
        or not all(isinstance(t, str) for t in process["targets"])
    ):
        errors.append(f"{where}.targets: expected a list of brick names.")
    if (
        "target" not in process
        and "targets" not in process
        and not kind.startswith(_TARGETLESS_PREFIXES)
    ):
        errors.append(
            f"{where}: a 'target' is required (a declared brick, a generated "
            "one such as '<cover>_snowpack', or 'outlet')."
        )
    for flag in ("log", "instantaneous"):
        if flag in process and not isinstance(process[flag], bool):
            errors.append(f"{where}.{flag}: expected true or false.")
    if "gate" in process and not isinstance(process["gate"], str):
        errors.append(f"{where}.gate: expected a brick name.")


def _validate_brick(brick: Any, where: str, errors: list[str]) -> None:
    if not isinstance(brick, dict):
        errors.append(f"{where}: expected a mapping (kind, processes, ...).")
        return
    _check_keys(brick, _BRICK_KEYS, where, errors)
    kind = brick.get("kind")
    if not isinstance(kind, str):
        errors.append(
            f"{where}.kind: the brick kind is required (e.g. 'storage' or "
            "'land_cover')."
        )
        kind = ""
    if kind == "land_cover":
        brick.setdefault("attach_to", "hydro_unit")
    else:
        attach_to = brick.setdefault("attach_to", "hydro_unit")
        if attach_to not in ("hydro_unit", "sub_basin"):
            errors.append(
                f"{where}.attach_to: expected 'hydro_unit' or 'sub_basin', "
                f"got {attach_to!r}."
            )
    if "computed_directly" in brick and not isinstance(
        brick["computed_directly"], bool
    ):
        errors.append(f"{where}.computed_directly: expected true or false.")
    parameters = brick.get("parameters", {})
    if not isinstance(parameters, dict):
        errors.append(f"{where}.parameters: expected a mapping of name to value.")
    else:
        for name, value in parameters.items():
            if isinstance(value, bool) or not isinstance(value, numbers.Real):
                errors.append(
                    f"{where}.parameters.{name}: expected a number, got {value!r}."
                )
    processes = brick.get("processes", {})
    if not isinstance(processes, dict):
        errors.append(f"{where}.processes: expected a mapping of process name.")
    else:
        for name, process in processes.items():
            _validate_process(process, f"{where}.processes.{name}", errors)


def _validate_structure_spec(spec: dict, errors: list[str]) -> None:
    _check_keys(spec, _SPEC_KEYS, "structure", errors)

    if "name" in spec and not isinstance(spec["name"], str):
        errors.append("structure.name: expected a string.")

    covers = spec.get("allowed_land_cover_types")
    if covers is not None and (
        not isinstance(covers, list) or not all(isinstance(c, str) for c in covers)
    ):
        errors.append(
            "structure.allowed_land_cover_types: expected a list of land "
            "cover types."
        )

    options = spec.get("options", {}) or {}
    if not isinstance(options, dict):
        errors.append("structure.options: expected a mapping.")
    else:
        _check_keys(options, set(_SNOW_OPTION_DEFAULTS), "structure.options", errors)

    bricks = spec.get("bricks")
    if not isinstance(bricks, dict) or not bricks:
        errors.append(
            "structure.bricks: a non-empty mapping of brick name to its "
            "definition is required."
        )
    else:
        for key, brick in bricks.items():
            _validate_brick(brick, f"structure.bricks.{key}", errors)

    aliases = spec.get("aliases", {}) or {}
    if not isinstance(aliases, dict):
        errors.append("structure.aliases: expected a mapping of parameter to alias.")
    else:
        for name, alias in aliases.items():
            if not isinstance(alias, str) and not (
                isinstance(alias, list) and all(isinstance(a, str) for a in alias)
            ):
                errors.append(
                    f"structure.aliases.{name}: expected an alias (string) or "
                    "a list of aliases."
                )

    constraints = spec.get("constraints", []) or []
    if not isinstance(constraints, list):
        errors.append(
            "structure.constraints: expected a list of [parameter, operator, "
            "parameter] triplets."
        )
    else:
        for i, constraint in enumerate(constraints):
            where = f"structure.constraints[{i}]"
            if (
                not isinstance(constraint, (list, tuple))
                or len(constraint) != 3
                or not all(isinstance(c, str) for c in constraint)
            ):
                errors.append(
                    f"{where}: expected a [parameter, operator, parameter] "
                    "triplet, e.g. [k_slow, '<', k_quick]."
                )
            elif constraint[1] not in _CONSTRAINT_OPERATORS:
                errors.append(
                    f"{where}: unknown operator '{constraint[1]}' (expected "
                    f"one of {', '.join(_CONSTRAINT_OPERATORS)})."
                )


class CustomModel(Model):
    """A model whose structure is declared as data (YAML file or dict).

    See the module docstring for the structure definition schema. The snow
    routine and the precipitation splitters are generated from the declared
    ``options`` (overridable as keyword arguments, like for any model), and
    the parameters are derived from the declared process kinds — so
    ``generate_parameters()``, calibration and project files work as for the
    pre-built models.

    Parameters
    ----------
    structure
        Path to a YAML structure file, or an equivalent (already parsed) dict.
    name
        Optional model name (defaults to the structure's ``name``, else
        ``'custom'``).
    **kwargs
        The usual model keyword arguments (``solver``, ``record_all``,
        ``land_cover_types``, ``land_cover_names``) plus any snow option
        declared in the structure's ``options``.
    """

    # Not instantiable by name alone (a structure is required), so it is kept
    # out of the by-name model registry used by project files and the CLI.
    registrable = False

    def __init__(
        self,
        structure: str | Path | dict,
        name: str | None = None,
        **kwargs: Any,
    ) -> None:
        spec = _load_structure_spec(structure)
        errors: list[str] = []
        _validate_structure_spec(spec, errors)
        if errors:
            plural = "s" if len(errors) > 1 else ""
            raise ConfigurationError(
                f"The structure definition has {len(errors)} problem{plural}:"
                "\n- " + "\n- ".join(errors),
                item_name="structure",
                reason="Invalid structure definition",
            )
        self._spec = spec

        super().__init__(name=name or spec.get("name", "custom"), **kwargs)

        # Default options: the snow scaffold, overridden by the structure's
        # own options, then by the constructor keyword arguments.
        self.options.update(_SNOW_OPTION_DEFAULTS)
        self.options.update(spec.get("options") or {})
        self.allowed_land_cover_types = spec.get("allowed_land_cover_types") or [
            "open",
            "ground",
            "generic_land_cover",
            "glacier",
            "forest",
            "water",
        ]

        self._set_options(kwargs)

        try:
            self._define_structure()
            self._generate_structure()
            self._define_parameter_aliases()
            self._define_parameter_constraints()
            self._define_parameter_transforms()
        except RuntimeError as err:
            raise ModelError(f"Custom model initialization raised an exception: {err}")

    def _set_specific_options(self, kwargs: dict[str, Any]) -> None:
        """No model-specific options beyond the (generic) snow options."""

    def _define_structure(self) -> None:
        self.structure = copy.deepcopy(self._spec["bricks"])
        declared = set(self.land_cover_names)
        covers = [
            key
            for key, brick in self.structure.items()
            if brick.get("kind") == "land_cover"
        ]
        for key in covers:
            if key not in declared:
                raise ConfigurationError(
                    f"The structure declares a land cover brick '{key}' that "
                    "is not among the model's land covers "
                    f"({', '.join(self.land_cover_names)}). Pass matching "
                    "land_cover_names/land_cover_types to the model.",
                    item_name=key,
                    reason="Unknown land cover",
                )

    def _define_parameter_aliases(self) -> None:
        self.parameter_aliases = dict(self._spec.get("aliases") or {})

    def _define_parameter_constraints(self) -> None:
        self.parameter_constraints = [
            tuple(constraint) for constraint in (self._spec.get("constraints") or [])
        ]
