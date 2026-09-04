import os.path
from pathlib import Path

import numpy as np
import pandas as pd
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
    # ...but no lat is given.
    with pytest.raises(hb.ConfigurationError) as excinfo:
        hb.load_project(config, base_dir=SITTER_DIR)
    message = str(excinfo.value)
    assert "does_not_exist.csv" in message
    assert "forcing.pet.lat" in message


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


# --- Calibration section ------------------------------------------------------


def calibration_config(tmp_path):
    """A calibratable project configuration using the Sitter test data."""
    config = minimal_config(tmp_path)
    config["observations"] = {
        "file": "discharge.csv",
        "time": {"column": "Date", "format": "%d/%m/%Y"},
        "column": "Discharge (mm/d)",
    }
    config["periods"] = {
        "calibration": ["1981-01-01", "1981-12-31"],
        "validation": ["1982-01-01", "1982-12-31"],
        "spinup": 60,
    }
    config["calibration"] = {
        "algorithm": "mc",
        "repetitions": 3,
        "objective": "nse",
        "parameters": ["a_snow", "A"],
    }
    return config


def test_calibration_section_validation(tmp_path):
    config = calibration_config(tmp_path)
    config["calibration"]["max_rep"] = 300  # Unknown key.
    config["calibration"]["repetitions"] = -1
    config["calibration"]["transform"] = "power(oops)"
    with pytest.raises(hb.ConfigurationError) as excinfo:
        hb.load_project(config, base_dir=SITTER_DIR)
    message = str(excinfo.value)
    assert "calibration: unknown key 'max_rep'" in message
    assert "calibration.repetitions: expected a positive integer" in message
    assert "calibration.transform:" in message


def test_calibration_requires_observations_and_period(tmp_path):
    config = calibration_config(tmp_path)
    del config["observations"]
    config["periods"] = ["1981-01-01", "1982-12-31"]  # No calibration period.
    with pytest.raises(hb.ConfigurationError) as excinfo:
        hb.load_project(config, base_dir=SITTER_DIR)
    message = str(excinfo.value)
    assert "'observations' section is required" in message
    assert "declare a calibration period" in message


def test_calibration_unknown_parameter_name(tmp_path):
    config = calibration_config(tmp_path)
    config["calibration"]["parameters"] = ["a_sno"]
    with pytest.raises(hb.ConfigurationError) as excinfo:
        hb.load_project(config, base_dir=SITTER_DIR)
    message = str(excinfo.value)
    assert "calibration.parameters.a_sno: unknown parameter" in message
    assert "did you mean 'a_snow'?" in message


def test_project_calibrate_smoke(tmp_path):
    """calibrate() optimizes on the calibration period and applies the best set."""
    project = hb.load_project(calibration_config(tmp_path), base_dir=SITTER_DIR)
    result = project.calibrate()

    assert {"score", "parameters", "index", "sampler", "algorithm"} <= set(result)
    assert result["algorithm"] == "mc"
    assert np.isfinite(result["score"])
    assert set(result["parameters"]) == {"a_snow", "A"}

    # The best values were applied; the project still runs over the full span.
    discharge = project.run()
    assert len(discharge) == 730

    scores = hb.evaluate_periods(
        project.model, project.observations, project.periods, metrics=("nse",)
    )
    assert np.isfinite(scores["nse"]).all()


def test_project_calibrate_overrides_and_errors(tmp_path):
    config = calibration_config(tmp_path)
    del config["calibration"]
    project = hb.load_project(config, base_dir=SITTER_DIR)

    with pytest.raises(hb.ConfigurationError) as excinfo:
        project.calibrate()
    assert "repetitions" in str(excinfo.value)

    with pytest.raises(hb.ConfigurationError) as excinfo:
        project.calibrate(repetitions=2)
    assert "parameters to calibrate" in str(excinfo.value)

    result = project.calibrate(
        algorithm="mc", repetitions=2, objective="nse", parameters=["a_snow"]
    )
    assert set(result["parameters"]) == {"a_snow"}


def test_project_cache_key(tmp_path):
    """The optional cache key overrides the default <output>/cache location."""
    config = minimal_config(tmp_path)
    config["cache"] = str(tmp_path / "shared_cache")
    project = hb.load_project(config, base_dir=SITTER_DIR)
    assert project.forcing.cache_dir == tmp_path / "shared_cache"

    project2 = hb.load_project(minimal_config(tmp_path), base_dir=SITTER_DIR)
    assert project2.forcing.cache_dir == tmp_path / "cache"


# --- Gridded forcing ----------------------------------------------------------

needs_gridded_packages = pytest.mark.skipif(
    not (hb.HAS_XARRAY and hb.HAS_RIOXARRAY and hb.HAS_RASTERIO and hb.HAS_NETCDF),
    reason="Gridded forcing needs xarray, rioxarray, rasterio and netCDF4",
)

GRIDDED_SOURCE = {
    "path": "gridded_precip.nc",  # 3 days (1962-01-01..03) of RhiresD
    "var_name": "RhiresD",
    "data_crs": 2056,
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
    config["forcing"]["gridded"]["precipitation"]["apply_data_gradient"] = True

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
    config["forcing"]["gridded"]["temperature"]["apply_data_gradient"] = True
    with pytest.raises(hb.ConfigurationError) as excinfo:
        hb.load_project(config, base_dir=SITTER_DIR)
    assert "apply_data_gradient: requires 'outline' and 'dem'" in str(excinfo.value)


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


@needs_catchment_packages
@pytest.mark.skipif(not hb.HAS_SCIPY, reason="Splitting the units needs scipy")
def test_discretization_split_discontinuous(tmp_path):
    """The split of spatially discontinuous units is available from the YAML."""
    config = minimal_config(tmp_path)
    config["hydro_units"] = {
        "outline": "outline.shp",
        "dem": "dem.tif",
        "discretization": {"method": "equal_intervals", "distance": 100},
    }
    reference = hb.load_project(config, base_dir=SITTER_DIR)

    config["hydro_units"]["discretization"].update(
        split_discontinuous=True, min_patch_area=100000, connectivity=4
    )
    project = hb.load_project(config, base_dir=SITTER_DIR)

    assert len(project.forcing.hydro_units) > len(reference.forcing.hydro_units)
    discharge = project.run()
    assert len(discharge) > 0


def test_discretization_invalid_connectivity(tmp_path):
    config = minimal_config(tmp_path)
    config["hydro_units"] = {
        "outline": "outline.shp",
        "dem": "dem.tif",
        "discretization": {"method": "equal_intervals", "connectivity": 6},
    }
    with pytest.raises(hb.ConfigurationError) as excinfo:
        hb.load_project(config, base_dir=SITTER_DIR)
    assert "connectivity: expected 4 or 8" in str(excinfo.value)


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


# --- Land covers ---------------------------------------------------------------

GLETSCH_DIR = TEST_FILES_DIR / "ch_rhone_gletsch"


def glacier_config(tmp_path, land_cover_types, land_cover_names, **land_covers):
    """A Gletsch (glacierized) project delineated from the DEM."""
    return {
        "model": {
            "name": "socont",
            "options": {
                "soil_storage_nb": 2,
                "surface_runoff": "linear_storage",
                "land_cover_types": land_cover_types,
                "land_cover_names": land_cover_names,
            },
        },
        "hydro_units": {
            "outline": "outline.shp",
            "dem": "dem.tif",
            "discretization": {"method": "equal_intervals", "distance": 200},
            **({"land_covers": land_covers} if land_covers else {}),
        },
        "forcing": {
            "file": "meteo.csv",
            "time": {"column": "date", "format": "%d/%m/%Y"},
            "columns": {
                "precipitation": "precip(mm/day)",
                "temperature": "temp(C)",
                "pet": "pet_sim(mm/day)",
            },
            "ref_elevation": 2702,
        },
        "periods": ["1981-01-01", "1981-12-31"],
        "output": str(tmp_path),
        "parameters": {**PARAMETERS, "a_ice": 6, "k_snow": 0.5, "k_ice": 0.3},
    }


def cover_fractions(project, cover):
    """The land cover fractions of a project's hydro units."""
    return project.hydro_units.hydro_units[f"fraction-{cover}"].to_numpy().squeeze()


@needs_catchment_packages
def test_land_covers_glacier_from_outline(tmp_path):
    """A glacier outline initializes the glacier fractions of the hydro units."""
    config = glacier_config(
        tmp_path,
        ["ground", "glacier"],
        ["ground", "glacier"],
        glacier={"outline": "glaciers/sgi_2016.shp"},
    )
    project = hb.load_project(config, base_dir=GLETSCH_DIR)

    glacier = cover_fractions(project, "glacier")
    ground = cover_fractions(project, "ground")
    assert np.any(glacier > 0)  # Gletsch is glacierized
    assert np.all(glacier <= 1)
    assert np.allclose(glacier + ground, 1)
    discharge = project.run()
    assert np.all(discharge.to_numpy() >= 0)


@needs_catchment_packages
def test_land_covers_glacier_split_at_ela(tmp_path):
    """Two glacier covers are split at the equilibrium line (ice below, firn above)."""
    config = glacier_config(
        tmp_path,
        ["ground", "glacier", "glacier"],
        ["ground", "glacier_ice", "glacier_firn"],
        glacier={"outline": "glaciers/sgi_2016.shp", "ela": 2900},
    )
    config["model"]["name"] = "prevah_unibe"
    config["model"]["options"] = {
        key: value
        for key, value in config["model"]["options"].items()
        if key.startswith("land_cover")
    }
    config["parameters"] = {
        "a_snow_min": 1.5,
        "a_snow_max": 4.5,
        "fc": 300,
        "beta": 2,
        "k0": 0.5,
        "k1": 0.2,
        "k_gw1": 0.05,
        "k_gw2": 0.01,
        "k_gw3": 0.005,
        "lp": 0.9,
        "sgrluz": 10,
        "cperc": 0.1,
        "a_ice_min_glacier_ice": 2,
        "a_ice_max_glacier_ice": 6,
        "a_ice_min_glacier_firn": 1.5,
        "a_ice_max_glacier_firn": 4,
        "k_snow": 0.5,
        "k_ice": 0.3,
        "k_firn": 0.1,
    }
    project = hb.load_project(config, base_dir=GLETSCH_DIR)

    ice = cover_fractions(project, "glacier_ice")
    firn = cover_fractions(project, "glacier_firn")
    elevation = project.hydro_units.hydro_units["elevation"].to_numpy().squeeze()
    assert np.any(ice > 0) and np.any(firn > 0)
    # The split follows the ELA: no firn well below it, no bare ice well above.
    assert np.all(firn[elevation < 2700] == 0)
    assert np.all(ice[elevation > 3100] == 0)
    assert np.allclose(ice + firn + cover_fractions(project, "ground"), 1)
    assert np.all(project.run().to_numpy() >= 0)


@needs_catchment_packages
def test_land_covers_ignored_without_glacier_cover(tmp_path):
    """A model without a glacier cover keeps the glacierized area in the soil."""
    config = glacier_config(
        tmp_path, ["ground"], ["ground"], glacier={"outline": "glaciers/sgi_2016.shp"}
    )
    config["parameters"] = dict(PARAMETERS)
    project = hb.load_project(config, base_dir=GLETSCH_DIR)
    assert np.allclose(cover_fractions(project, "ground"), 1)


def test_land_covers_glacier_cover_without_source(tmp_path):
    config = glacier_config(tmp_path, ["ground", "glacier"], ["ground", "glacier"])
    with pytest.raises(hb.ConfigurationError) as excinfo:
        hb.load_project(config, base_dir=GLETSCH_DIR)
    assert "no glacier extent" in str(excinfo.value)


def test_land_covers_split_requires_ela(tmp_path):
    config = glacier_config(
        tmp_path,
        ["ground", "glacier", "glacier"],
        ["ground", "glacier_ice", "glacier_firn"],
        glacier={"outline": "glaciers/sgi_2016.shp"},
    )
    with pytest.raises(hb.ConfigurationError) as excinfo:
        hb.load_project(config, base_dir=GLETSCH_DIR)
    assert "land_covers.glacier.ela: required" in str(excinfo.value)


def test_land_covers_validation_errors(tmp_path):
    config = glacier_config(
        tmp_path,
        ["ground", "glacier"],
        ["ground", "glacier"],
        glacier={
            "outline": "glaciers/sgi_2016.shp",
            "ice_thickness": "glaciers/ice_thickness.tif",
        },
    )
    config["hydro_units"]["land_covers"]["forest"] = {"outline": "outline.shp"}
    config["model"]["options"]["land_cover_names"] = ["ground"]
    with pytest.raises(hb.ConfigurationError) as excinfo:
        hb.load_project(config, base_dir=GLETSCH_DIR)
    message = str(excinfo.value)
    assert "not both" in message
    assert "unsupported land cover source 'forest'" in message
    assert "must have the same length" in message


def test_land_covers_require_dem(tmp_path):
    """The cover fractions are computed on the DEM grid."""
    config = minimal_config(tmp_path)
    config["hydro_units"]["land_covers"] = {"glacier": {"outline": "outline.shp"}}
    with pytest.raises(hb.ConfigurationError) as excinfo:
        hb.load_project(config, base_dir=SITTER_DIR)
    assert "land_covers: requires 'outline' and 'dem'" in str(excinfo.value)


def test_columns_areas_must_match_the_model_covers(tmp_path):
    """Per-cover area columns are checked against the covers the model declares."""
    config = minimal_config(tmp_path)
    config["model"]["options"].update(
        land_cover_types=["ground", "glacier"], land_cover_names=["ground", "glacier"]
    )
    config["hydro_units"]["columns_areas"] = {"ground": "area"}
    with pytest.raises(hb.ConfigurationError) as excinfo:
        hb.load_project(config, base_dir=SITTER_DIR)
    assert "one area column per land cover" in str(excinfo.value)


# --- Lateral connectivity and actions -------------------------------------------


def snow_slide_config(tmp_path, **hydro_units):
    """A Gletsch project with the snow redistribution (needs slope + connectivity)."""
    return {
        "model": {
            "name": "socont",
            "options": {
                "soil_storage_nb": 2,
                "surface_runoff": "linear_storage",
                "snow_redistribution": "transport:snow_slide",
            },
        },
        "hydro_units": {
            "file": "hydro_units_elevation_radiation.csv",
            **hydro_units,
        },
        "forcing": {
            "file": "meteo.csv",
            "time": {"column": "date", "format": "%d/%m/%Y"},
            "columns": {
                "precipitation": "precip(mm/day)",
                "temperature": "temp(C)",
                "pet": "pet_sim(mm/day)",
            },
            "ref_elevation": 2702,
        },
        "periods": ["1981-01-01", "1981-12-31"],
        "output": str(tmp_path),
        "parameters": {
            **PARAMETERS,
            "snow_slide_coeff": 3178.4,
            "snow_slide_exp": -1.998,
        },
    }


def test_hydro_units_carry_the_slope_of_a_csv(tmp_path):
    """The slope column is loaded without being asked for (lateral processes)."""
    config = snow_slide_config(
        tmp_path, connectivity="connectivity_elevation_radiation.csv"
    )
    project = hb.load_project(config, base_dir=GLETSCH_DIR)
    assert "slope" in project.hydro_units.hydro_units.columns


def test_lateral_process_requires_the_connectivity(tmp_path):
    """Without the connectivity a lateral process would silently do nothing."""
    with pytest.raises(hb.ConfigurationError) as excinfo:
        hb.load_project(snow_slide_config(tmp_path), base_dir=GLETSCH_DIR)
    message = str(excinfo.value)
    assert "hydro_units.connectivity" in message
    assert "transport:snow_slide" in message


def test_connectivity_enables_the_snow_redistribution(tmp_path):
    """A connectivity table declared in the project file reaches the model."""
    config = snow_slide_config(
        tmp_path, connectivity="connectivity_elevation_radiation.csv"
    )
    project = hb.load_project(config, base_dir=GLETSCH_DIR)

    assert project.hydro_units.settings.get_lateral_connection_count() > 0
    discharge = project.run()
    assert np.all(discharge.to_numpy() >= 0)


def test_connectivity_file_must_exist(tmp_path):
    """A missing connectivity table is reported like any other missing file."""
    config = snow_slide_config(tmp_path, connectivity="no_such_connectivity.csv")
    with pytest.raises(hb.ConfigurationError) as excinfo:
        hb.load_project(config, base_dir=GLETSCH_DIR)
    assert "hydro_units.connectivity" in str(excinfo.value)


def glacier_evolution_config(tmp_path, lookup_tables, **action):
    """A Gletsch project replaying a precomputed glacier evolution."""
    config = glacier_config(
        tmp_path,
        ["ground", "glacier"],
        ["ground", "glacier"],
        glacier={"outline": "glaciers/sgi_2016.shp"},
    )
    # A glacier evolution tracks the ice volume, so the ice cannot be an
    # infinite storage (the Socont default).
    config["model"]["options"]["glacier_infinite_storage"] = False
    config["actions"] = {
        "glacier_evolution": {"lookup_tables": str(lookup_tables), **action}
    }
    return config


def write_lookup_tables(directory, unit_ids, initial_areas, n_increments=101):
    """A minimal (linearly shrinking) delta-h lookup table for the given units.

    The first row must hold the glacier area the model starts from, which the
    action checks against the initialized land cover fractions.
    """
    directory.mkdir(parents=True, exist_ok=True)
    shrinking = np.linspace(1.0, 0.0, n_increments)[:, None]
    areas = shrinking * np.asarray(initial_areas, dtype=float)
    for name, values in (
        ("glacier_evolution_lookup_table_area.csv", areas),
        # A 10 m thick glacier, melting with the area.
        ("glacier_evolution_lookup_table_volume.csv", areas * 10),
    ):
        table = pd.DataFrame(values, columns=[str(i) for i in unit_ids])
        table.to_csv(directory / name, index=False)


@needs_catchment_packages
def test_glacier_evolution_action_from_lookup_tables(tmp_path):
    """The 'actions' section attaches a glacier evolution to the model."""
    # The lookup tables are indexed by hydro unit id: build the units first.
    plain = glacier_config(
        tmp_path,
        ["ground", "glacier"],
        ["ground", "glacier"],
        glacier={"outline": "glaciers/sgi_2016.shp"},
    )
    project = hb.load_project(plain, base_dir=GLETSCH_DIR, setup=False)
    hydro_units = project.hydro_units.hydro_units
    unit_ids = project.hydro_units.get_ids().to_numpy().squeeze()
    fractions = cover_fractions(project, "glacier")
    areas = hydro_units["area"].to_numpy().squeeze() * fractions
    # Only the glacierized units belong to the lookup table.
    glacierized = fractions > 0
    write_lookup_tables(tmp_path / "glacier", unit_ids[glacierized], areas[glacierized])

    project = hb.load_project(
        glacier_evolution_config(tmp_path, tmp_path / "glacier"), base_dir=GLETSCH_DIR
    )
    assert project.model.get_action_count() == 1
    discharge = project.run()
    assert np.all(discharge.to_numpy() >= 0)


@needs_catchment_packages
def test_glacier_evolution_requires_a_glacier_cover(tmp_path):
    """The action is refused on a model without a glacier land cover."""
    write_lookup_tables(tmp_path / "glacier", [1, 2], [1e5, 2e5])
    config = glacier_evolution_config(tmp_path, tmp_path / "glacier")
    config["model"]["options"].update(
        land_cover_types=["ground"], land_cover_names=["ground"]
    )
    del config["hydro_units"]["land_covers"]
    config["parameters"] = dict(PARAMETERS)
    with pytest.raises(hb.ConfigurationError) as excinfo:
        hb.load_project(config, base_dir=GLETSCH_DIR)
    assert "the model declares no glacier land cover" in str(excinfo.value)


def test_glacier_evolution_validation_errors(tmp_path):
    """Unknown method, missing tables and a wrong month are all reported."""
    config = glacier_config(
        tmp_path,
        ["ground", "glacier"],
        ["ground", "glacier"],
        glacier={"outline": "glaciers/sgi_2016.shp"},
    )
    config["actions"] = {
        "glacier_evolution": {
            "method": "delta_hh",
            "lookup_tables": str(tmp_path),
            "update_month": "Octobre",
        }
    }
    with pytest.raises(hb.ConfigurationError) as excinfo:
        hb.load_project(config, base_dir=GLETSCH_DIR)
    message = str(excinfo.value)
    assert "actions.glacier_evolution.method" in message
    assert "is missing from" in message
    assert "unknown month 'Octobre'" in message


def test_unknown_action_is_reported(tmp_path):
    """An unknown action name is reported with the valid ones."""
    config = minimal_config(tmp_path)
    config["actions"] = {"land_cover_change": {}}
    with pytest.raises(hb.ConfigurationError) as excinfo:
        hb.load_project(config, base_dir=SITTER_DIR)
    assert "actions: unknown key 'land_cover_change'" in str(excinfo.value)


@needs_catchment_packages
def test_glacier_evolution_needs_a_finite_ice_storage(tmp_path):
    """The default infinite ice storage cannot hold an evolving volume."""
    write_lookup_tables(tmp_path / "glacier", [1, 2], [1e5, 2e5])
    config = glacier_evolution_config(tmp_path, tmp_path / "glacier")
    config["model"]["options"]["glacier_infinite_storage"] = True
    with pytest.raises(hb.ConfigurationError) as excinfo:
        hb.load_project(config, base_dir=GLETSCH_DIR)
    assert "glacier_infinite_storage" in str(excinfo.value)
