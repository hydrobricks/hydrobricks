import copy
import os.path
from pathlib import Path

import numpy as np
import pytest
import yaml

import hydrobricks as hb
import hydrobricks.models as models

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

# A structure definition replicating Socont(soil_storage_nb=2,
# surface_runoff='linear_storage') on a glacier-free catchment.
SOCONT_LIKE = {
    "name": "socont_like",
    "options": {"snow_melt_process": "melt:degree_day"},
    "bricks": {
        "open": {
            "kind": "land_cover",
            "processes": {
                "infiltration": {
                    "kind": "infiltration:socont",
                    "target": "slow_reservoir",
                },
                "runoff": {"kind": "outflow:rest", "target": "surface_runoff"},
            },
        },
        "slow_reservoir": {
            "kind": "storage",
            "parameters": {"capacity": 200},
            "processes": {
                "et": {"kind": "et:socont"},
                "outflow": {"kind": "outflow:linear", "target": "outlet"},
                "percolation": {
                    "kind": "percolation:constant",
                    "target": "slow_reservoir_2",
                },
                "overflow": {"kind": "overflow", "target": "outlet"},
            },
        },
        "slow_reservoir_2": {
            "kind": "storage",
            "processes": {"outflow": {"kind": "outflow:linear", "target": "outlet"}},
        },
        "surface_runoff": {
            "kind": "storage",
            "processes": {"runoff": {"kind": "outflow:linear", "target": "outlet"}},
        },
    },
    "aliases": {
        "slow_reservoir:capacity": "A",
        "slow_reservoir:response_factor": "k_slow_1",
        "slow_reservoir_2:response_factor": "k_slow_2",
        "surface_runoff:response_factor": "k_quick",
    },
    "constraints": [["k_slow_2", "<", "k_slow_1"]],
}


def project_config(tmp_path, model_section):
    return {
        "model": model_section,
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


def test_custom_model_matches_socont(tmp_path):
    """A custom structure replicating SOCONT produces the same discharge."""
    structure_file = tmp_path / "structure.yaml"
    structure_file.write_text(yaml.safe_dump(SOCONT_LIKE), encoding="utf-8")

    custom = hb.load_project(
        project_config(tmp_path / "custom", {"structure": str(structure_file)}),
        base_dir=SITTER_DIR,
    )
    reference = hb.load_project(
        project_config(
            tmp_path / "socont",
            {
                "name": "socont",
                "options": {
                    "soil_storage_nb": 2,
                    "surface_runoff": "linear_storage",
                },
            },
        ),
        base_dir=SITTER_DIR,
    )

    assert custom.model.name == "socont_like"
    sim_custom = custom.run()
    sim_reference = reference.run()
    np.testing.assert_allclose(sim_custom.to_numpy(), sim_reference.to_numpy())


def test_custom_model_direct_use():
    """CustomModel works standalone: parameters with aliases and constraints."""
    model = models.CustomModel(copy.deepcopy(SOCONT_LIKE))
    parameters = model.generate_parameters()
    assert parameters.has("A")
    assert parameters.has("k_quick")
    assert parameters.has("a_snow")  # from the generated snow routine
    parameters.set_values(dict(PARAMETERS))
    assert parameters.is_valid()


def test_custom_model_structure_graph():
    """The structure graph works on custom models (bricks are all there)."""
    model = models.CustomModel(copy.deepcopy(SOCONT_LIKE))
    graph = model.get_structure_graph()
    names = {node["name"] for node in graph.to_dict()["nodes"]}
    assert "slow_reservoir" in names
    assert "surface_runoff" in names
    assert "open_snowpack" in names  # the generated snow routine is there too


def test_custom_model_validation_errors():
    """All structure problems are collected, with suggestions for typos."""
    spec = copy.deepcopy(SOCONT_LIKE)
    spec["bricks"]["slow_reservoir"]["atach_to"] = "hydro_unit"  # Typo.
    del spec["bricks"]["surface_runoff"]["processes"]["runoff"]["target"]
    spec["constraints"].append(["a", "!=", "b"])
    with pytest.raises(hb.ConfigurationError) as excinfo:
        models.CustomModel(spec)
    message = str(excinfo.value)
    assert "did you mean 'attach_to'?" in message
    assert "a 'target' is required" in message
    assert "unknown operator '!='" in message


def test_custom_model_unknown_land_cover():
    spec = copy.deepcopy(SOCONT_LIKE)
    spec["bricks"]["forest_zone"] = spec["bricks"].pop("open")
    with pytest.raises(hb.ConfigurationError) as excinfo:
        models.CustomModel(spec)
    assert "land cover brick 'forest_zone'" in str(excinfo.value)


def test_custom_model_missing_bricks():
    with pytest.raises(hb.ConfigurationError) as excinfo:
        models.CustomModel({"name": "empty"})
    assert "structure.bricks" in str(excinfo.value)


def test_project_model_name_and_structure_exclusive(tmp_path):
    structure_file = tmp_path / "structure.yaml"
    structure_file.write_text(yaml.safe_dump(SOCONT_LIKE), encoding="utf-8")
    config = project_config(
        tmp_path, {"name": "socont", "structure": str(structure_file)}
    )
    with pytest.raises(hb.ConfigurationError) as excinfo:
        hb.load_project(config, base_dir=SITTER_DIR)
    assert "'name' and 'structure' are mutually exclusive" in str(excinfo.value)


def test_project_missing_structure_file(tmp_path):
    config = project_config(tmp_path, {"structure": "does_not_exist.yaml"})
    with pytest.raises(hb.ConfigurationError) as excinfo:
        hb.load_project(config, base_dir=SITTER_DIR)
    assert "does_not_exist.yaml" in str(excinfo.value)


def test_project_inline_structure(tmp_path):
    """A structure can be declared inline in the project configuration."""
    config = project_config(tmp_path, {"structure": copy.deepcopy(SOCONT_LIKE)})
    project = hb.load_project(config, base_dir=SITTER_DIR)
    discharge = project.run()
    assert len(discharge) == 365
