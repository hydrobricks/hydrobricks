"""Compare model settings across a matrix of variants (multi-* studies).

A *study file* declares a whole comparison experiment as data: a ``base``
project configuration (see :mod:`hydrobricks.project`) plus a GitHub-Actions
style ``matrix`` whose dimensions are crossed into independent *jobs* — one
calibration + assessment per combination. Any configuration key can be a
dimension (catchments, models, objectives, transforms, algorithms, forcing
datasets, ...), which makes the formulation generic::

    name: my-study
    output: results            # study directory: jobs/, results/, cache/

    base:                      # a project config skeleton (shared settings)
      periods:
        calibration: [1981-01-01, 2000-12-31]
        validation: [2001-01-01, 2020-12-31]
        spinup: 4y
      forcing: { ...shared defaults... }
      calibration: {algorithm: sceua, repetitions: 300}

    variants:                  # named config patches, per dimension
      catchment:
        appenzell: {hydro_units: {...}, forcing: {...}, observations: {...}}
        stgallen:  {...}
      model:
        socont2: {model: {name: socont, options: {soil_storage_nb: 2}},
                  calibration: {parameters: [a_snow, A, k_slow_1]}}
        gr4j:    {model: {name: gr4j},
                  calibration: {parameters: [X1, X2, X3, X4]}}

    matrix:
      catchment: [appenzell, stgallen]
      model: [socont2, gr4j]
      objective: [kge_2012, kge_np]      # shorthand -> calibration.objective
      transform: [none, "power(0.2)"]    # shorthand -> calibration.transform
      exclude:                           # remove combos (partial match)
        - {model: gr4j, transform: "power(0.2)"}
      include:                           # append extra combos (all dims given)
        - {catchment: appenzell, model: socont2,
           objective: nse, transform: none}

    evaluation:                # cross-assessment of every finished job
      metrics: [kge_2012, kge_np, nse]
      transforms: [none, "power(0.2)"]

How a matrix value is applied to the base config, for a dimension ``d`` with
value ``v``:

1. If the dimension has ``variants`` — ``v`` names a patch,
   ``variants.d.v``, deep-merged onto the config.
2. Else if ``d`` contains a dot — it is a config path set to ``v``
   (e.g. ``calibration.algorithm: [sceua, mc]``).
3. Else if ``d`` is a shorthand — ``objective`` (``calibration.objective``)
   or ``transform`` (``calibration.transform``).

``exclude`` entries remove every combination matching all their items;
``include`` entries (which must value every dimension) are appended after the
exclusions, so they can re-add or extend the grid.

Each job gets its own ``output`` (``<study output>/jobs/<job id>``) and shares
the study cache (``<study output>/cache``) so identical expensive steps
(e.g. regridding the same catchment for two models) run once. Results are
idempotent JSON records under ``<study output>/results/`` — a finished job is
skipped on re-run — and :meth:`Study.assess` aggregates them into a tidy
table (``results/scores.csv``).

Jobs are independent, so they parallelize at the process level through the
CLI (``hydrobricks study run <file> <job_id>``): one job per process, e.g.
with GNU parallel, PowerShell ``Start-Job`` or a SLURM job array.
"""

from __future__ import annotations

import copy
import itertools
import json
import re
from dataclasses import dataclass, field
from pathlib import Path
from typing import Any

import pandas as pd
import yaml

from hydrobricks._exceptions import ConfigurationError, HydroBricksError

_TOP_LEVEL_KEYS = {"name", "output", "base", "variants", "matrix", "evaluation"}
_MATRIX_CONTROL_KEYS = {"include", "exclude"}
# Columns of the assess() table that a matrix dimension may not shadow.
_RESERVED_DIMENSIONS = {
    "job_id",
    "period",
    "eval_transform",
    "eval_metric",
    "score",
    "calibration_score",
}
_SHORTHANDS = {
    "objective": "calibration.objective",
    "transform": "calibration.transform",
}


def _deep_merge(base: dict, patch: dict) -> dict:
    """A new dict: ``patch`` merged onto ``base`` (dicts recursively)."""
    out = dict(base)
    for key, value in patch.items():
        if isinstance(value, dict) and isinstance(out.get(key), dict):
            out[key] = _deep_merge(out[key], value)
        else:
            out[key] = value
    return out


def _set_dotted(config: dict, path: str, value: Any) -> None:
    """Set a dotted path (e.g. 'calibration.objective') in a nested dict."""
    keys = path.split(".")
    node = config
    for key in keys[:-1]:
        child = node.get(key)
        if not isinstance(child, dict):
            child = {}
            node[key] = child
        node = child
    node[keys[-1]] = value


def _sanitize(value: Any) -> str:
    """A filesystem-safe token for a matrix value ('power(0.2)' -> 'power-0.2')."""
    return re.sub(r"[^A-Za-z0-9_.-]+", "-", str(value)).strip("-")


@dataclass
class StudyJob:
    """One point of the study matrix: a fully resolved project configuration.

    Attributes
    ----------
    id
        The stable job identifier: the sanitized matrix values joined with
        ``__``, in dimension order (e.g. ``appenzell__socont2__kge_2012__none``).
    values
        The raw matrix values, keyed by dimension.
    config
        The resolved project configuration (base + patches + study overrides).
    """

    id: str
    values: dict[str, Any]
    config: dict = field(repr=False)


class Study:
    """A comparison study: a list of jobs plus their result store.

    Build it with :func:`load_study`. Run jobs with :meth:`run` (one) or
    :meth:`run_all` (sequential loop); both are idempotent through the JSON
    records in ``results_dir``. Aggregate with :meth:`assess` and compare with
    :meth:`pivot`.
    """

    def __init__(
        self,
        jobs: list[StudyJob],
        dimensions: list[str],
        output_dir: Path,
        base_dir: Path,
        evaluation: dict | None = None,
        name: str | None = None,
    ) -> None:
        self.jobs = jobs
        self.dimensions = dimensions
        self.output_dir = Path(output_dir)
        self.base_dir = Path(base_dir)
        self.evaluation = evaluation or {}
        self.name = name

    @property
    def results_dir(self) -> Path:
        """Where the per-job result records and the scores table live."""
        return self.output_dir / "results"

    def job(self, job_id: str) -> StudyJob:
        """The job with the given id (raises with suggestions if unknown)."""
        for job in self.jobs:
            if job.id == job_id:
                return job
        import difflib

        close = difflib.get_close_matches(job_id, [j.id for j in self.jobs], n=3)
        hint = f" Did you mean: {', '.join(close)}?" if close else ""
        raise ConfigurationError(
            f"Unknown job '{job_id}'.{hint} Use 'hydrobricks study list' or "
            f"Study.jobs for the {len(self.jobs)} defined jobs.",
            item_name="job_id",
            item_value=job_id,
            reason="Unknown job id",
        )

    def is_done(self, job_id: str) -> bool:
        """Whether the job already has a result record."""
        return (self.results_dir / f"{job_id}.json").is_file()

    def validate(self) -> None:
        """Validate every job's project configuration (files, columns, ...).

        :func:`load_study` only checks the study structure (matrix, variants,
        calibration settings); this checks each resolved job like
        :func:`~hydrobricks.project.load_project` would — reading the declared
        files — and reports every problem at once, prefixed with the job id.
        """
        from hydrobricks.project import _validate_config

        errors: list[str] = []
        for job in self.jobs:
            job_errors: list[str] = []
            _validate_config(copy.deepcopy(job.config), self.base_dir, job_errors)
            errors.extend(f"job '{job.id}': {e}" for e in job_errors)
        _raise_study_errors(errors, None)

    def run(self, job_id: str, force: bool = False) -> dict:
        """Run one job (calibrate, re-run the best set, assess) idempotently.

        A job with an existing result record is loaded from disk instead of
        recomputed, unless ``force`` is given. Returns the result record:
        the matrix ``values``, the ``calibration_score`` and
        ``best_parameters``, and the long-format ``scores`` list
        (period x transform x metric).
        """
        from hydrobricks.project import load_project

        job = self.job(job_id)
        result_file = self.results_dir / f"{job.id}.json"
        if result_file.is_file() and not force:
            return json.loads(result_file.read_text(encoding="utf-8"))

        project = load_project(copy.deepcopy(job.config), base_dir=self.base_dir)
        best = project.calibrate()
        project.run()

        calibration = project.calibration or {}
        metrics = self.evaluation.get("metrics") or [
            calibration.get("objective") or "kge_np"
        ]
        transforms = self.evaluation.get("transforms") or [calibration.get("transform")]

        from hydrobricks.periods import evaluate_periods

        table = evaluate_periods(
            project.model,
            project.observations,
            project.periods,
            metrics=metrics,
            transform=list(transforms),
        )
        scores = [
            {
                "period": str(period),
                "transform": str(transform),
                "metric": str(metric),
                "score": float(table.loc[period, (transform, metric)]),
            }
            for period in table.index
            for transform, metric in table.columns
        ]

        record = {
            "job_id": job.id,
            "values": dict(job.values),
            "algorithm": best["algorithm"],
            "repetitions": best["repetitions"],
            "calibration_score": float(best["score"]),
            "best_parameters": {
                name: float(value) for name, value in best["parameters"].items()
            },
            "scores": scores,
        }
        result_file.parent.mkdir(parents=True, exist_ok=True)
        result_file.write_text(
            json.dumps(record, indent=2, sort_keys=False), encoding="utf-8"
        )
        return record

    def run_all(self, force: bool = False) -> pd.DataFrame:
        """Run every job sequentially (skipping finished ones), then assess."""
        for job in self.jobs:
            self.run(job.id, force=force)
        return self.assess()

    def assess(self) -> pd.DataFrame:
        """Aggregate the finished jobs into a tidy scores table.

        Returns a long-format DataFrame with one column per matrix dimension
        plus ``job_id``, ``period``, ``transform``, ``metric``, ``score`` and
        ``calibration_score``, and writes it to ``results/scores.csv``.
        """
        records = []
        for file in sorted(self.results_dir.glob("*.json")):
            records.append(json.loads(file.read_text(encoding="utf-8")))
        if not records:
            raise ConfigurationError(
                f"No job results found in '{self.results_dir}'. Run the jobs "
                "first (Study.run_all or 'hydrobricks study run').",
                item_name="results",
                reason="No results to assess",
            )
        rows = []
        for record in records:
            for score in record["scores"]:
                rows.append(
                    {
                        **record["values"],
                        "job_id": record["job_id"],
                        "period": score["period"],
                        # 'eval_' prefixes so a matrix dimension may be named
                        # 'transform'/'metric' (calibration vs evaluation).
                        "eval_transform": score["transform"],
                        "eval_metric": score["metric"],
                        "score": score["score"],
                        "calibration_score": record["calibration_score"],
                    }
                )
        table = pd.DataFrame(rows)
        table.to_csv(self.results_dir / "scores.csv", index=False)
        return table

    def pivot(
        self,
        period: str = "validation",
        metric: str | None = None,
    ) -> pd.DataFrame:
        """A comparison pivot: dimensions as rows, evaluation columns.

        Parameters
        ----------
        period
            The period to compare on. Default: ``'validation'``.
        metric
            Restrict to one evaluation metric. Default: all.
        """
        table = self.assess()
        table = table[table["period"] == period]
        if metric is not None:
            table = table[table["eval_metric"] == metric]
        index = [d for d in self.dimensions if d in table.columns]
        return table.pivot_table(
            index=index, columns=["eval_transform", "eval_metric"], values="score"
        )


# --- Loading and matrix resolution --------------------------------------------


def load_study(
    source: str | Path | dict,
    base_dir: str | Path | None = None,
) -> Study:
    """Build a :class:`Study` from a YAML study file (or dict).

    The matrix is resolved into jobs and the study structure is validated
    (dimensions, variants, include/exclude, calibration settings), with all
    problems reported together. The per-job project configurations (declared
    files, columns, ...) are checked when a job runs — or all at once with
    :meth:`Study.validate`.

    Parameters
    ----------
    source
        Path to a YAML study file, or an equivalent (already parsed) mapping.
    base_dir
        Directory used to resolve the relative paths in the configurations.
        Defaults to the study file directory (or the current working directory
        when ``source`` is a dict).
    """
    path: Path | None = None
    if isinstance(source, (str, Path)):
        path = Path(source)
        if not path.is_file():
            raise ConfigurationError(
                f"The study file '{path}' does not exist.",
                item_name="source",
                item_value=str(path),
                reason="File not found",
            )
        try:
            with open(path, encoding="utf-8") as f:
                config = yaml.safe_load(f)
        except yaml.YAMLError as err:
            raise ConfigurationError(
                f"The study file '{path}' is not valid YAML: {err}",
                item_name="source",
                reason="Invalid YAML",
            ) from None
        base = Path(base_dir) if base_dir is not None else path.parent
    elif isinstance(source, dict):
        config = source
        base = Path(base_dir) if base_dir is not None else Path.cwd()
    else:
        raise ConfigurationError(
            "The study source must be a path to a YAML file or a dict, "
            f"not {type(source).__name__}.",
            item_name="source",
            reason="Invalid type",
        )

    if not isinstance(config, dict):
        raise ConfigurationError(
            "The study file must contain a mapping of sections "
            "(base, matrix, variants, ...).",
            item_name="source",
            reason="Not a mapping",
        )

    errors: list[str] = []
    _check_study_keys(config, errors)

    base_config = config.get("base")
    if not isinstance(base_config, dict):
        errors.append("base: this section is required (a project config skeleton).")
        base_config = {}
    variants = _validate_variants(config, errors)
    dimensions, combos = _resolve_matrix(config, variants, errors)
    evaluation = _validate_evaluation(config, errors)
    _raise_study_errors(errors, path)

    output = config.get("output")
    if output is not None and not isinstance(output, str):
        errors.append(f"output: expected a directory path, got {output!r}.")
        output = None
    out_dir = Path(output) if output is not None else Path("study")
    if not out_dir.is_absolute():
        out_dir = base / out_dir

    jobs = _build_jobs(base_config, variants, dimensions, combos, out_dir, errors)
    _raise_study_errors(errors, path)

    name = config.get("name")
    return Study(
        jobs=jobs,
        dimensions=dimensions,
        output_dir=out_dir,
        base_dir=base,
        evaluation=evaluation,
        name=str(name) if name is not None else None,
    )


def _raise_study_errors(errors: list[str], path: Path | None) -> None:
    if not errors:
        return
    where = f" '{path}'" if path is not None else ""
    plural = "s" if len(errors) > 1 else ""
    raise ConfigurationError(
        f"The study file{where} has {len(errors)} problem{plural}:\n- "
        + "\n- ".join(errors),
        item_name="study",
        reason="Invalid study configuration",
    )


def _check_study_keys(config: dict, errors: list[str]) -> None:
    import difflib

    for key in config:
        if key not in _TOP_LEVEL_KEYS:
            close = difflib.get_close_matches(str(key), sorted(_TOP_LEVEL_KEYS), n=1)
            hint = f"; did you mean '{close[0]}'?" if close else ""
            errors.append(f"study: unknown key '{key}'{hint}")


def _validate_variants(config: dict, errors: list[str]) -> dict:
    section = config.get("variants")
    if section is None:
        return {}
    if not isinstance(section, dict):
        errors.append(
            "variants: expected a mapping of dimension to named config patches."
        )
        return {}
    out: dict[str, dict] = {}
    for dim, patches in section.items():
        if not isinstance(patches, dict) or not all(
            isinstance(p, dict) for p in patches.values()
        ):
            errors.append(
                f"variants.{dim}: expected a mapping of variant name to a "
                "config patch (a mapping)."
            )
            continue
        out[str(dim)] = {str(name): patch for name, patch in patches.items()}
    return out


def _validate_evaluation(config: dict, errors: list[str]) -> dict:
    section = config.get("evaluation")
    if section is None:
        return {}
    if not isinstance(section, dict):
        errors.append("evaluation: expected a mapping (metrics, transforms).")
        return {}
    for key in section:
        if key not in ("metrics", "transforms"):
            errors.append(f"evaluation: unknown key '{key}'.")
    out: dict = {}
    metrics = section.get("metrics")
    if metrics is not None:
        if not isinstance(metrics, list) or not all(
            isinstance(m, str) for m in metrics
        ):
            errors.append("evaluation.metrics: expected a list of metric names.")
        else:
            out["metrics"] = list(metrics)
    transforms = section.get("transforms")
    if transforms is not None:
        if not isinstance(transforms, list):
            errors.append(
                "evaluation.transforms: expected a list of transform " "specifications."
            )
        else:
            from hydrobricks.evaluation.transforms import DischargeTransform

            for spec in transforms:
                try:
                    DischargeTransform.from_spec(
                        None if spec in (None, "none") else spec
                    )
                except (HydroBricksError, ValueError, TypeError) as err:
                    message = err.args[0] if getattr(err, "args", None) else str(err)
                    errors.append(f"evaluation.transforms: {message}")
            out["transforms"] = list(transforms)
    return out


def _resolve_matrix(
    config: dict, variants: dict, errors: list[str]
) -> tuple[list[str], list[dict]]:
    """The matrix dimensions (in order) and the resolved combinations."""
    section = config.get("matrix")
    if not isinstance(section, dict) or not (set(section) - _MATRIX_CONTROL_KEYS):
        errors.append(
            "matrix: this section is required (at least one dimension with a "
            "list of values)."
        )
        return [], []

    dimensions: list[str] = []
    axes: dict[str, list] = {}
    for dim, values in section.items():
        if dim in _MATRIX_CONTROL_KEYS:
            continue
        dim = str(dim)
        if dim in _RESERVED_DIMENSIONS:
            errors.append(
                f"matrix.{dim}: '{dim}' is a reserved results column; rename "
                "the dimension."
            )
            continue
        if not isinstance(values, list) or not values:
            errors.append(f"matrix.{dim}: expected a non-empty list of values.")
            continue
        for value in values:
            if isinstance(value, (dict, list)):
                errors.append(
                    f"matrix.{dim}: values must be scalars (a variant name or "
                    f"a literal), got {value!r}. Put mappings under "
                    f"variants.{dim} and reference them by name."
                )
        dimensions.append(dim)
        axes[dim] = list(values)

    # Every dimension must be applicable to the config.
    for dim in dimensions:
        if dim in variants:
            missing = [
                v for v in axes[dim] if not isinstance(v, str) or v not in variants[dim]
            ]
            if missing:
                errors.append(
                    f"matrix.{dim}: no variant patch for "
                    f"{', '.join(repr(v) for v in missing)}; define them under "
                    f"variants.{dim}."
                )
        elif "." not in dim and dim not in _SHORTHANDS:
            errors.append(
                f"matrix.{dim}: unknown dimension; define patches under "
                f"variants.{dim}, use a dotted config path (e.g. "
                f"'calibration.algorithm') or one of the shorthands "
                f"({', '.join(sorted(_SHORTHANDS))})."
            )
    if errors:
        return dimensions, []

    combos = [
        dict(zip(dimensions, values))
        for values in itertools.product(*(axes[d] for d in dimensions))
    ]

    combos = _apply_exclude(section, dimensions, combos, errors)
    combos = _apply_include(section, dimensions, combos, errors)
    return dimensions, combos


def _apply_exclude(
    section: dict, dimensions: list[str], combos: list[dict], errors: list[str]
) -> list[dict]:
    exclude = section.get("exclude")
    if exclude is None:
        return combos
    if not isinstance(exclude, list) or not all(isinstance(e, dict) for e in exclude):
        errors.append("matrix.exclude: expected a list of {dimension: value} maps.")
        return combos
    for entry in exclude:
        unknown = set(entry) - set(dimensions)
        if unknown:
            errors.append(
                f"matrix.exclude: unknown dimension(s) "
                f"{', '.join(sorted(str(u) for u in unknown))}."
            )
            return combos
    return [
        combo
        for combo in combos
        if not any(
            all(combo[dim] == value for dim, value in entry.items())
            for entry in exclude
        )
    ]


def _apply_include(
    section: dict, dimensions: list[str], combos: list[dict], errors: list[str]
) -> list[dict]:
    include = section.get("include")
    if include is None:
        return combos
    if not isinstance(include, list) or not all(isinstance(e, dict) for e in include):
        errors.append("matrix.include: expected a list of {dimension: value} maps.")
        return combos
    combos = list(combos)
    for entry in include:
        if set(entry) != set(dimensions):
            errors.append(
                f"matrix.include: each entry must value every dimension "
                f"({', '.join(dimensions)}), got {sorted(str(k) for k in entry)}."
            )
            continue
        combo = {dim: entry[dim] for dim in dimensions}
        if combo not in combos:
            combos.append(combo)
    return combos


def _build_jobs(
    base_config: dict,
    variants: dict,
    dimensions: list[str],
    combos: list[dict],
    out_dir: Path,
    errors: list[str],
) -> list[StudyJob]:
    jobs: list[StudyJob] = []
    seen: dict[str, dict] = {}
    for combo in combos:
        job_id = "__".join(_sanitize(combo[dim]) for dim in dimensions)
        if job_id in seen:
            if seen[job_id] != combo:
                errors.append(
                    f"job '{job_id}': id collision between {seen[job_id]} and "
                    f"{combo} after sanitization; rename the values."
                )
            continue
        seen[job_id] = combo

        config = copy.deepcopy(base_config)
        for dim in dimensions:
            value = combo[dim]
            if dim in variants:
                config = _deep_merge(config, variants[dim][value])
            elif "." in dim:
                _set_dotted(config, dim, value)
            else:
                _set_dotted(config, _SHORTHANDS[dim], value)

        # Study-level overrides: one output dir per job, one shared cache.
        config["output"] = str(out_dir / "jobs" / job_id)
        if "cache" not in config:
            config["cache"] = str(out_dir / "cache")

        calibration = config.get("calibration")
        if not isinstance(calibration, dict) or not (
            calibration.get("repetitions") and calibration.get("parameters")
        ):
            errors.append(
                f"job '{job_id}': a 'calibration' section with 'repetitions' "
                "and 'parameters' is required (in base or via a variant patch)."
            )

        jobs.append(StudyJob(id=job_id, values=combo, config=config))
    return jobs
