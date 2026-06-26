import logging
import sys

import matplotlib.pyplot as plt

import hydrobricks.trainer as trainer
from examples._helpers.models_setup_helper import ModelSetupHelper

logging.basicConfig(
    level=logging.INFO,
    stream=sys.stdout,
    force=True,
    format="%(levelname)s - %(name)s - %(message)s",
)

# Set up the model for the Sitter
helper_appenzell = ModelSetupHelper(
    "ch_sitter_appenzell", start_date="1981-01-01", end_date="2020-12-31"
)
helper_appenzell.create_hydro_units_from_csv_file(filename="hydro_units_elevation.csv")
forcing_appenzell = helper_appenzell.get_forcing_data_from_csv_file(
    ref_elevation=1253, use_precip_gradient=True
)
obs_appenzell = helper_appenzell.get_obs_data_from_csv_file()
socont_appenzell, _ = helper_appenzell.get_model_and_params_socont()

# Set up the model for the Rhone at Gletsch
helper_stgallen = ModelSetupHelper(
    "ch_sitter_stgallen", start_date="1981-01-01", end_date="2020-12-31"
)
helper_stgallen.create_hydro_units_from_csv_file(filename="elevation_bands.csv")
forcing_stgallen = helper_stgallen.get_forcing_data_from_csv_file(
    ref_elevation=1045, use_precip_gradient=True
)
obs_stgallen = helper_stgallen.get_obs_data_from_csv_file()
socont_stgallen, parameters = helper_stgallen.get_model_and_params_socont()

# Select the parameters to optimize/analyze
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
    [socont_appenzell, socont_stgallen],
    parameters,
    [forcing_appenzell, forcing_stgallen],
    [obs_appenzell, obs_stgallen],
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
