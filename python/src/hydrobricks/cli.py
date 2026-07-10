"""Command-line interface: create, check and run YAML project files.

The subcommands wrap the :mod:`hydrobricks.project` and
:mod:`hydrobricks.study` loaders so the canonical workflows need no Python at
all::

    hydrobricks init                 # interactive wizard, writes project.yaml
    hydrobricks validate project.yaml
    hydrobricks run project.yaml

    hydrobricks study list study.yaml       # the comparison matrix's jobs
    hydrobricks study run study.yaml <job>  # one job (parallelize per process)
    hydrobricks study run study.yaml --all
    hydrobricks study assess study.yaml     # aggregate + comparison pivot

``init`` asks a short series of questions (proposing answers sniffed from the
CSV files: column names, date formats, data period) and writes a commented
project file — including the model's parameter list with valid ranges. It
generates *configuration*, not code: the file can be re-edited or regenerated
freely, and anything beyond it is done in Python via
:func:`hydrobricks.load_project`.
"""

from __future__ import annotations

import argparse
import sys
from datetime import datetime
from pathlib import Path

import pandas as pd

from hydrobricks._exceptions import HydroBricksError
from hydrobricks.project import _model_registry, load_project

_TIME_FORMATS = (
    "%Y-%m-%d",
    "%d/%m/%Y",
    "%m/%d/%Y",
    "%d.%m.%Y",
    "%Y/%m/%d",
    "%d-%m-%Y",
    "%Y%m%d",
)

_COLUMN_KEYWORDS = {
    "time": ("date", "time"),
    "precipitation": ("precip", "rain", "rr"),
    "temperature": ("temp", "tabs", "tmean"),
    "pet": ("pet", "etp", "evap"),
    "discharge": ("discharge", "flow", "runoff", "debit"),
    "elevation": ("elevation", "elev", "altitude", "alt"),
    "area": ("area",),
    "slope": ("slope",),
}

_SINGLE_LETTER = {"precipitation": "p", "temperature": "t", "discharge": "q"}


def main(argv: list[str] | None = None) -> int:
    """Entry point of the ``hydrobricks`` command. Returns the exit code."""
    parser = argparse.ArgumentParser(
        prog="hydrobricks",
        description="Create, check and run hydrobricks YAML project files.",
    )
    subparsers = parser.add_subparsers(dest="command", required=True)

    parser_init = subparsers.add_parser(
        "init",
        help="Create a project file interactively (proposes answers sniffed "
        "from your CSV files).",
    )
    parser_init.add_argument(
        "path",
        nargs="?",
        default="project.yaml",
        help="Project file to write (default: project.yaml).",
    )

    parser_validate = subparsers.add_parser(
        "validate",
        help="Check a project file and report every problem at once.",
    )
    parser_validate.add_argument("project", help="Path to the project file.")

    parser_run = subparsers.add_parser(
        "run",
        help="Run the model described by a project file and report the scores.",
    )
    parser_run.add_argument("project", help="Path to the project file.")

    parser_study = subparsers.add_parser(
        "study",
        help="Run a multi-variant comparison study (matrix of jobs).",
    )
    study_subparsers = parser_study.add_subparsers(dest="study_command", required=True)

    study_list = study_subparsers.add_parser(
        "list", help="List the study's jobs and their status."
    )
    study_list.add_argument("study", help="Path to the study file.")

    study_validate = study_subparsers.add_parser(
        "validate",
        help="Check every job's project configuration (files, columns, ...) "
        "and report every problem at once.",
    )
    study_validate.add_argument("study", help="Path to the study file.")

    study_run = study_subparsers.add_parser(
        "run",
        help="Run one job (or all with --all); finished jobs are skipped, so "
        "jobs can be distributed one-per-process (e.g. a SLURM array).",
    )
    study_run.add_argument("study", help="Path to the study file.")
    study_run.add_argument("job_id", nargs="?", help="Job to run (see 'study list').")
    study_run.add_argument(
        "--all", action="store_true", help="Run every pending job sequentially."
    )
    study_run.add_argument(
        "--force", action="store_true", help="Recompute even if a result exists."
    )

    study_assess = study_subparsers.add_parser(
        "assess",
        help="Aggregate the finished jobs into results/scores.csv and print "
        "the comparison pivot.",
    )
    study_assess.add_argument("study", help="Path to the study file.")
    study_assess.add_argument(
        "--period",
        default="validation",
        help="Period for the printed pivot (default: validation).",
    )

    args = parser.parse_args(argv)
    try:
        if args.command == "init":
            return _cmd_init(args)
        if args.command == "validate":
            return _cmd_validate(args)
        if args.command == "study":
            return _cmd_study(args)
        return _cmd_run(args)
    except HydroBricksError as err:
        # The message is args[0]; str(err) would render the whole args tuple.
        message = err.args[0] if err.args else str(err)
        print(f"\nError: {message}", file=sys.stderr)
        return 1
    except KeyboardInterrupt:
        print("\nAborted.", file=sys.stderr)
        return 130


# --- validate / run ----------------------------------------------------------


def _cmd_validate(args: argparse.Namespace) -> int:
    project = load_project(args.project)
    start, end = project.periods.full_span.bounds
    print(f"'{args.project}' is valid.")
    print(f"  model:       {project.model.name}")
    print(f"  hydro units: {len(project.forcing.hydro_units)}")
    print(f"  simulation:  {start} to {end} (spinup: {project.periods.spinup})")
    print(f"  output:      {project.output_dir}")
    undefined = project.parameters.get_undefined()
    if undefined:
        print(
            f"  note: {len(undefined)} parameter(s) still need a value "
            f"({', '.join(undefined)})."
        )
    return 0


def _cmd_run(args: argparse.Namespace) -> int:
    project = load_project(args.project)
    start, end = project.periods.full_span.bounds
    print(f"Running {project.model.name} over {start} to {end}...")
    simulated = project.run()

    csv_path = project.output_dir / "simulated_discharge.csv"
    simulated.to_csv(csv_path, index_label="date")
    project.model.dump_outputs(str(project.output_dir))
    print(f"Mean simulated discharge: {simulated.mean():.3f} mm/d")
    print(f"Results written to {project.output_dir}")

    if project.observations is not None:
        from hydrobricks.periods import evaluate_periods

        scores = evaluate_periods(
            project.model,
            project.observations,
            project.periods,
            metrics=("nse", "kge_2012"),
        )
        print("\nScores against the observed discharge:")
        print(scores.to_string(float_format=lambda x: f"{x:.3f}"))
    return 0


# --- study --------------------------------------------------------------------


def _cmd_study(args: argparse.Namespace) -> int:
    from hydrobricks.study import load_study

    study = load_study(args.study)

    if args.study_command == "list":
        done = sum(study.is_done(job.id) for job in study.jobs)
        name = f" '{study.name}'" if study.name else ""
        print(
            f"Study{name}: {len(study.jobs)} job(s), {done} done "
            f"({', '.join(study.dimensions)})."
        )
        for job in study.jobs:
            status = "done" if study.is_done(job.id) else "pending"
            print(f"  [{status:7s}] {job.id}")
        return 0

    if args.study_command == "validate":
        study.validate()
        print(f"'{args.study}' is valid: all {len(study.jobs)} job(s) can run.")
        return 0

    if args.study_command == "run":
        if args.all == (args.job_id is not None):
            print(
                "Error: pass either a job id or --all (see 'study list').",
                file=sys.stderr,
            )
            return 2
        job_ids = [j.id for j in study.jobs] if args.all else [args.job_id]
        for job_id in job_ids:
            if study.is_done(job_id) and not args.force:
                print(f"[done   ] {job_id} (skipped; --force to recompute)")
                continue
            print(f"[running] {job_id}...")
            record = study.run(job_id, force=args.force)
            print(
                f"[done   ] {job_id}: calibration score "
                f"{record['calibration_score']:.3f}"
            )
        return 0

    # assess
    scores = study.assess()
    print(f"Aggregated {scores['job_id'].nunique()} job(s) into ")
    print(f"  {study.results_dir / 'scores.csv'}")
    print(f"\nComparison on the {args.period} period:")
    pivot = study.pivot(period=args.period)
    print(pivot.to_string(float_format=lambda x: f"{x:.3f}"))
    return 0


# --- init wizard: sniffing helpers -------------------------------------------


def _guess_column(columns: list[str], variable: str) -> str | None:
    """Propose the CSV column for a variable by keyword (None if no match)."""
    lowered = {col: col.lower() for col in columns}
    for keyword in _COLUMN_KEYWORDS.get(variable, ()):
        for col, low in lowered.items():
            if keyword in low:
                return col
    single = _SINGLE_LETTER.get(variable)
    if single:
        for col, low in lowered.items():
            if low == single:
                return col
    return None


def _detect_time_format(values: list) -> str | None:
    """Detect the date format matching all sample values (None if none does)."""
    for fmt in _TIME_FORMATS:
        try:
            for value in values:
                datetime.strptime(str(value), fmt)
            return fmt
        except ValueError:
            continue
    return None


def _read_csv_header(path: Path) -> list[str]:
    return list(pd.read_csv(path, nrows=0).columns)


def _read_time_info(
    path: Path, time_column: str
) -> tuple[str | None, str | None, str | None]:
    """The detected (format, first date, last date) of a CSV time column."""
    values = pd.read_csv(path, usecols=[time_column], dtype=str)[time_column]
    values = values.dropna()
    if values.empty:
        return None, None, None
    sample = pd.concat([values.head(200), values.tail(200)])
    fmt = _detect_time_format(sample.tolist())
    if fmt is None:
        return None, None, None
    first = datetime.strptime(values.iloc[0], fmt).strftime("%Y-%m-%d")
    last = datetime.strptime(values.iloc[-1], fmt).strftime("%Y-%m-%d")
    return fmt, first, last


def _sniff_netcdf(
    path: Path, file_pattern: str | None
) -> tuple[list[str], list[str], tuple[str | None, str | None]]:
    """The (data variables, dims, time range) of the first matching netCDF file.

    Returns empty proposals when xarray is unavailable or the file cannot be
    read — the wizard then simply asks without defaults.
    """
    from hydrobricks._optional import HAS_XARRAY, xr

    empty: tuple[list[str], list[str], tuple[str | None, str | None]] = (
        [],
        [],
        (None, None),
    )
    if not HAS_XARRAY:
        return empty
    if path.is_file():
        file = path
    else:
        file = next(iter(sorted(path.glob(file_pattern or "*.nc"))), None)
    if file is None:
        return empty
    try:
        dataset = xr.open_dataset(file)
    except Exception:
        return empty
    try:
        variables = [str(v) for v in dataset.data_vars if dataset[v].ndim >= 3]
        variables = variables or [str(v) for v in dataset.data_vars]
        dims = [str(d) for d in dataset.dims]
        start = end = None
        time_dim = _guess_dim(dims, "time")
        if time_dim is not None:
            values = pd.to_datetime(dataset[time_dim].values)
            start = values.min().strftime("%Y-%m-%d")
            end = values.max().strftime("%Y-%m-%d")
        return variables, dims, (start, end)
    finally:
        dataset.close()


def _guess_dim(dims: list[str], kind: str) -> str | None:
    """Propose the dimension name for time/x/y (None if no match)."""
    candidates = {
        "time": ("time", "date"),
        "x": ("x", "e", "lon", "longitude", "easting"),
        "y": ("y", "n", "lat", "latitude", "northing"),
    }[kind]
    lowered = {d.lower(): d for d in dims}
    for candidate in candidates:
        if candidate in lowered:
            return lowered[candidate]
    return None


# --- init wizard: prompting helpers ------------------------------------------


def _ask(prompt: str, default: str | None = None) -> str:
    """Prompt until an answer (or the default) is given. '' accepts a '' default."""
    suffix = f" [{default}]" if default else ""
    while True:
        answer = input(f"{prompt}{suffix}: ").strip()
        if answer:
            return answer
        if default is not None:
            return default


def _ask_choice(prompt: str, choices: list[str], default: str) -> str:
    while True:
        answer = _ask(f"{prompt} ({'/'.join(choices)})", default)
        if answer in choices:
            return answer
        print(f"  Please answer one of: {', '.join(choices)}.")


def _ask_file(prompt: str, optional: bool = False) -> Path | None:
    while True:
        answer = _ask(prompt, default="" if optional else None)
        if not answer and optional:
            return None
        path = Path(answer).expanduser()
        if path.is_file():
            return path
        print(f"  '{path}' does not exist; enter a valid file path.")


def _ask_path(prompt: str, optional: bool = False) -> Path | None:
    """Like :func:`_ask_file`, but also accepts directories."""
    while True:
        answer = _ask(prompt, default="" if optional else None)
        if not answer and optional:
            return None
        path = Path(answer).expanduser()
        if path.exists():
            return path
        print(f"  '{path}' does not exist; enter a valid path.")


def _ask_float(prompt: str, default: str | None = None) -> float:
    while True:
        answer = _ask(prompt, default)
        try:
            return float(answer)
        except ValueError:
            print("  Please enter a number.")


def _ask_column(prompt: str, columns: list[str], default: str | None) -> str:
    while True:
        answer = _ask(prompt, default)
        if answer in columns:
            return answer
        print(f"  No column '{answer}'. Available: {', '.join(columns)}.")


def _ask_date(prompt: str, default: str | None = None) -> str:
    while True:
        answer = _ask(prompt, default)
        try:
            datetime.strptime(answer, "%Y-%m-%d")
            return answer
        except ValueError:
            print("  Please enter a date as YYYY-MM-DD.")


def _ask_gridded_source(label: str, path: Path | None = None) -> dict:
    """Ask for one gridded netCDF source, proposing sniffed answers."""
    if path is None:
        path = _ask_path(f"{label} netCDF file or folder")
    file_pattern = None
    if path.is_dir():
        while True:
            file_pattern = _ask("  File pattern (e.g. '*.nc')", "*.nc")
            if any(path.glob(file_pattern)):
                break
            print(f"  No file in '{path}' matches '{file_pattern}'.")
    variables, dims, time_range = _sniff_netcdf(path, file_pattern)
    while True:
        var_name = _ask(
            "  Variable name in the file", variables[0] if variables else None
        )
        if not variables or var_name in variables:
            break
        print(f"  No variable '{var_name}'. Available: {', '.join(variables)}.")
    dim_time = _ask("  Time dimension", _guess_dim(dims, "time") or "time")
    dim_x = _ask("  X dimension", _guess_dim(dims, "x") or "x")
    dim_y = _ask("  Y dimension", _guess_dim(dims, "y") or "y")
    crs = None
    while True:
        answer = _ask("  CRS (EPSG id, empty to read it from the file)", "")
        if not answer:
            break
        try:
            crs = int(answer)
            break
        except ValueError:
            print("  Please enter an integer EPSG id.")
    return {
        "path": path,
        "file_pattern": file_pattern,
        "var_name": var_name,
        "crs": crs,
        "dim_time": dim_time,
        "dim_x": dim_x,
        "dim_y": dim_y,
        "time_range": time_range,
    }


# --- init wizard -------------------------------------------------------------


def _cmd_init(args: argparse.Namespace) -> int:
    target = Path(args.path)
    if target.exists():
        overwrite = _ask_choice(
            f"'{target}' already exists; overwrite?", ["y", "n"], "n"
        )
        if overwrite == "n":
            print("Aborted.")
            return 1

    print("This wizard writes a hydrobricks project file. Press Enter to")
    print("accept the proposed [default] answers.\n")

    # Model.
    models = sorted(_model_registry())
    model_name = _ask_choice("Model", models, "socont")

    # Hydro units: loaded from a CSV or delineated from a DEM.
    hu_source = _ask_choice(
        "Hydro units source (csv file / delineated from a dem)", ["csv", "dem"], "csv"
    )
    hu_cfg: dict = {"kind": hu_source}
    model_options = {}
    option_lines = ["  # options: {}  # model-specific options, see the documentation"]
    if hu_source == "csv":
        hu_file = _ask_file("Hydro units CSV (elevation bands, two header rows)")
        hu_columns = _read_csv_header(hu_file)
        elevation_col = _ask_column(
            "  Elevation column", hu_columns, _guess_column(hu_columns, "elevation")
        )
        area_col = _ask_column(
            "  Area column", hu_columns, _guess_column(hu_columns, "area")
        )
        hu_cfg.update(file=hu_file, elevation_col=elevation_col, area_col=area_col)

        # Adapt the model to the available hydro unit properties (a DEM-based
        # delineation computes the slope itself).
        hu_other_columns = {}
        slope_col = _guess_column(hu_columns, "slope")
        if model_name == "socont":
            if slope_col:
                hu_other_columns["slope"] = slope_col
            else:
                model_options["surface_runoff"] = "linear_storage"
                option_lines = [
                    "  options:",
                    "    # The default surface runoff ('socont_runoff', a kinematic",
                    "    # wave) needs a 'slope' column in the hydro units CSV; none",
                    "    # was found, so a linear storage is used instead.",
                    "    surface_runoff: linear_storage",
                ]
        hu_cfg["other_columns"] = hu_other_columns
    else:
        outline = _ask_file("Catchment outline shapefile")
        dem_file = _ask_file("DEM raster (GeoTIFF)")
        method = _ask_choice(
            "  Elevation bands method",
            ["equal_intervals", "quantiles"],
            "equal_intervals",
        )
        distance = number = None
        if method == "equal_intervals":
            distance = _ask_float("  Band height [m]", "100")
        else:
            number = int(_ask_float("  Number of bands", "25"))
        hu_cfg.update(
            outline=outline,
            dem=dem_file,
            method=method,
            distance=distance,
            number=number,
        )

    # Forcing: a station CSV or gridded netCDF data.
    source = _ask_choice("Forcing source", ["station", "gridded"], "station")
    forcing_cfg: dict = {"kind": source}
    if source == "station":
        meteo_file = _ask_file("Meteo CSV (station time series)")
        meteo_columns = _read_csv_header(meteo_file)
        meteo_time_col = _ask_column(
            "  Time column", meteo_columns, _guess_column(meteo_columns, "time")
        )
        detected_fmt, data_start, data_end = _read_time_info(meteo_file, meteo_time_col)
        meteo_time_fmt = _ask("  Time format", detected_fmt)
        precip_col = _ask_column(
            "  Precipitation column",
            meteo_columns,
            _guess_column(meteo_columns, "precipitation"),
        )
        temp_col = _ask_column(
            "  Temperature column",
            meteo_columns,
            _guess_column(meteo_columns, "temperature"),
        )
        pet_default = _guess_column(meteo_columns, "pet") or ""
        while True:
            pet_col = _ask(
                "  PET column (empty to compute PET from temperature)", pet_default
            )
            if not pet_col or pet_col in meteo_columns:
                break
            print(f"  No column '{pet_col}'. Available: {', '.join(meteo_columns)}.")
        latitude = None
        if not pet_col and hu_cfg["kind"] == "csv":
            # With an outline/DEM the latitude is derived from the catchment.
            latitude = _ask_float("  Catchment latitude (for the PET computation)")
        ref_elevation = _ask_float("  Station (reference) elevation [m a.s.l.]")
        forcing_cfg.update(
            meteo_file=meteo_file,
            meteo_time_col=meteo_time_col,
            meteo_time_fmt=meteo_time_fmt,
            precip_col=precip_col,
            temp_col=temp_col,
            pet_col=pet_col,
            latitude=latitude,
            ref_elevation=ref_elevation,
        )
    else:
        if hu_cfg["kind"] == "dem":
            # The unit ids raster is generated by the delineation, and the
            # outline/DEM (needed for the data gradients) are already known.
            unit_ids_raster = None
            outline = dem = None
            with_gradient = True
        else:
            unit_ids_raster = _ask_file(
                "Hydro unit ids raster (GeoTIFF, e.g. unit_ids.tif)"
            )
            outline = _ask_file(
                "Catchment outline shapefile (empty to skip elevation "
                "gradients derived from the grids)",
                optional=True,
            )
            dem = _ask_file("  DEM raster (GeoTIFF)") if outline is not None else None
            with_gradient = outline is not None

        gridded = {}
        data_start = data_end = None
        for variable in ("precipitation", "temperature"):
            spec = _ask_gridded_source(variable.capitalize())
            spec["elevation_gradient"] = with_gradient
            gridded[variable] = spec
            data_start = data_start or spec["time_range"][0]
            data_end = data_end or spec["time_range"][1]

        pet_path = _ask_path(
            "PET netCDF file or folder (empty to compute PET from temperature)",
            optional=True,
        )
        latitude = None
        if pet_path is not None:
            spec = _ask_gridded_source("PET", path=pet_path)
            spec["elevation_gradient"] = False
            gridded["pet"] = spec
        elif outline is None and hu_cfg["kind"] == "csv":
            latitude = _ask_float("  Catchment latitude (for the PET computation)")

        forcing_cfg.update(
            gridded=gridded,
            latitude=latitude,
            unit_ids_raster=unit_ids_raster,
            outline=outline,
            dem=dem,
        )

    # Observations.
    discharge_file = _ask_file("Observed discharge CSV (empty to skip)", optional=True)
    discharge = None
    if discharge_file is not None:
        q_columns = _read_csv_header(discharge_file)
        q_time_col = _ask_column(
            "  Time column", q_columns, _guess_column(q_columns, "time")
        )
        q_fmt, _, _ = _read_time_info(discharge_file, q_time_col)
        q_time_fmt = _ask("  Time format", q_fmt)
        q_col = _ask_column(
            "  Discharge column", q_columns, _guess_column(q_columns, "discharge")
        )
        discharge = (discharge_file, q_time_col, q_time_fmt, q_col)

    # Periods.
    if data_start:
        print(f"The meteo data covers {data_start} to {data_end}.")
    start = _ask_date("Simulation start (YYYY-MM-DD)", data_start)
    end = _ask_date("Simulation end (YYYY-MM-DD)", data_end)
    calibration_end = _ask(
        "End of the calibration period (empty for no calibration/validation " "split)",
        "",
    )
    spinup = _ask("Spin-up (days or e.g. '2y')", "2y")
    output = _ask("Output directory", "output")

    text = _project_yaml(
        model_name=model_name,
        model_options=model_options,
        option_lines=option_lines,
        hydro_units=hu_cfg,
        forcing=forcing_cfg,
        discharge=discharge,
        start=start,
        end=end,
        calibration_end=calibration_end,
        spinup=spinup,
        output=output,
    )
    target.write_text(text, encoding="utf-8")

    print(f"\nWrote '{target}'.")
    print("Next steps:")
    print("  1. Set the parameter values in the 'parameters' section (the")
    print("     valid ranges are listed in the file).")
    print(f"  2. Check it:  hydrobricks validate {target}")
    print(f"  3. Run it:    hydrobricks run {target}")
    return 0


# --- init wizard: project file rendering --------------------------------------


def _yaml_str(value: str) -> str:
    """Quote a string for YAML only when a plain scalar would be unsafe."""
    safe = value and all(c.isalnum() or c in "_-()./ " for c in value)
    if safe and value == value.strip() and not _looks_like_scalar(value):
        return value
    escaped = value.replace("\\", "\\\\").replace('"', '\\"')
    return f'"{escaped}"'


def _looks_like_scalar(value: str) -> bool:
    """True when YAML would parse the bare string as a non-string scalar."""
    if value.lower() in ("true", "false", "yes", "no", "on", "off", "null", "~"):
        return True
    try:
        float(value)
        return True
    except ValueError:
        return False


def _number(value: float) -> str:
    return f"{value:g}"


def _parameter_lines(model_name: str, options: dict) -> list[str]:
    """Commented placeholder lines for every parameter, with ranges and units."""
    from hydrobricks.project import _parameter_label, _parameter_range

    model = _model_registry()[model_name](**options)
    parameter_set = model.generate_parameters()
    lines = []
    for _, row in parameter_set.get_model_parameters().iterrows():
        unit = f" {row['unit']}" if row["unit"] else ""
        lines.append(
            f"  # {_parameter_label(row)}:   " f"# range {_parameter_range(row)}{unit}"
        )
    return lines


def _station_forcing_lines(fc: dict) -> list[str]:
    lines = [
        "forcing:",
        f"  file: {_yaml_str(fc['meteo_file'].as_posix())}",
        f"  time: {{column: {_yaml_str(fc['meteo_time_col'])}, "
        f"format: {_yaml_str(fc['meteo_time_fmt'])}}}",
        "  columns:",
        f"    precipitation: {_yaml_str(fc['precip_col'])}",
        f"    temperature: {_yaml_str(fc['temp_col'])}",
    ]
    if fc["pet_col"]:
        lines.append(f"    pet: {_yaml_str(fc['pet_col'])}")
    lines += [
        f"  ref_elevation: {_number(fc['ref_elevation'])}",
        "  temperature:",
        "    gradient: -0.6  # additive lapse rate per 100 m",
        "  # precipitation:",
        "  #   correction_factor: 1.0  # e.g. undercatch correction",
        "  #   gradient: 0.05          # multiplicative gradient per 100 m",
    ]
    if not fc["pet_col"]:
        lines += ["  pet:", "    method: Oudin"]
        if fc["latitude"] is not None:
            lines.append(f"    lat: {_number(fc['latitude'])}")
    return lines


def _gridded_forcing_lines(fc: dict) -> list[str]:
    lines = ["forcing:", "  gridded:"]
    for variable, spec in fc["gridded"].items():
        lines.append(f"    {variable}:")
        lines.append(f"      path: {_yaml_str(spec['path'].as_posix())}")
        if spec["file_pattern"]:
            lines.append(f"      file_pattern: {_yaml_str(spec['file_pattern'])}")
        lines.append(f"      var_name: {_yaml_str(spec['var_name'])}")
        if spec["crs"] is not None:
            lines.append(f"      data_crs: {spec['crs']}")
        lines.append(f"      dim_time: {_yaml_str(spec['dim_time'])}")
        lines.append(f"      dim_x: {_yaml_str(spec['dim_x'])}")
        lines.append(f"      dim_y: {_yaml_str(spec['dim_y'])}")
        if spec["elevation_gradient"]:
            lines.append("      apply_data_gradient: true")
    if "pet" not in fc["gridded"]:
        lines += ["  pet:", "    method: Oudin"]
        if fc["latitude"] is not None:
            lines.append(f"    lat: {_number(fc['latitude'])}")
    return lines


def _hydro_units_lines(hu: dict, fc: dict) -> list[str]:
    lines = ["hydro_units:"]
    if hu["kind"] == "csv":
        lines += [
            f"  file: {_yaml_str(hu['file'].as_posix())}",
            "  columns:",
            f"    elevation: {_yaml_str(hu['elevation_col'])}",
            f"    area: {_yaml_str(hu['area_col'])}",
            *(
                f"    {prop}: {_yaml_str(col)}"
                for prop, col in hu["other_columns"].items()
            ),
        ]
        if fc.get("unit_ids_raster") is not None:
            lines.append(
                f"  unit_ids_raster: {_yaml_str(fc['unit_ids_raster'].as_posix())}"
            )
        if fc.get("outline") is not None:
            lines.append(f"  outline: {_yaml_str(fc['outline'].as_posix())}")
            lines.append(f"  dem: {_yaml_str(fc['dem'].as_posix())}")
    else:
        lines += [
            f"  outline: {_yaml_str(hu['outline'].as_posix())}",
            f"  dem: {_yaml_str(hu['dem'].as_posix())}",
            "  discretization:",
            f"    method: {hu['method']}",
        ]
        if hu["distance"] is not None:
            lines.append(f"    distance: {_number(hu['distance'])}  # band height [m]")
        if hu["number"] is not None:
            lines.append(f"    number: {hu['number']}  # number of bands")
    return lines


def _project_yaml(**cfg) -> str:
    fc = cfg["forcing"]
    lines = [
        "# hydrobricks project file, generated by 'hydrobricks init'.",
        "# Edit freely: https://hydrobricks.readthedocs.io",
        "",
        "model:",
        f"  name: {cfg['model_name']}",
        *cfg["option_lines"],
        "",
        *_hydro_units_lines(cfg["hydro_units"], fc),
    ]
    lines.append("")

    if fc["kind"] == "station":
        lines += _station_forcing_lines(fc)
    else:
        lines += _gridded_forcing_lines(fc)
    lines.append("")

    if cfg["discharge"] is not None:
        file, time_col, time_fmt, column = cfg["discharge"]
        lines += [
            "observations:",
            f"  file: {_yaml_str(file.as_posix())}",
            f"  time: {{column: {_yaml_str(time_col)}, "
            f"format: {_yaml_str(time_fmt)}}}",
            f"  column: {_yaml_str(column)}",
            "",
        ]

    lines.append("periods:")
    if cfg["calibration_end"]:
        cal_end = pd.Timestamp(cfg["calibration_end"])
        val_start = (cal_end + pd.Timedelta(days=1)).strftime("%Y-%m-%d")
        lines += [
            f"  calibration: [{cfg['start']}, {cal_end.strftime('%Y-%m-%d')}]",
            f"  validation: [{val_start}, {cfg['end']}]",
        ]
    else:
        lines.append(f"  simulation: [{cfg['start']}, {cfg['end']}]")
    lines += [
        f"  spinup: {_yaml_str(cfg['spinup'])}",
        "",
        f"output: {_yaml_str(cfg['output'])}",
        "",
        "parameters:",
        "  # Uncomment and set a value for each parameter, e.g. 'a_snow: 3'.",
    ]
    lines += _parameter_lines(cfg["model_name"], cfg["model_options"])
    lines.append("")
    return "\n".join(lines)


if __name__ == "__main__":
    sys.exit(main())
