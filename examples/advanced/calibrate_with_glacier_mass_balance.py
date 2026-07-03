"""Calibrate a glacio-hydrological model on discharge AND glacier mass balance.

On glacierized catchments the observed glacier mass balance (here GLAMOS data for
the Rhonegletscher) is a strong, independent constraint on the snow/ice melt
parameters. This example calibrates a SOCONT model on the Rhone @ Gletsch
catchment using both the observed discharge and the observed glacier mass
balance, and shows the three ways the mass balance can be used.

Simulated mass balance
----------------------
hydrobricks computes the **glaciological (surface) mass balance**, i.e.
accumulation minus ablation at the glacier surface — exactly what the GLAMOS
glaciological method measures. Per glacier hydro unit and observation period::

    B = ΔS - Σ ice_melt           (S = glacier snowpack water equivalent)

This flux-based balance excludes ice dynamics, so it matches the GLAMOS
*glaciological* product both for the whole glacier and per elevation band (a
state-difference balance would instead mix in the delta-h redistribution and
become a *geodetic* per-band balance). It works with the default infinite ice
storage, which is what we use here: the glacier geometry stays fixed, which is
appropriate for calibrating melt parameters and lets the model be re-run cleanly
thousands of times. Per-band values are normalized by the model's own glacier
area. See ``hydrobricks/evaluation/glacier_mass_balance.py`` for the full rationale.

Three ways to use the mass balance
----------------------------------
The mass balance is passed to ``SpotpySetup`` as an ``extra_observations`` signal.
Each signal carries its own ``mode`` (``'objective'`` or ``'constraint'``), and the
objective terms combine via the setup-level ``combine`` argument:

- objective + ``combine='weighted'``: a single score combining discharge and
  mass-balance skill.
- objective + ``combine='pareto'``: a ``[discharge, mass-balance]`` objective
  vector for a multi-objective sampler (SPOTPY's NSGAII).
- ``mode='constraint'``: a behavioural pass/fail filter — runs whose mass balance
  is off by more than the signal's ``tolerance`` are rejected; discharge stays the
  objective.

Note: this script runs several calibrations of a glacierized model and can take
a while. ``CALIBRATION_MAX_REP`` is kept small for a quick look; raise it (and
consider the parallel helpers, see ``calibrate_sceua_socont_parallel.py``) for a
real calibration.
"""

import logging
import os.path
import sys
import tempfile
import uuid
import warnings
from pathlib import Path

import matplotlib.pyplot as plt

import hydrobricks as hb
import hydrobricks.models as models
import hydrobricks.trainer as trainer

logging.basicConfig(
    level=logging.INFO,
    stream=sys.stdout,
    force=True,
    format="%(levelname)s - %(name)s - %(message)s",
)

# Configuration
START_DATE = "2009-01-01"
END_DATE = "2020-12-31"
REF_ELEVATION = 2702  # Reference altitude of the meteorological station [m]
WARMUP = 365
CALIBRATION_MAX_REP = 5000
# Score SPOTPY stores for a rejected (non-behavioural / invalid) run: hydrobricks'
# _WORST_PENALTY. It is a large *finite* value, and real objective scores are O(1),
# so a stored score below it is a genuine run and one at it is a rejection.
REJECTED_SCORE = 1e12

# Paths
CATCHMENT_DIR = Path(
    os.path.dirname(os.path.realpath(__file__)),
    "..",
    "..",
    "tests",
    "files",
    "catchments",
    "ch_rhone_gletsch",
)
GLACIER_DIR = CATCHMENT_DIR / "glaciers"
CATCHMENT_OUTLINE = CATCHMENT_DIR / "outline.shp"
CATCHMENT_DEM = CATCHMENT_DIR / "dem.tif"
GLACIER_ICE_THICKNESS = GLACIER_DIR / "ice_thickness.tif"
GLACIER_MB_WHOLE = GLACIER_DIR / "massbalance_fixdate.csv"
CATCHMENT_METEO = CATCHMENT_DIR / "meteo.csv"
CATCHMENT_DISCHARGE = CATCHMENT_DIR / "discharge.csv"

working_dir = Path(tempfile.gettempdir()) / f"tmp_{uuid.uuid4().hex}"
working_dir.mkdir(parents=True, exist_ok=True)
print(f"Working directory: {working_dir}")

# ---------------------------------------------------------------------------
# 1. Catchment, hydro units and initial glacier cover
# ---------------------------------------------------------------------------
# Discretize into elevation bands and initialize the glacier fraction of each
# unit from the ice-thickness raster (a static glacier; the geometry is kept
# fixed while we calibrate the melt parameters).
print("Setting up the catchment and the initial glacier cover...")
catchment = hb.Catchment(
    CATCHMENT_OUTLINE,
    land_cover_types=["open", "glacier"],
    land_cover_names=["open", "glacier"],
)
catchment.extract_dem(CATCHMENT_DEM)
catchment.create_elevation_bands(method="equal_intervals", distance=100)

# The glacier geometry stays fixed here (static glacier), so only the initial cover
# is needed — not a delta-h lookup table. initialize_glacier_cover_from_extent sets
# the glacier land-cover fractions of each hydro unit directly from the ice-thickness
# raster, without the elevation-band discretization compute_initial_ice_thickness does.
hb.preprocessing.initialize_glacier_cover_from_extent(
    catchment, ice_thickness=GLACIER_ICE_THICKNESS
)
hydro_units = catchment.hydro_units

# ---------------------------------------------------------------------------
# 2. Observations: discharge and glacier mass balance
# ---------------------------------------------------------------------------
# DischargeObservations restricts the loaded data to [START_DATE, END_DATE] by
# default, so it already matches the simulated series (used as-is in evaluate_run()
# below).
obs = hb.DischargeObservations(START_DATE, END_DATE)
obs.load_from_csv(
    CATCHMENT_DISCHARGE,
    column_time="Date",
    time_format="%d/%m/%Y",
    content={"discharge": "Discharge (mm/d)"},
)

# Observed glacier mass balance (GLAMOS): whole-glacier annual, winter and summer
# balances. The observation periods come from the per-row dates in the file.
glacier_mb = hb.GlacierMassBalanceObservations.from_glamos(
    GLACIER_MB_WHOLE,
    kind="whole",
    glacier_id="B43-03",
    balance_types=("annual", "winter", "summer"),
    start_date=START_DATE,
    end_date=END_DATE,
)
print(f"Loaded {len(glacier_mb)} glacier mass-balance observations.")


# ---------------------------------------------------------------------------
# Helpers
# ---------------------------------------------------------------------------
def build_model():
    """Build and set up the (static-glacier) SOCONT model.

    ``record_all=True`` is required so the glacier snowpack and ice melt are
    recorded and the simulated mass balance can be read from memory.
    """
    model = models.Socont(
        soil_storage_nb=2,
        surface_runoff="linear_storage",
        record_all=True,
        land_cover_types=["open", "glacier"],
        land_cover_names=["open", "glacier"],
    )
    model.setup(
        spatial_structure=hydro_units,
        output_path=str(working_dir),
        start_date=START_DATE,
        end_date=END_DATE,
    )
    return model


def build_forcing():
    """Build the forcing for the shared hydro units."""
    forcing = hb.Forcing(hydro_units)
    forcing.load_station_data_from_csv(
        CATCHMENT_METEO,
        column_time="date",
        time_format="%d/%m/%Y",
        content={"precipitation": "precip(mm/day)", "temperature": "temp(C)"},
    )
    forcing.spatialize_from_station_data(
        variable="temperature", ref_elevation=REF_ELEVATION, gradient=-0.6
    )
    forcing.spatialize_from_station_data(
        variable="precipitation", ref_elevation=REF_ELEVATION, gradient=0.05
    )
    forcing.compute_pet(method="Oudin", use=["t", "lat"], lat=46.6)
    return forcing


def build_parameters():
    """Parameters with sensible defaults; the melt parameters are calibrated."""
    parameters = build_model().generate_parameters()
    parameters.allow_changing = [
        "A",
        "a_snow",
        "a_ice",
        "k_slow_1",
        "k_slow_2",
        "k_quick",
        "percol",
        "k_snow",
        "k_ice",
    ]
    return parameters


def print_real_parameters(param_values):
    """Print the calibrated parameters as their real (physical) values.

    SPOTPY's own summary prints the values in the optimizer's *transformed* space
    (e.g. the slow-reservoir response factors as log(k[1/h]), which look negative);
    ``get_best``/``get_results`` back-transform them, so these are the values the
    model actually uses.
    """
    print("Best parameters (real values):")
    for name, value in param_values.items():
        print(f"  {name}: {value:.4g}")


def evaluate_run(param_values):
    """Re-run the model with the given real parameters and score the two signals.

    Returns the discharge KGE (2012) and the glacier mass-balance RMSE separately,
    so the combined weighted objective can be read term by term. The discharge is
    sliced by the warmup to match the calibration; the glacier mass-balance targets
    are already restricted to the post-warmup period, so they are compared as-is.
    """
    parameters.set_values(param_values)
    model.run(parameters=parameters, forcing=forcing)
    sim_q = model.get_outlet_discharge()
    kge = hb.evaluate(sim_q[WARMUP:], obs.data[0][WARMUP:], "kge_2012")
    rmse = hb.evaluate(glacier_mb.simulated(model), glacier_mb.observed(), "rmse")
    return kge, rmse


# ---------------------------------------------------------------------------
# 3. Calibrate, comparing the three glacier mass-balance modes
# ---------------------------------------------------------------------------
# Each calibration scores the discharge with the 2012 KGE (inverted for the
# minimizer). The 'weighted' and 'constraint' modes use SCE-UA (single score);
# the 'pareto' mode uses NSGAII (a [discharge, mass balance] objective vector).
model = build_model()
forcing = build_forcing()
parameters = build_parameters()

# The mass-balance signal carries its own metric/weight/mode; here it is an
# 'objective' term (used by the weighted and pareto combinations below).
glacier_mb.metric = "rmse"
glacier_mb.weight = 0.5
glacier_mb.mode = "objective"

# Collects the discharge KGE and glacier mass-balance RMSE of each mode's best run
# for the comparison table printed at the end.
summary = {}

print("\n=== combine='weighted': single score combining discharge and mass balance ===")
spot_setup = trainer.SpotpySetup(
    model,
    parameters,
    forcing,
    obs,
    warmup=WARMUP,
    obj_func="kge_2012",
    extra_observations=[glacier_mb],
    combine="weighted",
    discharge_weight=1.0,
)
sampler = trainer.calibrate(spot_setup, "sceua", CALIBRATION_MAX_REP, dbformat="ram")
best = trainer.get_best(sampler)
# Score in skill space (higher is better): the combined discharge + mass-balance
# objective, not a sign-flipped value.
print(f"Best combined objective: {best['score']:.3f}")
print_real_parameters(best["parameters"])
kge_w, rmse_w = evaluate_run(best["parameters"])
print(f"  Discharge KGE (2012):      {kge_w:.3f}")
print(f"  Glacier MB RMSE [mm w.e.]: {rmse_w:.1f}")
summary["weighted"] = {"kge": kge_w, "rmse": rmse_w, "combined": best["score"]}

print("\n=== constraint: reject runs with a poor mass balance ===")
# Keep the discharge KGE as the objective, but reject any run whose mean absolute
# mass-balance error exceeds the tolerance (mm w.e.). The behavioural filter lives
# on the observation object itself (mode='constraint').
glacier_mb_constraint = hb.GlacierMassBalanceObservations.from_glamos(
    GLACIER_MB_WHOLE,
    kind="whole",
    glacier_id="B43-03",
    balance_types=("annual", "winter", "summer"),
    start_date=START_DATE,
    end_date=END_DATE,
    mode="constraint",
    # Accept runs whose mass balance is within 50% of the observed mean. A tighter
    # value (e.g. 0.3) can reject the entire random burn-in population, leaving
    # SCE-UA with no fitness gradient to evolve from; loosen it if every run scores
    # the rejection penalty.
    relative_tolerance=0.5,
)
spot_setup_c = trainer.SpotpySetup(
    model,
    parameters,
    forcing,
    obs,
    warmup=WARMUP,
    obj_func="kge_2012",
    extra_observations=[glacier_mb_constraint],
)
sampler_c = trainer.calibrate(
    spot_setup_c, "sceua", CALIBRATION_MAX_REP, dbformat="ram"
)
results_c = sampler_c.getdata()
# A rejected run is stored with the (finite) rejection penalty, so np.isfinite() would
# wrongly count it as behavioural; a behavioural run scores below the penalty.
behavioural = results_c["like1"] < REJECTED_SCORE
print(f"Behavioural (within tolerance) runs: {behavioural.sum()} / {len(results_c)}")
if behavioural.sum() > 0:
    # Objective here is the discharge KGE alone; the mass balance only filters runs.
    best_c = trainer.get_best(sampler_c)
    print_real_parameters(best_c["parameters"])
    kge_c, rmse_c = evaluate_run(best_c["parameters"])
    # best_c['score'] is the discharge KGE, so it should match kge_c (cross-check).
    print(f"  Discharge KGE (2012):      {kge_c:.3f}  (the objective)")
    print(f"  Glacier MB RMSE [mm w.e.]: {rmse_c:.1f}")
    summary["constraint"] = {"kge": kge_c, "rmse": rmse_c, "combined": None}
else:
    print("  No behavioural run: skipping (relax the tolerance or run more reps).")

print("\n=== combine='pareto': discharge vs mass balance trade-off (NSGAII) ===")
# Multi-objective: NSGAII returns a Pareto set rather than a single best run.
spot_setup_p = trainer.SpotpySetup(
    model,
    parameters,
    forcing,
    obs,
    warmup=WARMUP,
    obj_func="kge_2012",
    extra_observations=[glacier_mb],
    combine="pareto",
)
# Unlike SCE-UA, NSGAII's first sample() argument is the number of *generations*,
# not the total number of runs: it performs generations * n_pop model evaluations.
# Divide by n_pop so the total stays comparable to CALIBRATION_MAX_REP. A larger
# population keeps the Pareto front more diverse (fewer fronts collapse to a single
# objective value), which reduces NSGA-II's zero-range crowding-distance divisions,
# at the cost of fewer generations for the same total budget.
NSGAII_POP = 50
# SPOTPY's NSGA-II normalizes crowding distance by each objective's range within a
# front; when a front's members share an objective value that range is zero, raising
# a harmless "invalid value encountered in divide" RuntimeWarning (NaN crowding
# distances for that step, then the run continues). Silence just that warning.
warnings.filterwarnings(
    "ignore",
    message="invalid value encountered in divide",
    module="spotpy.algorithms.nsgaii",
)
sampler_p = trainer.calibrate(
    spot_setup_p,
    "NSGAII",
    CALIBRATION_MAX_REP // NSGAII_POP,
    dbformat="ram",
    sample_kwargs={"n_obj": 2, "n_pop": NSGAII_POP},
)
results_p = sampler_p.getdata()
print(f"Pareto sampler produced {len(results_p)} evaluations.")

# get_best is undefined for a multi-objective run, so pick one representative point:
# the compromise that maximizes the sum of the two skills (both higher is better).
df_p = trainer.get_results(sampler_p)
param_names = [c for c in df_p.columns if not c.startswith("score")]
idx_p = (df_p["score1"] + df_p["score2"]).idxmax()
pareto_params = {n: float(df_p.loc[idx_p, n]) for n in param_names}
print("Representative Pareto point (max score1 + score2):")
print_real_parameters(pareto_params)
kge_p, rmse_p = evaluate_run(pareto_params)
print(f"  Discharge KGE (2012):      {kge_p:.3f}")
print(f"  Glacier MB RMSE [mm w.e.]: {rmse_p:.1f}")
summary["pareto*"] = {"kge": kge_p, "rmse": rmse_p, "combined": None}

# ---------------------------------------------------------------------------
# Summary: compare the discharge KGE and glacier MB RMSE across the three modes
# ---------------------------------------------------------------------------
print("\n=== Summary: discharge KGE and glacier MB RMSE by mode ===")
print(f"{'Mode':<12} {'Discharge KGE':>14} {'MB RMSE [mm w.e.]':>18} {'Combined':>10}")
for mode, m in summary.items():
    combined = f"{m['combined']:.3f}" if m["combined"] is not None else "-"
    print(f"{mode:<12} {m['kge']:>14.3f} {m['rmse']:>18.1f} {combined:>10}")
print("* pareto: one representative point (max score1 + score2) on the Pareto front")

# ---------------------------------------------------------------------------
# 4. Inspect the best 'weighted' run: observed vs simulated mass balance
# ---------------------------------------------------------------------------
# Re-run the model with the best parameter set and compute the simulated mass
# balance to compare it with the observations.
parameters.set_values(best["parameters"])
model.run(parameters=parameters, forcing=forcing)
sim_mb = glacier_mb.simulated(model)

annual = [
    (t["t0"].year, t["value"], s)
    for t, s in zip(glacier_mb.targets, sim_mb)
    if t["balance_type"] == "annual"
]
years = [a[0] for a in annual]
obs_vals = [a[1] for a in annual]
sim_vals = [a[2] for a in annual]

fig, ax = plt.subplots(figsize=(12, 6))
ax.plot(years, obs_vals, "o-", color="black", label="Observed (GLAMOS)")
ax.plot(years, sim_vals, "s--", color="tab:blue", label="Simulated")
ax.axhline(0, color="gray", linewidth=0.8)
ax.set_title("Rhonegletscher annual glacier mass balance")
ax.set_xlabel("Hydrological year")
ax.set_ylabel("Mass balance [mm w.e.]")
ax.legend()
fig.tight_layout()
plt.show()

hb.close_log()
