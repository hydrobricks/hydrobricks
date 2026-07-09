import os.path
from pathlib import Path

import numpy as np
import pytest

import hydrobricks as hb

TEST_FILES_DIR = Path(
    os.path.dirname(os.path.realpath(__file__)),
    "..",
    "..",
    "tests",
    "files",
    "catchments",
)
SITTER_DIR = TEST_FILES_DIR / "ch_sitter_appenzell"

PARAMETERS = {
    "A": 200,
    "a_snow": 3,
    "k_slow_1": 0.01,
    "k_slow_2": 0.001,
    "percol": 1,
    "k_quick": 0.05,
}


def minimal_config(tmp_path):
    """A valid minimal project configuration using the Sitter test data."""
    return {
        "model": {
            "name": "socont",
            "options": {"soil_storage_nb": 2, "surface_runoff": "linear_storage"},
        },
        "hydro_units": {"file": "hydro_units_elevation.csv"},
        "forcing": {
            "file": "meteo.csv",
            "time": {"column": "date", "format": "%d/%m/%Y"},
            "columns": {
                "precipitation": "precip(mm/day)",
                "temperature": "temp(C)",
                "pet": "pet_sim(mm/day)",
            },
            "ref_elevation": 1250,
        },
        "periods": ["1981-01-01", "1981-12-31"],
        "output": str(tmp_path),
        "parameters": dict(PARAMETERS),
    }


def test_load_project_from_yaml_end_to_end(tmp_path):
    """A YAML project file builds a model that runs and evaluates per period."""
    project_file = tmp_path / "project.yaml"
    project_file.write_text(
        f"""
model:
  name: socont
  options:
    soil_storage_nb: 2
    surface_runoff: linear_storage

hydro_units:
  file: hydro_units_elevation.csv

forcing:
  file: meteo.csv
  time: {{column: date, format: "%d/%m/%Y"}}
  columns:
    precipitation: precip(mm/day)
    temperature: temp(C)
    pet: pet_sim(mm/day)
  ref_elevation: 1250
  precipitation: {{correction_factor: 0.9}}

observations:
  file: discharge.csv
  time: {{column: Date, format: "%d/%m/%Y"}}
  column: Discharge (mm/d)

periods:
  calibration: [1981-01-01, 1981-12-31]
  validation: [1982-01-01, 1982-12-31]
  spinup: 60

output: {(tmp_path / "output").as_posix()}

parameters:
  A: 200
  a_snow: 3
  k_slow_1: 0.01
  k_slow_2: 0.001
  percol: 1
  k_quick: 0.05
""",
        encoding="utf-8",
    )

    project = hb.load_project(project_file, base_dir=SITTER_DIR)

    assert isinstance(project, hb.Project)
    assert project.observations is not None
    assert project.parameters.is_valid()
    assert project.model.start_date == "1981-01-01"
    assert project.model.end_date == "1982-12-31"
    assert project.model.spinup_days == 60

    discharge = project.run()
    assert len(discharge) > 0
    assert np.all(discharge.to_numpy() >= 0)

    scores = hb.evaluate_periods(
        project.model, project.observations, project.periods, metrics=("nse",)
    )
    assert set(scores.index) == {"calibration", "validation", "simulation"}
    assert np.isfinite(scores["nse"]).all()


def test_load_project_from_dict(tmp_path):
    """A plain dict works as source; observations is None when not declared."""
    project = hb.load_project(minimal_config(tmp_path), base_dir=SITTER_DIR)
    assert project.observations is None
    assert project.periods.simulation.bounds == ("1981-01-01", "1981-12-31")
    discharge = project.run()
    assert len(discharge) > 0


def test_validation_collects_all_errors(tmp_path):
    """All problems are reported together, with suggestions for typos."""
    config = minimal_config(tmp_path)
    # A typo in a section name:
    config["modle"] = config.pop("model")  # codespell:ignore modle
    config["forcing"]["columns"]["temperature"] = "temp"  # Wrong column.
    del config["periods"]

    with pytest.raises(hb.ConfigurationError) as excinfo:
        hb.load_project(config, base_dir=SITTER_DIR)
    message = str(excinfo.value)
    assert "did you mean 'model'?" in message
    assert "column 'temp' not found in 'meteo.csv'" in message
    assert "did you mean 'temp(C)'?" in message
    assert "periods: this section is required" in message


def test_unknown_model_name(tmp_path):
    config = minimal_config(tmp_path)
    config["model"]["name"] = "socnt"
    with pytest.raises(hb.ConfigurationError) as excinfo:
        hb.load_project(config, base_dir=SITTER_DIR)
    message = str(excinfo.value)
    assert "unknown model 'socnt'" in message
    assert "did you mean 'socont'?" in message
    assert "gr4j" in message  # The available models are listed.


def test_unknown_parameter_name(tmp_path):
    config = minimal_config(tmp_path)
    config["parameters"]["a_sno"] = config["parameters"].pop("a_snow")
    with pytest.raises(hb.ConfigurationError) as excinfo:
        hb.load_project(config, base_dir=SITTER_DIR)
    message = str(excinfo.value)
    assert "parameters.a_sno: unknown parameter" in message
    assert "did you mean 'a_snow'?" in message


def test_missing_file_and_pet_latitude(tmp_path):
    config = minimal_config(tmp_path)
    config["hydro_units"]["file"] = "does_not_exist.csv"
    del config["forcing"]["columns"]["pet"]  # PET must be computed...
    # ...but no latitude is given.
    with pytest.raises(hb.ConfigurationError) as excinfo:
        hb.load_project(config, base_dir=SITTER_DIR)
    message = str(excinfo.value)
    assert "does_not_exist.csv" in message
    assert "forcing.pet.latitude" in message


def test_period_not_covered_by_forcing(tmp_path):
    config = minimal_config(tmp_path)
    config["periods"] = ["1981-01-01", "2049-12-31"]
    with pytest.raises(hb.ConfigurationError) as excinfo:
        hb.load_project(config, base_dir=SITTER_DIR)
    assert "not covered by the forcing data" in str(excinfo.value)


def test_run_lists_undefined_parameters(tmp_path):
    config = minimal_config(tmp_path)
    del config["parameters"]
    project = hb.load_project(config, base_dir=SITTER_DIR)
    with pytest.raises(hb.ConfigurationError) as excinfo:
        project.run()
    message = str(excinfo.value)
    assert "a_snow" in message
    assert "[" in message  # The valid ranges are listed.


# --- Gridded forcing ----------------------------------------------------------

needs_gridded_packages = pytest.mark.skipif(
    not (hb.HAS_XARRAY and hb.HAS_RIOXARRAY and hb.HAS_RASTERIO and hb.HAS_NETCDF),
    reason="Gridded forcing needs xarray, rioxarray, rasterio and netCDF4",
)

GRIDDED_SOURCE = {
    "path": "gridded_precip.nc",  # 3 days (1962-01-01..03) of RhiresD
    "var_name": "RhiresD",
    "crs": 2056,
    "dim_x": "E",
    "dim_y": "N",
}


def gridded_config(tmp_path):
    """A pure-gridded project configuration using the Sitter test data.

    The same 3-day precipitation grid feeds all three variables; this is
    physically meaningless but mechanically exercises the whole gridded path.
    """
    return {
        "model": {
            "name": "socont",
            "options": {"soil_storage_nb": 2, "surface_runoff": "linear_storage"},
        },
        "hydro_units": {
            "file": "hydro_units_elevation.csv",
            "unit_ids_raster": "unit_ids.tif",
        },
        "forcing": {
            "gridded": {
                "precipitation": dict(GRIDDED_SOURCE),
                "temperature": dict(GRIDDED_SOURCE),
                "pet": dict(GRIDDED_SOURCE),
            }
        },
        "periods": ["1962-01-01", "1962-01-03"],
        "output": str(tmp_path),
        "parameters": dict(PARAMETERS),
    }


@needs_gridded_packages
def test_gridded_forcing_end_to_end(tmp_path):
    """A pure-gridded project loads, regrids per hydro unit and runs."""
    project = hb.load_project(gridded_config(tmp_path), base_dir=SITTER_DIR)
    discharge = project.run()
    assert len(discharge) == 3
    assert np.all(discharge.to_numpy() >= 0)


@needs_gridded_packages
def test_gridded_with_elevation_gradient(tmp_path):
    """With outline+dem a Catchment is built and data gradients are applied."""
    if not (hb.HAS_GEOPANDAS and hb.HAS_SHAPELY):
        pytest.skip("Needs geopandas and shapely for the Catchment")
    config = gridded_config(tmp_path)
    config["hydro_units"]["outline"] = "outline.shp"
    config["hydro_units"]["dem"] = "dem.tif"
    config["forcing"]["gridded"]["precipitation"]["elevation_gradient"] = True

    project = hb.load_project(config, base_dir=SITTER_DIR)
    project.forcing.apply_operations()
    assert len(project.forcing.data2D.data) >= 1
    assert project.forcing.data2D.data[0].shape[0] == 3


@needs_gridded_packages
def test_gridded_forcing_cache_populated_and_reused(tmp_path):
    """The project wires <output>/cache into the forcing; a reload reuses it."""
    project = hb.load_project(gridded_config(tmp_path), base_dir=SITTER_DIR)
    assert project.forcing.cache_dir == Path(tmp_path) / "cache"
    discharge = project.run()

    # All three variables read the same source with the same options, so they
    # share a single cache entry (the key covers source + options, not the
    # target variable).
    cache_files = sorted((Path(tmp_path) / "cache").glob("forcing_regrid_*.csv"))
    assert len(cache_files) == 1

    project2 = hb.load_project(gridded_config(tmp_path), base_dir=SITTER_DIR)
    discharge2 = project2.run()
    cache_files2 = sorted((Path(tmp_path) / "cache").glob("forcing_regrid_*.csv"))
    assert cache_files2 == cache_files
    assert np.allclose(discharge.to_numpy(), discharge2.to_numpy())


def test_mixed_station_and_gridded(tmp_path):
    """Station and gridded sources can be mixed (one source per variable)."""
    config = minimal_config(tmp_path)
    del config["forcing"]["columns"]["precipitation"]
    config["hydro_units"]["unit_ids_raster"] = "unit_ids.tif"
    config["forcing"]["gridded"] = {"precipitation": dict(GRIDDED_SOURCE)}
    project = hb.load_project(config, base_dir=SITTER_DIR)
    assert project.forcing is not None


def test_variable_in_both_station_and_gridded(tmp_path):
    config = minimal_config(tmp_path)
    config["hydro_units"]["unit_ids_raster"] = "unit_ids.tif"
    config["forcing"]["gridded"] = {"precipitation": dict(GRIDDED_SOURCE)}
    with pytest.raises(hb.ConfigurationError) as excinfo:
        hb.load_project(config, base_dir=SITTER_DIR)
    assert "both in 'columns' (station) and in 'gridded'" in str(excinfo.value)


def test_gridded_requires_unit_ids_raster(tmp_path):
    config = gridded_config(tmp_path)
    del config["hydro_units"]["unit_ids_raster"]
    with pytest.raises(hb.ConfigurationError) as excinfo:
        hb.load_project(config, base_dir=SITTER_DIR)
    assert "hydro_units.unit_ids_raster: required with gridded forcing" in str(
        excinfo.value
    )


def test_gridded_elevation_gradient_requires_dem(tmp_path):
    config = gridded_config(tmp_path)
    config["forcing"]["gridded"]["temperature"]["elevation_gradient"] = True
    with pytest.raises(hb.ConfigurationError) as excinfo:
        hb.load_project(config, base_dir=SITTER_DIR)
    assert "elevation_gradient: requires 'outline' and 'dem'" in str(excinfo.value)


def test_gridded_validation_collects_errors(tmp_path):
    config = gridded_config(tmp_path)
    del config["forcing"]["gridded"]["precipitation"]["var_name"]
    config["forcing"]["gridded"]["temperature"]["path"] = "missing.nc"
    config["forcing"]["gridded"]["pet"]["dim_xx"] = "E"  # Typo.
    with pytest.raises(hb.ConfigurationError) as excinfo:
        hb.load_project(config, base_dir=SITTER_DIR)
    message = str(excinfo.value)
    assert "precipitation.var_name: the netCDF variable name is required" in message
    assert "missing.nc" in message
    assert "did you mean 'dim_x'?" in message


# --- DEM-based discretization -------------------------------------------------

needs_catchment_packages = pytest.mark.skipif(
    not (hb.HAS_GEOPANDAS and hb.HAS_SHAPELY and hb.HAS_RASTERIO and hb.HAS_PYPROJ),
    reason="Catchment delineation needs geopandas, shapely, rasterio and pyproj",
)


@needs_catchment_packages
def test_discretization_from_dem_end_to_end(tmp_path):
    """Hydro units delineated from the DEM (no CSV) build and run."""
    config = minimal_config(tmp_path)
    config["hydro_units"] = {
        "outline": "outline.shp",
        "dem": "dem.tif",
        "discretization": {"method": "equal_intervals", "distance": 100},
    }
    project = hb.load_project(config, base_dir=SITTER_DIR)
    assert len(project.forcing.hydro_units) > 5
    discharge = project.run()
    assert len(discharge) > 0
    assert np.all(discharge.to_numpy() >= 0)


@needs_catchment_packages
@needs_gridded_packages
def test_discretization_with_gridded_forcing(tmp_path):
    """With gridded forcing the unit ids raster is generated automatically."""
    config = gridded_config(tmp_path)
    config["hydro_units"] = {
        "outline": "outline.shp",
        "dem": "dem.tif",
        "discretization": {"method": "quantiles", "number": 10},
    }
    project = hb.load_project(config, base_dir=SITTER_DIR)
    assert (tmp_path / "unit_ids.tif").is_file()
    discharge = project.run()
    assert len(discharge) == 3


def test_discretization_requires_outline_and_dem(tmp_path):
    config = minimal_config(tmp_path)
    config["hydro_units"] = {"discretization": {"method": "equal_intervals"}}
    with pytest.raises(hb.ConfigurationError) as excinfo:
        hb.load_project(config, base_dir=SITTER_DIR)
    assert "requires 'outline' and 'dem'" in str(excinfo.value)


def test_discretization_conflicts(tmp_path):
    """CSV-only options are rejected in discretization mode, and vice versa."""
    config = minimal_config(tmp_path)
    config["hydro_units"].update(
        outline="outline.shp",
        dem="dem.tif",
        unit_ids_raster="unit_ids.tif",
        columns={"elevation": "elevation"},
        discretization={"method": "quantiles", "number": 10},
    )
    with pytest.raises(hb.ConfigurationError) as excinfo:
        hb.load_project(config, base_dir=SITTER_DIR)
    message = str(excinfo.value)
    assert "'file' and 'discretization' are mutually exclusive" in message
    assert "hydro_units.columns: only applies" in message
    assert "unit_ids_raster: not needed with 'discretization'" in message


def test_hydro_units_require_file_or_discretization(tmp_path):
    config = minimal_config(tmp_path)
    config["hydro_units"] = {}
    with pytest.raises(hb.ConfigurationError) as excinfo:
        hb.load_project(config, base_dir=SITTER_DIR)
    assert "provide either 'file'" in str(excinfo.value)


def test_discretization_unknown_method(tmp_path):
    config = minimal_config(tmp_path)
    config["hydro_units"] = {
        "outline": "outline.shp",
        "dem": "dem.tif",
        "discretization": {"method": "equal_intervalls"},
    }
    with pytest.raises(hb.ConfigurationError) as excinfo:
        hb.load_project(config, base_dir=SITTER_DIR)
    assert "'equal_intervals' or 'quantiles'" in str(excinfo.value)


# --- Data parameters and deferred setup ----------------------------------------


def test_data_parameters_and_param_refs(tmp_path):
    """'param:' forcing references resolve against the data_parameters section."""
    config = minimal_config(tmp_path)
    config["forcing"]["precipitation"] = {"correction_factor": "param:corr"}
    config["forcing"]["temperature"] = {"gradient": "param:temp_gradients"}
    config["data_parameters"] = {
        "corr": {"value": 0.9, "min": 0.7, "max": 1.3},
        "temp_gradients": -0.6,  # scalar shorthand (no bounds)
    }
    project = hb.load_project(config, base_dir=SITTER_DIR)
    assert project.parameters.get("corr") == 0.9
    assert project.parameters.get("temp_gradients") == -0.6
    discharge = project.run()
    assert len(discharge) > 0


def test_param_ref_without_data_parameter(tmp_path):
    config = minimal_config(tmp_path)
    config["forcing"]["precipitation"] = {"gradient": "param:precip_gradient"}
    with pytest.raises(hb.ConfigurationError) as excinfo:
        hb.load_project(config, base_dir=SITTER_DIR)
    assert "'param:precip_gradient' has no matching entry" in str(excinfo.value)


def test_invalid_param_ref_syntax(tmp_path):
    config = minimal_config(tmp_path)
    config["forcing"]["precipitation"] = {
        "gradient": "parma:typo"  # codespell:ignore parma
    }
    with pytest.raises(hb.ConfigurationError) as excinfo:
        hb.load_project(config, base_dir=SITTER_DIR)
    assert "expected a number or a 'param:<name>' reference" in str(excinfo.value)


def test_deferred_setup(tmp_path):
    """With setup=False the model is set up later, over a chosen period."""
    config = minimal_config(tmp_path)
    config["periods"] = {
        "calibration": ["1981-01-01", "1981-12-31"],
        "validation": ["1982-01-01", "1982-12-31"],
        "spinup": 30,
    }
    project = hb.load_project(config, base_dir=SITTER_DIR, setup=False)

    # Running without setup fails with the model's own clear error.
    with pytest.raises(hb.ModelError):
        project.run()

    # Set up over the calibration period only (spin-up clamped to it).
    project.setup(period="calibration")
    assert project.model.start_date == "1981-01-01"
    assert project.model.end_date == "1981-12-31"
    assert project.model.spinup_days == 30
    discharge = project.run()
    assert len(discharge) == 365

    # An unknown period name is rejected with the declared names listed.
    with pytest.raises(hb.ConfigurationError) as excinfo:
        project.setup(period="valdiation")
    assert "Unknown period 'valdiation'" in str(excinfo.value)
