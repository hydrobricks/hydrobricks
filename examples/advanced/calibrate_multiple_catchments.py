import logging
import sys
import tempfile
import uuid
from pathlib import Path

import matplotlib.pyplot as plt

import hydrobricks as hb
import hydrobricks.trainer as trainer

logging.basicConfig(
    level=logging.INFO,
    stream=sys.stdout,
    force=True,
    format="%(levelname)s - %(name)s - %(message)s",
)

TEST_FILES_DIR = Path(Path(__file__).parent, "..", "..", "tests", "files", "catchments")


def catchment_config(name, hydro_units_csv, ref_elevation):
    """The per-catchment project configuration, built as a dict.

    Both catchments share the same structure, so a small function with the
    varying pieces replaces two project files (``hb.load_project`` accepts
    dicts as well as YAML paths).
    """
    return {
        "model": {
            "name": "socont",
            "options": {"soil_storage_nb": 2, "surface_runoff": "linear_storage"},
        },
        "hydro_units": {"file": hydro_units_csv},
        "forcing": {
            "file": "meteo.csv",
            "time": {"column": "date", "format": "%d/%m/%Y"},
            "columns": {
                "precipitation": "precip(mm/day)",
                "temperature": "temp(C)",
                "pet": "pet_sim(mm/day)",
            },
            "ref_elevation": ref_elevation,
            "temperature": {"gradient": "param:temp_gradients"},
            "precipitation": {
                "correction_factor": "param:precip_corr_factor",
                "gradient": "param:precip_gradient",
            },
        },
        "observations": {
            "file": "discharge.csv",
            "time": {"column": "Date", "format": "%d/%m/%Y"},
            "column": "Discharge (mm/d)",
        },
        "periods": ["1981-01-01", "2020-12-31"],
        "output": str(Path(tempfile.gettempdir()) / f"hb_{name}_{uuid.uuid4().hex}"),
        "data_parameters": {
            "temp_gradients": {"value": -0.6, "min": -1, "max": 0},
            "precip_corr_factor": {"value": 0.85, "min": 0.7, "max": 1.3},
            "precip_gradient": {"value": 0.05, "min": 0, "max": 0.2},
        },
    }


# Set up the two catchments from their (dict) project configurations.
appenzell = hb.load_project(
    catchment_config("appenzell", "hydro_units_elevation.csv", 1253),
    base_dir=TEST_FILES_DIR / "ch_sitter_appenzell",
)
stgallen = hb.load_project(
    catchment_config("stgallen", "elevation_bands.csv", 1045),
    base_dir=TEST_FILES_DIR / "ch_sitter_stgallen",
)

# One shared parameter set is calibrated on both catchments at once.
parameters = stgallen.parameters
parameters.allow_changing = [
    "a_snow",
    "k_quick",
    "A",
    "k_slow_1",
    "percol",
    "k_slow_2",
    "precip_corr_factor",
]

# Set up SPOTPY. The objective is a skill (higher is better); trainer.calibrate
# applies the sign SCE-UA needs (it minimizes) automatically — no manual inversion.
spot_setup = trainer.SpotpySetup(
    [appenzell.model, stgallen.model],
    parameters,
    [appenzell.forcing, stgallen.forcing],
    [appenzell.observations, stgallen.observations],
    warmup=365,
    obj_func="kge_2012",
)

# Run the calibration (orients the objective for the algorithm automatically).
max_rep = 4000
sampler = trainer.calibrate(
    spot_setup, "sceua", max_rep, dbname="spotpy_socont_sitter_SCEUA", dbformat="csv"
)

# Results in metric space (KGE, higher is better) — no confusing negative values.
results = trainer.get_results(sampler)
best = trainer.get_best(sampler)
print(f"Best KGE: {best['score']:.3f}")
print("Best parameters:", best["parameters"])

# Plot evolution (scores are already in skill space, higher is better).
fig_evolution = plt.figure(figsize=(9, 5))
plt.plot(results["score"])
plt.ylabel("KGE")
plt.xlabel("Iteration")
plt.tight_layout()
plt.show()

# Best simulation series (read from the raw SPOTPY records at the best index).
records = sampler.getdata()
best_model_run = records[best["index"]]
fields = [word for word in best_model_run.dtype.names if word.startswith("sim")]
best_simulation = list(best_model_run[fields])
