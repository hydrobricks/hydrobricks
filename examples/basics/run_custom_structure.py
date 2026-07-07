"""Define a model structure as data and run it.

``custom_structure.yaml`` declares the full model graph — bricks (stores and
land covers), processes, and the fluxes routing the water — and replicates
SOCONT as an illustration. The structure is inspected, plugged into a project
configuration and run; the parameters (with the aliases and constraints of the
structure file) are generated from the declared processes, so calibration
works exactly as for the pre-built models.
"""

import logging
import sys
from pathlib import Path

import hydrobricks as hb
import hydrobricks.models as models

logging.basicConfig(
    level=logging.INFO,
    stream=sys.stdout,
    force=True,
    format="%(levelname)s - %(name)s - %(message)s",
)

EXAMPLE_DIR = Path(__file__).parent
STRUCTURE_FILE = EXAMPLE_DIR / "custom_structure.yaml"
CATCHMENT_DIR = (
    EXAMPLE_DIR / ".." / ".." / "tests" / "files" / "catchments" / "ch_sitter_appenzell"
)

# Inspect the declared structure (includes the generated snow routine).
model = models.CustomModel(STRUCTURE_FILE)
model.print_structure()

# Run it through a project configuration (the model section points to the
# structure file instead of a pre-built model name).
config = {
    "model": {"structure": str(STRUCTURE_FILE)},
    "hydro_units": {"file": "hydro_units_elevation.csv"},
    "forcing": {
        "file": "meteo.csv",
        "time": {"column": "date", "format": "%d/%m/%Y"},
        "columns": {
            "precipitation": "precip(mm/day)",
            "temperature": "temp(C)",
            "pet": "pet_sim(mm/day)",
        },
        "ref_elevation": 1253,
        "precipitation": {"correction_factor": 0.75, "gradient": 0.05},
    },
    "observations": {
        "file": "discharge.csv",
        "time": {"column": "Date", "format": "%d/%m/%Y"},
        "column": "Discharge (mm/d)",
    },
    "periods": {"simulation": ["1981-01-01", "2020-12-31"], "spinup": "2y"},
    "output": str(EXAMPLE_DIR / "output"),
    "parameters": {
        "A": 458,
        "a_snow": 2,
        "k_slow_1": 0.9,
        "k_slow_2": 0.8,
        "k_quick": 1,
        "percol": 9.8,
    },
}
project = hb.load_project(config, base_dir=CATCHMENT_DIR)
simulated = project.run()

nse = project.model.eval("nse", project.observations.data[0])
print(f"Custom structure '{project.model.name}': NSE = {nse:.3f}")
