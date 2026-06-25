"""Calibrate several hydrological models on one catchment and compare them.

This advanced example calibrates five model configurations on the same
catchment (Sitter @ Appenzell, non-glacierized so the GR models apply) and
compares their performance:

  1. SOCONT (original)        - 1 soil reservoir, kinematic-wave surface runoff
  2. SOCONT (2 soils + snow   - 2 soil reservoirs, linear quick flow and the
     redistribution)            snow_slide lateral snow redistribution
  3. GR4J                     - with a degree-day snow routine
  4. GR6J                     - GR4J + exponential routing store
  5. HBV-96

All models share the same elevation-band discretization (with slope) and the
same forcing, so the comparison is fair. The snow redistribution variant needs
the slope of each unit and the lateral connectivity between units, both computed
here once and reused by every model. Each model is calibrated with SCE-UA on the
2012 Kling-Gupta Efficiency (KGE); the best run of each is then compared with the
observations.

Note: this script runs five calibrations and can take a while (~15-30 min with
the default settings). Reduce ``CALIBRATION_MAX_REP`` for a quick look, or
increase it (and consider the parallel calibration helpers, see
``calibrate_sceua_socont_parallel.py``) for a more thorough comparison. More
complex models (GR6J, HBV-96) have more parameters and need more iterations to
calibrate well, so keep that in mind when interpreting the scores.
"""

import logging
import os.path
import sys
import tempfile
import uuid
from pathlib import Path

import matplotlib.pyplot as plt
import numpy as np
import pandas as pd
import spotpy

import hydrobricks as hb
import hydrobricks.models as models
import hydrobricks.trainer as trainer
from hydrobricks.trainer import evaluate

logging.basicConfig(
    level=logging.INFO,
    stream=sys.stdout,
    force=True,
    format="%(levelname)s - %(name)s - %(message)s",
)

# Configuration
START_DATE = "1981-01-01"
END_DATE = "2000-12-31"
REF_ELEVATION = 1253  # Reference altitude of the meteorological station [m]
WARMUP = 365  # Days skipped at the start of the evaluation
CALIBRATION_ALGORITHM = "sceua"
CALIBRATION_OBJ_FUNC = "kge_2012"
CALIBRATION_MAX_REP = 1000  # Increase (e.g. 3000-5000) for a thorough comparison
PLOT_YEAR = 1995  # Year shown in the hydrograph comparison

# Paths
TEST_FILES_DIR = Path(
    os.path.dirname(os.path.realpath(__file__)),
    "..",
    "..",
    "tests",
    "files",
    "catchments",
)
CATCHMENT_DIR = TEST_FILES_DIR / "ch_sitter_appenzell"
CATCHMENT_OUTLINE = CATCHMENT_DIR / "outline.shp"
CATCHMENT_DEM = CATCHMENT_DIR / "dem.tif"
CATCHMENT_METEO = CATCHMENT_DIR / "meteo.csv"
CATCHMENT_DISCHARGE = CATCHMENT_DIR / "discharge.csv"

working_dir = Path(tempfile.gettempdir()) / f"tmp_{uuid.uuid4().hex}"
working_dir.mkdir(parents=True, exist_ok=True)
print(f"Working directory: {working_dir}")

# Save the C++ engine log to a file in the working directory. Note that some
# models still print transient solver messages to the console during the many
# calibration runs (the snow-redistribution variant in particular); these are
# harmless numerical warnings.
hb.init_log(str(working_dir))

# ---------------------------------------------------------------------------
# 1. Spatial discretization (shared by all models)
# ---------------------------------------------------------------------------
# Discretize the catchment into elevation bands and compute the lateral
# connectivity between them. The slope (added by the discretization) and the
# connectivity are both required by the snow-redistribution variant; the other
# models simply ignore them. Using the very same hydro units for every model
# keeps the comparison fair.
print("Discretizing the catchment and computing the lateral connectivity...")
catchment = hb.Catchment(CATCHMENT_OUTLINE)
catchment.extract_dem(CATCHMENT_DEM)
catchment.calculate_slope_aspect()
catchment.create_elevation_bands(method="equal_intervals", distance=100)
hydro_units = catchment.get_hydro_units_attributes()
connectivity = catchment.calculate_connectivity(mode="multiple")
hydro_units.set_connectivity(connectivity)

# ---------------------------------------------------------------------------
# 2. Observations (trimmed to the simulation period)
# ---------------------------------------------------------------------------
# The discharge file covers a longer period than the simulation; trim it so the
# observed series matches the simulated one (the trainer compares them directly).
obs = hb.DischargeObservations()
obs.load_from_csv(
    CATCHMENT_DISCHARGE,
    column_time="Date",
    time_format="%d/%m/%Y",
    content={"discharge": "Discharge (mm/d)"},
)
period_mask = obs.time <= pd.Timestamp(END_DATE)
obs.time = obs.time[period_mask].reset_index(drop=True)
obs.data[0] = obs.data[0][period_mask.to_numpy()]

# Time axis of the evaluation period (after the warmup), used for the plots.
eval_time = pd.DatetimeIndex(obs.time[WARMUP:])


# ---------------------------------------------------------------------------
# Helpers
# ---------------------------------------------------------------------------
def build_forcing():
    """Build the forcing for the shared hydro units.

    A fresh Forcing is created for each model so that every calibration owns its
    own; the spatialization uses ``param:`` references that are resolved at run
    time from the model parameters.
    """
    forcing = hb.Forcing(hydro_units)
    forcing.load_station_data_from_csv(
        CATCHMENT_METEO,
        column_time="date",
        time_format="%d/%m/%Y",
        content={
            "precipitation": "precip(mm/day)",
            "temperature": "temp(C)",
            "pet": "pet_sim(mm/day)",
        },
    )
    forcing.spatialize_from_station_data(
        variable="temperature",
        ref_elevation=REF_ELEVATION,
        gradient="param:temp_gradients",
    )
    forcing.correct_station_data(
        variable="precipitation", correction_factor="param:precip_corr_factor"
    )
    forcing.spatialize_from_station_data(
        variable="precipitation",
        ref_elevation=REF_ELEVATION,
        gradient="param:precip_gradient",
    )
    forcing.spatialize_from_station_data(variable="pet", method="constant")
    return forcing


def add_data_parameters(parameters):
    """Add the (fixed) data parameters referenced by the forcing.

    These are kept out of ``allow_changing`` (i.e. not calibrated) so the
    comparison focuses on the model parameters; they are added only so the
    ``param:`` references in the forcing resolve.
    """
    parameters.add_data_parameter("temp_gradients", -0.6, min_val=-1, max_val=0)
    parameters.add_data_parameter("precip_corr_factor", 0.85, min_val=0.7, max_val=1.3)
    parameters.add_data_parameter("precip_gradient", 0.05, min_val=0, max_val=0.2)


# ---------------------------------------------------------------------------
# 3. Model configurations to compare
# ---------------------------------------------------------------------------
# Each entry provides a label, a factory that builds the (uncalibrated) model,
# and the list of parameters to calibrate. The snow degree-day factor and the
# rain/snow correction factors are calibrated for every model; the snow
# redistribution parameters (snow_slide_*) are kept at their (literature)
# defaults.
model_configs = [
    (
        "SOCONT (original)",
        lambda: models.Socont(
            soil_storage_nb=1,
            surface_runoff="socont_runoff",
            snow_melt_process="melt:degree_day",
        ),
        ["a_snow", "A", "k_slow_1", "beta", "rain_corr_factor", "snow_corr_factor"],
    ),
    (
        "SOCONT (2 soils + redist.)",
        lambda: models.Socont(
            soil_storage_nb=2,
            surface_runoff="linear_storage",
            snow_redistribution="transport:snow_slide",
        ),
        [
            "a_snow",
            "A",
            "k_slow_1",
            "k_slow_2",
            "percol",
            "k_quick",
            "rain_corr_factor",
            "snow_corr_factor",
        ],
    ),
    (
        "GR4J",
        lambda: models.GR4J(snow_melt_process="melt:degree_day"),
        ["X1", "X2", "X3", "X4", "a_snow", "rain_corr_factor", "snow_corr_factor"],
    ),
    (
        "GR6J",
        lambda: models.GR6J(snow_melt_process="melt:degree_day"),
        [
            "X1",
            "X2",
            "X3",
            "X4",
            "X5",
            "X6",
            "a_snow",
            "rain_corr_factor",
            "snow_corr_factor",
        ],
    ),
    (
        "HBV-96",
        lambda: models.HBV96(),
        [
            "cfmax",
            "tt",
            "fc",
            "lp",
            "beta",
            "k_uz",
            "alpha",
            "perc",
            "k_lz",
            "maxbas",
            "rain_corr_factor",
            "snow_corr_factor",
        ],
    ),
]

# ---------------------------------------------------------------------------
# 4. Calibrate each model and collect its best simulation
# ---------------------------------------------------------------------------
comparison = []
observation = None
for label, factory, allow_changing in model_configs:
    print(f"\n=== Calibrating {label} ({len(allow_changing)} parameters) ===")

    # Build and set up the model on the shared hydro units.
    model = factory()
    model.setup(
        spatial_structure=hydro_units,
        output_path=str(working_dir),
        start_date=START_DATE,
        end_date=END_DATE,
    )
    parameters = model.generate_parameters()
    add_data_parameters(parameters)
    parameters.allow_changing = allow_changing

    # Calibrate with SCE-UA on the KGE (inverted, as SCE-UA minimizes).
    spot_setup = trainer.SpotpySetup(
        model,
        parameters,
        build_forcing(),
        obs,
        warmup=WARMUP,
        obj_func=CALIBRATION_OBJ_FUNC,
        invert_obj_func=True,
    )
    sampler = trainer.calibrate(
        spot_setup,
        CALIBRATION_ALGORITHM,
        CALIBRATION_MAX_REP,
        dbname=None,
        dbformat="ram",
    )

    # Extract the best run. SPOTPY stores the simulated series ('sim' columns)
    # of every run, so we read the best one directly (no need to re-run the
    # model nor to back-transform the parameters).
    results = sampler.getdata()
    best_index, _ = spotpy.analyser.get_minlikeindex(results)
    best_run = results[best_index]
    sim_fields = [name for name in best_run.dtype.names if name.startswith("sim")]
    best_sim = np.array([best_run[name] for name in sim_fields], dtype=float)

    # The observed series over the evaluation period (identical for all models).
    if observation is None:
        observation = np.asarray(spot_setup.evaluation()[0], dtype=float)

    kge = evaluate(best_sim, observation, "kge_2012")
    nse = evaluate(best_sim, observation, "nse")
    print(f"{label}: KGE = {kge:.3f}, NSE = {nse:.3f}")

    comparison.append(
        {
            "label": label,
            "sim": best_sim,
            "kge": kge,
            "nse": nse,
            "n_params": len(allow_changing),
        }
    )

# ---------------------------------------------------------------------------
# 5. Compare the results
# ---------------------------------------------------------------------------
ref_kge = obs.compute_reference_metric("kge_2012")
ref_nse = obs.compute_reference_metric("nse")

print("\n" + "=" * 60)
print(f"{'Model':<28}{'KGE':>8}{'NSE':>8}{'# params':>10}")
print("-" * 60)
for r in sorted(comparison, key=lambda r: r["kge"], reverse=True):
    print(f"{r['label']:<28}{r['kge']:>8.3f}{r['nse']:>8.3f}{r['n_params']:>10}")
print("-" * 60)
print(f"{'Benchmark (mean ann. cycle)':<28}{ref_kge:>8.3f}{ref_nse:>8.3f}")
print("=" * 60)

# Hydrograph comparison for a representative year.
year_mask = eval_time.year == PLOT_YEAR
fig, ax = plt.subplots(figsize=(14, 6))
ax.plot(
    eval_time[year_mask],
    observation[year_mask],
    color="black",
    linewidth=1.8,
    label="Observed",
)
for r in comparison:
    ax.plot(
        eval_time[year_mask],
        r["sim"][year_mask],
        linewidth=1.0,
        alpha=0.85,
        label=r["label"],
    )
ax.set_title(f"Calibrated model comparison - {PLOT_YEAR}")
ax.set_xlabel("Date")
ax.set_ylabel("Discharge [mm/d]")
ax.legend(loc="upper right")
fig.tight_layout()
plt.show()

# Bar chart of the performance metrics.
labels = [r["label"] for r in comparison]
x = np.arange(len(labels))
width = 0.35
fig, ax = plt.subplots(figsize=(11, 6))
ax.bar(x - width / 2, [r["kge"] for r in comparison], width, label="KGE")
ax.bar(x + width / 2, [r["nse"] for r in comparison], width, label="NSE")
ax.axhline(ref_kge, color="gray", linestyle="--", linewidth=1, label="KGE benchmark")
ax.set_xticks(x)
ax.set_xticklabels(labels, rotation=20, ha="right")
ax.set_ylabel("Score")
ax.set_title("Calibration performance by model")
ax.legend()
fig.tight_layout()
plt.show()

hb.close_log()
