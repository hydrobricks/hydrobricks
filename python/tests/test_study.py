import json
import os.path
from pathlib import Path

import numpy as np
import pytest

import hydrobricks as hb
from hydrobricks.study import (
    _deep_merge,
    _resolve_matrix,
    _sanitize,
    _set_dotted,
)

TEST_FILES_DIR = Path(
    os.path.dirname(os.path.realpath(__file__)),
    "..",
    "..",
    "tests",
    "files",
    "catchments",
)

PARAMETERS = {
    "A": 200,
    "a_snow": 3,
    "k_slow_1": 0.01,
    "k_slow_2": 0.001,
    "percol": 1,
    "k_quick": 0.05,
}


# --------------------------------------------------------------------------- #
# Matrix resolution helpers (pure)
# --------------------------------------------------------------------------- #
def test_deep_merge_nested_and_replace():
    base = {"a": {"x": 1, "y": 2}, "b": [1, 2], "c": 3}
    patch = {"a": {"y": 20, "z": 30}, "b": [9]}
    merged = _deep_merge(base, patch)
    assert merged == {"a": {"x": 1, "y": 20, "z": 30}, "b": [9], "c": 3}
    assert base["a"] == {"x": 1, "y": 2}  # The base is not mutated.


def test_set_dotted_creates_nested_keys():
    config = {"calibration": {"algorithm": "sceua"}}
    _set_dotted(config, "calibration.objective", "nse")
    _set_dotted(config, "model.options.soil_storage_nb", 2)
    assert config["calibration"] == {"algorithm": "sceua", "objective": "nse"}
    assert config["model"] == {"options": {"soil_storage_nb": 2}}


def test_sanitize_values():
    assert _sanitize("power(0.2)") == "power-0.2"
    assert _sanitize("kge_2012") == "kge_2012"
    assert _sanitize(0.5) == "0.5"


def test_matrix_product_order_and_shorthands():
    errors = []
    dims, combos = _resolve_matrix(
        {
            "matrix": {
                "objective": ["nse", "kge_2012"],
                "transform": ["none", "power(0.2)"],
            }
        },
        variants={},
        errors=errors,
    )
    assert errors == []
    assert dims == ["objective", "transform"]
    assert len(combos) == 4
    assert combos[0] == {"objective": "nse", "transform": "none"}
    assert combos[-1] == {"objective": "kge_2012", "transform": "power(0.2)"}


def test_matrix_exclude_and_include():
    matrix = {
        "objective": ["nse", "kge_2012"],
        "transform": ["none", "power(0.2)"],
        "exclude": [{"objective": "kge_2012", "transform": "power(0.2)"}],
        "include": [{"objective": "nse", "transform": "log"}],
    }
    errors = []
    dims, combos = _resolve_matrix({"matrix": matrix}, variants={}, errors=errors)
    assert errors == []
    assert len(combos) == 4  # 4 - 1 excluded + 1 included
    assert {"objective": "kge_2012", "transform": "power(0.2)"} not in combos
    assert combos[-1] == {"objective": "nse", "transform": "log"}


def test_matrix_include_must_value_every_dimension():
    matrix = {
        "objective": ["nse"],
        "transform": ["none"],
        "include": [{"objective": "kge_2012"}],
    }
    errors = []
    _resolve_matrix({"matrix": matrix}, variants={}, errors=errors)
    assert any("must value every dimension" in e for e in errors)


def test_matrix_unknown_dimension_reported():
    errors = []
    _resolve_matrix({"matrix": {"catchment": ["a"]}}, variants={}, errors=errors)
    assert any("unknown dimension" in e for e in errors)


def test_matrix_dotted_dimension_allowed():
    errors = []
    dims, combos = _resolve_matrix(
        {"matrix": {"calibration.algorithm": ["sceua", "mc"]}},
        variants={},
        errors=errors,
    )
    assert errors == []
    assert combos == [
        {"calibration.algorithm": "sceua"},
        {"calibration.algorithm": "mc"},
    ]


def test_matrix_missing_variant_value_reported():
    errors = []
    _resolve_matrix(
        {"matrix": {"catchment": ["a", "b"]}},
        variants={"catchment": {"a": {}}},
        errors=errors,
    )
    assert any("no variant patch for 'b'" in e for e in errors)


# --------------------------------------------------------------------------- #
# load_study on real project configs
# --------------------------------------------------------------------------- #
def demo_study(tmp_path, repetitions=2):
    return {
        "name": "demo",
        "output": str(tmp_path / "study"),
        "base": {
            "model": {
                "name": "socont",
                "options": {"soil_storage_nb": 2, "surface_runoff": "linear_storage"},
            },
            "forcing": {
                "time": {"column": "date", "format": "%d/%m/%Y"},
                "columns": {
                    "precipitation": "precip(mm/day)",
                    "temperature": "temp(C)",
                    "pet": "pet_sim(mm/day)",
                },
                "ref_elevation": 1250,
            },
            "observations": {
                "time": {"column": "Date", "format": "%d/%m/%Y"},
                "column": "Discharge (mm/d)",
            },
            "periods": {
                "calibration": ["1981-01-01", "1981-12-31"],
                "validation": ["1982-01-01", "1982-12-31"],
                "spinup": 60,
            },
            "parameters": dict(PARAMETERS),
            "calibration": {
                "algorithm": "mc",
                "repetitions": repetitions,
                "parameters": ["a_snow", "A"],
            },
        },
        "variants": {
            "catchment": {
                "appenzell": {
                    "hydro_units": {
                        "file": "ch_sitter_appenzell/hydro_units_elevation.csv"
                    },
                    "forcing": {"file": "ch_sitter_appenzell/meteo.csv"},
                    "observations": {"file": "ch_sitter_appenzell/discharge.csv"},
                },
                "stgallen": {
                    "hydro_units": {"file": "ch_sitter_stgallen/elevation_bands.csv"},
                    "forcing": {"file": "ch_sitter_stgallen/meteo.csv"},
                    "observations": {"file": "ch_sitter_stgallen/discharge.csv"},
                },
            },
        },
        "matrix": {
            "catchment": ["appenzell", "stgallen"],
            "objective": ["nse", "kge_2012"],
        },
        "evaluation": {
            "metrics": ["nse"],
            "transforms": ["none", "power(0.2)"],
        },
    }


def test_load_study_builds_jobs(tmp_path):
    study = hb.load_study(demo_study(tmp_path), base_dir=TEST_FILES_DIR)
    assert study.name == "demo"
    assert study.dimensions == ["catchment", "objective"]
    assert [job.id for job in study.jobs] == [
        "appenzell__nse",
        "appenzell__kge_2012",
        "stgallen__nse",
        "stgallen__kge_2012",
    ]
    job = study.job("stgallen__kge_2012")
    assert job.values == {"catchment": "stgallen", "objective": "kge_2012"}
    assert job.config["calibration"]["objective"] == "kge_2012"
    assert job.config["hydro_units"]["file"].startswith("ch_sitter_stgallen")
    assert job.config["output"].endswith(os.path.join("jobs", "stgallen__kge_2012"))
    assert job.config["cache"] == str(tmp_path / "study" / "cache")

    with pytest.raises(hb.ConfigurationError) as excinfo:
        study.job("stgallen__kge")
    assert "Did you mean" in str(excinfo.value)


def test_study_validate_checks_each_job(tmp_path):
    config = demo_study(tmp_path)
    config["variants"]["catchment"]["stgallen"]["hydro_units"]["file"] = "missing.csv"
    config["base"]["calibration"]["parameters_typo"] = ["a_snow"]
    study = hb.load_study(config, base_dir=TEST_FILES_DIR)  # Loads fine.
    with pytest.raises(hb.ConfigurationError) as excinfo:
        study.validate()
    message = str(excinfo.value)
    assert "job 'stgallen__nse': " in message
    assert "missing.csv" in message
    assert "unknown key 'parameters_typo'" in message


def test_load_study_requires_calibration(tmp_path):
    config = demo_study(tmp_path)
    del config["base"]["calibration"]
    with pytest.raises(hb.ConfigurationError) as excinfo:
        hb.load_study(config, base_dir=TEST_FILES_DIR)
    assert "'calibration' section with 'repetitions' and 'parameters'" in str(
        excinfo.value
    )


def test_study_run_all_and_assess(tmp_path):
    """End-to-end: 2 catchments x 2 objectives, calibrated and assessed."""
    study = hb.load_study(demo_study(tmp_path), base_dir=TEST_FILES_DIR)
    scores = study.run_all()

    result_files = sorted(study.results_dir.glob("*.json"))
    assert len(result_files) == 4
    assert (study.results_dir / "scores.csv").exists()

    # 4 jobs x 3 periods x 2 transforms x 1 metric = 24 rows.
    assert len(scores) == 24
    assert set(scores.columns) >= {
        "catchment",
        "objective",
        "job_id",
        "period",
        "eval_transform",
        "eval_metric",
        "score",
        "calibration_score",
    }
    assert set(scores["catchment"]) == {"appenzell", "stgallen"}
    assert set(scores["eval_transform"]) == {"none", "power(0.2)"}
    assert np.isfinite(scores["score"]).all()

    # A result record is complete and reloadable.
    record = json.loads(result_files[0].read_text(encoding="utf-8"))
    assert set(record) >= {
        "job_id",
        "values",
        "calibration_score",
        "best_parameters",
        "scores",
    }
    assert set(record["best_parameters"]) == {"a_snow", "A"}

    # Idempotence: a re-run reloads the records instead of recomputing.
    mtimes = [f.stat().st_mtime_ns for f in result_files]
    study2 = hb.load_study(demo_study(tmp_path), base_dir=TEST_FILES_DIR)
    study2.run_all()
    assert [f.stat().st_mtime_ns for f in result_files] == mtimes

    # The comparison pivot has the dimensions as rows.
    pivot = study.pivot(period="validation", metric="nse")
    assert pivot.index.names == ["catchment", "objective"]
    assert pivot.shape == (4, 2)
