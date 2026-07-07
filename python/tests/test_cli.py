import os.path
from pathlib import Path

import pytest

import hydrobricks as hb
from hydrobricks.cli import (
    _detect_time_format,
    _guess_column,
    _yaml_str,
    main,
)

TEST_FILES_DIR = Path(
    os.path.dirname(os.path.realpath(__file__)),
    "..",
    "..",
    "tests",
    "files",
    "catchments",
)
SITTER_DIR = TEST_FILES_DIR / "ch_sitter_appenzell"


def write_project_file(tmp_path, with_parameters=True):
    parameters = ""
    if with_parameters:
        parameters = """
parameters:
  A: 200
  a_snow: 3
  k_slow_1: 0.01
  k_slow_2: 0.001
  percol: 1
  k_quick: 0.05
"""
    project_file = tmp_path / "project.yaml"
    project_file.write_text(
        f"""
model:
  name: socont
  options: {{soil_storage_nb: 2, surface_runoff: linear_storage}}
hydro_units:
  file: {(SITTER_DIR / 'hydro_units_elevation.csv').as_posix()}
forcing:
  file: {(SITTER_DIR / 'meteo.csv').as_posix()}
  time: {{column: date, format: "%d/%m/%Y"}}
  columns:
    precipitation: precip(mm/day)
    temperature: temp(C)
    pet: pet_sim(mm/day)
  ref_elevation: 1250
observations:
  file: {(SITTER_DIR / 'discharge.csv').as_posix()}
  time: {{column: Date, format: "%d/%m/%Y"}}
  column: Discharge (mm/d)
periods:
  calibration: [1981-01-01, 1981-12-31]
  validation: [1982-01-01, 1982-12-31]
  spinup: 30
output: {(tmp_path / 'output').as_posix()}
{parameters}""",
        encoding="utf-8",
    )
    return project_file


def test_guess_column():
    columns = ["date", "precip(mm/day)", "temp(C)", "pet_sim(mm/day)"]
    assert _guess_column(columns, "time") == "date"
    assert _guess_column(columns, "precipitation") == "precip(mm/day)"
    assert _guess_column(columns, "temperature") == "temp(C)"
    assert _guess_column(columns, "pet") == "pet_sim(mm/day)"
    assert _guess_column(["Date", "Discharge (mm/d)"], "discharge") == (
        "Discharge (mm/d)"
    )
    assert _guess_column(["a", "b"], "temperature") is None


def test_guess_column_single_letter():
    assert _guess_column(["day", "P", "T"], "precipitation") == "P"


def test_detect_time_format():
    assert _detect_time_format(["1981-01-31", "1981-02-01"]) == "%Y-%m-%d"
    assert _detect_time_format(["31/01/1981", "01/02/1981"]) == "%d/%m/%Y"
    # A day > 12 disambiguates month-first from day-first.
    assert _detect_time_format(["01/31/1981"]) == "%m/%d/%Y"
    assert _detect_time_format(["31.01.1981"]) == "%d.%m.%Y"
    assert _detect_time_format(["not a date"]) is None


def test_yaml_str_quoting():
    assert _yaml_str("precip(mm/day)") == "precip(mm/day)"
    assert _yaml_str("%d/%m/%Y") == '"%d/%m/%Y"'
    assert _yaml_str("Discharge (mm/d)") == "Discharge (mm/d)"
    assert _yaml_str("no") == '"no"'  # YAML 1.1 boolean
    assert _yaml_str("1250") == '"1250"'  # would parse as a number
    assert _yaml_str("2y") == "2y"


def test_cli_validate(tmp_path, capsys):
    project_file = write_project_file(tmp_path)
    assert main(["validate", str(project_file)]) == 0
    out = capsys.readouterr().out
    assert "is valid" in out
    assert "socont" in out


def test_cli_validate_reports_missing_parameters(tmp_path, capsys):
    project_file = write_project_file(tmp_path, with_parameters=False)
    assert main(["validate", str(project_file)]) == 0
    assert "still need a value" in capsys.readouterr().out


def test_cli_validate_invalid_project(tmp_path, capsys):
    project_file = tmp_path / "project.yaml"
    project_file.write_text("model:\n  name: nope\n", encoding="utf-8")
    assert main(["validate", str(project_file)]) == 1
    assert "unknown model" in capsys.readouterr().err


def test_cli_run(tmp_path, capsys):
    project_file = write_project_file(tmp_path)
    assert main(["run", str(project_file)]) == 0
    out = capsys.readouterr().out
    assert "Mean simulated discharge" in out
    assert "calibration" in out and "validation" in out
    assert (tmp_path / "output" / "simulated_discharge.csv").is_file()
    assert (tmp_path / "output" / "results.nc").is_file()


def test_cli_init_wizard(tmp_path, monkeypatch, capsys):
    """The wizard, fed scripted answers, writes a valid loadable project file."""
    target = tmp_path / "project.yaml"
    answers = iter(
        [
            "",  # model -> socont
            "",  # hydro units source -> csv
            str(SITTER_DIR / "hydro_units_elevation.csv"),  # hydro units CSV
            "",  # elevation column -> elevation
            "",  # area column -> area
            "",  # forcing source -> station
            str(SITTER_DIR / "meteo.csv"),  # meteo CSV
            "",  # time column -> date
            "",  # time format -> detected %d/%m/%Y
            "",  # precipitation column -> precip(mm/day)
            "",  # temperature column -> temp(C)
            "",  # pet column -> pet_sim(mm/day)
            "1250",  # reference elevation
            str(SITTER_DIR / "discharge.csv"),  # discharge CSV
            "",  # time column -> Date
            "",  # time format -> detected %d/%m/%Y
            "",  # discharge column -> Discharge (mm/d)
            "1981-01-01",  # simulation start
            "1982-12-31",  # simulation end
            "1981-12-31",  # calibration end -> split
            "",  # spinup -> 2y
            (tmp_path / "output").as_posix(),  # output directory
        ]
    )
    monkeypatch.setattr("builtins.input", lambda _: next(answers))

    assert main(["init", str(target)]) == 0
    assert "Wrote" in capsys.readouterr().out

    # The generated file is valid and fully wired (parameters still unset).
    project = hb.load_project(target)
    assert project.model.name == "socont"
    assert project.observations is not None
    assert project.periods.calibration.bounds == ("1981-01-01", "1981-12-31")
    assert project.periods.validation.bounds == ("1982-01-01", "1982-12-31")
    assert not project.parameters.is_valid()  # placeholders are commented

    # Without a slope column, the wizard falls back to a linear storage
    # (the kinematic-wave default needs the hydro units' slope).
    text = target.read_text(encoding="utf-8")
    assert "surface_runoff: linear_storage" in text

    # The parameter placeholders list names and ranges, matching the options.
    assert "# a_snow:" in text
    assert "# k_quick:" in text
    assert "range [" in text


def test_cli_init_refuses_existing_file(tmp_path, monkeypatch):
    target = tmp_path / "project.yaml"
    target.write_text("model: {}\n", encoding="utf-8")
    answers = iter(["n"])
    monkeypatch.setattr("builtins.input", lambda _: next(answers))
    assert main(["init", str(target)]) == 1
    assert target.read_text(encoding="utf-8") == "model: {}\n"


def test_cli_unknown_project_file(capsys):
    assert main(["validate", "does_not_exist.yaml"]) == 1
    assert "does not exist" in capsys.readouterr().err


needs_gridded_packages = pytest.mark.skipif(
    not (hb.HAS_XARRAY and hb.HAS_RIOXARRAY and hb.HAS_RASTERIO and hb.HAS_NETCDF),
    reason="Gridded forcing needs xarray, rioxarray, rasterio and netCDF4",
)


@needs_gridded_packages
def test_cli_init_wizard_gridded(tmp_path, monkeypatch, capsys):
    """The gridded branch sniffs the netCDF and writes a loadable project."""
    target = tmp_path / "project.yaml"
    grid = str(SITTER_DIR / "gridded_precip.nc")
    answers = iter(
        [
            "",  # model -> socont
            "",  # hydro units source -> csv
            str(SITTER_DIR / "hydro_units_elevation.csv"),  # hydro units CSV
            "",  # elevation column
            "",  # area column
            "gridded",  # forcing source
            str(SITTER_DIR / "unit_ids.tif"),  # unit ids raster
            "",  # outline -> skip (no elevation gradients)
            grid,  # precipitation netCDF
            "",  # variable name -> RhiresD (sniffed)
            "",  # time dimension -> time
            "",  # x dimension -> E
            "",  # y dimension -> N
            "2056",  # CRS
            grid,  # temperature netCDF (same file, for the test)
            "",  # variable name -> RhiresD
            "",  # time dimension
            "",  # x dimension
            "",  # y dimension
            "2056",  # CRS
            "",  # PET netCDF -> compute from temperature
            "47.3",  # latitude
            "",  # discharge CSV -> skip
            "",  # simulation start -> 1962-01-01 (sniffed)
            "",  # simulation end -> 1962-01-03 (sniffed)
            "",  # calibration end -> no split
            "0",  # spinup
            (tmp_path / "output").as_posix(),  # output directory
        ]
    )
    monkeypatch.setattr("builtins.input", lambda _: next(answers))

    assert main(["init", str(target)]) == 0
    text = target.read_text(encoding="utf-8")
    assert "gridded:" in text
    assert "var_name: RhiresD" in text
    assert "crs: 2056" in text
    assert "dim_x: E" in text
    assert "simulation: [1962-01-01, 1962-01-03]" in text

    project = hb.load_project(target)
    assert project.periods.simulation.bounds == ("1962-01-01", "1962-01-03")


needs_catchment_packages = pytest.mark.skipif(
    not (hb.HAS_GEOPANDAS and hb.HAS_SHAPELY and hb.HAS_RASTERIO and hb.HAS_PYPROJ),
    reason="Catchment delineation needs geopandas, shapely, rasterio and pyproj",
)


@needs_catchment_packages
def test_cli_init_wizard_dem(tmp_path, monkeypatch, capsys):
    """The DEM branch writes a discretization section that loads and delineates."""
    target = tmp_path / "project.yaml"
    answers = iter(
        [
            "",  # model -> socont
            "dem",  # hydro units source
            str(SITTER_DIR / "outline.shp"),  # outline
            str(SITTER_DIR / "dem.tif"),  # DEM
            "",  # method -> equal_intervals
            "",  # band height -> 100
            "",  # forcing source -> station
            str(SITTER_DIR / "meteo.csv"),  # meteo CSV
            "",  # time column -> date
            "",  # time format -> detected
            "",  # precipitation column
            "",  # temperature column
            "",  # pet column -> pet_sim(mm/day)
            "1250",  # reference elevation
            "",  # discharge CSV -> skip
            "1981-01-01",  # simulation start
            "1981-12-31",  # simulation end
            "",  # calibration end -> no split
            "0",  # spinup
            (tmp_path / "output").as_posix(),  # output directory
        ]
    )
    monkeypatch.setattr("builtins.input", lambda _: next(answers))

    assert main(["init", str(target)]) == 0
    text = target.read_text(encoding="utf-8")
    assert "discretization:" in text
    assert "method: equal_intervals" in text
    assert "distance: 100" in text
    # In DEM mode the slope is computed, so Socont keeps its default runoff.
    assert "surface_runoff: linear_storage" not in text
    assert "# beta:" in text  # placeholders match the kinematic-wave option

    project = hb.load_project(target)
    assert len(project.forcing.hydro_units) > 5
