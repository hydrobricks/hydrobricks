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

Signals, and the three ways to use the mass balance
--------------------------------------------------
The GLAMOS file holds three balances — annual, winter and summer — and they
are loaded as three *separate* ``extra_observations`` signals rather than one
pooled one. Pooling would concatenate them into a single vector scored by a single
RMSE, and the benchmark that RMSE is measured against is the standard deviation of
that pooled vector (~1690 mm w.e.), dominated by the winter/summer contrast (means
of roughly +1560 and -2370 mm w.e.) rather than by the interannual variability the
melt parameters control.

Each signal carries its own ``metric``, ``weight`` and ``mode`` (``'objective'`` or
``'constraint'``), and the objective terms combine via the setup-level ``combine``
argument:

- objective + ``combine='weighted'``: a single score adding the weighted discharge
  and mass-balance skills.
- objective + ``combine='pareto'``: a ``[discharge, annual, winter, summer]``
  objective vector for a multi-objective sampler (SPOTPY's NSGAII).
- ``mode='constraint'``: a behavioural pass/fail filter — runs whose mass balance
  is off by more than the signal's ``tolerance`` are rejected; discharge stays the
  objective. With separate signals a run must pass all three filters.

An ``'objective'`` signal scored with an error metric such as ``rmse`` is not handed
to the optimizer as that error: ``SpotpySetup`` maps it to a benchmark skill
(``1 - value/reference``, 1 = perfect, 0 = the mean of that signal's observations),
so every term — discharge KGE included — is a higher-is-better score on a
comparable scale. That is why the Pareto compromise below simply *maximizes* the sum
of the score columns.

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
#
# One signal per balance type rather than a single pooled one. Loading the three
# types into one object would concatenate them into a single vector scored by a
# single RMSE, and that pools observations with very different means.
DISCHARGE_WEIGHT = 1.0  # weight of the discharge term
MB_WEIGHT_TOTAL = 0.5  # total weight of the mass balance against the discharge
# Relative share of MB_WEIGHT_TOTAL per balance type. Shares are renormalized over
# the types actually available, so removing a type reweights the rest instead of
# quietly shrinking the mass-balance term.
MB_WEIGHT_SHARES = {"annual": 2.0, "winter": 1.0, "summer": 1.0}


def load_mass_balance(balance_type, **kwargs):
    """Load one GLAMOS balance type as its own calibration signal."""
    return hb.GlacierMassBalanceObservations.from_glamos(
        GLACIER_MB_WHOLE,
        kind="whole",
        glacier_id="B43-03",
        balance_types=(balance_type,),
        start_date=START_DATE,
        end_date=END_DATE,
        **kwargs,
    )


BALANCE_TYPES = tuple(
    bt for bt in ("annual", "winter", "summer") if len(load_mass_balance(bt)) > 0
)
if not BALANCE_TYPES:
    raise SystemExit("No glacier mass-balance observations in the simulation period.")

# MB_WEIGHT_TOTAL is split between the available types following MB_WEIGHT_SHARES,
# so the mass balance keeps the same overall say against the discharge term whatever
# the source provides. Note these weights act on the 'weighted' combination only:
# 'constraint' signals ignore them (they filter, they do not score), and the Pareto
# vector keeps its components unweighted by construction. They are reused below to
# pick the representative Pareto point, so the same preference is applied when a
# single compromise has to be named.
_share_total = sum(MB_WEIGHT_SHARES[bt] for bt in BALANCE_TYPES)
glacier_mb = [
    load_mass_balance(
        bt,
        metric="rmse",
        weight=MB_WEIGHT_TOTAL * MB_WEIGHT_SHARES[bt] / _share_total,
        mode="objective",
    )
    for bt in BALANCE_TYPES
]
for bt, signal in zip(BALANCE_TYPES, glacier_mb):
    print(
        f"Loaded {len(signal)} {bt} glacier mass-balance observations "
        f"(weight {signal.weight:.3f})."
    )


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
    """Parameters with sensible defaults; the melt and precip inputs are calibrated."""
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
        "rain_correction_factor",
        "snow_correction_factor",
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
    """Re-run the model with the given real parameters and score every signal.

    Returns the discharge KGE (2012) and one glacier mass-balance RMSE per balance
    type, so the combined weighted objective can be read term by term. The discharge
    is sliced by the warmup to match the calibration; the glacier mass-balance
    targets are already restricted to the post-warmup period, so they are compared
    as-is.
    """
    parameters.set_values(param_values)
    model.run(parameters=parameters, forcing=forcing)
    sim_q = model.get_outlet_discharge()
    kge = hb.evaluate(sim_q[WARMUP:], obs.data[0][WARMUP:], "kge_2012")
    rmse = {
        bt: hb.evaluate(signal.simulated(model), signal.observed(), "rmse")
        for bt, signal in zip(BALANCE_TYPES, glacier_mb)
    }
    return kge, rmse


# ---------------------------------------------------------------------------
# 3. Calibrate, comparing the three glacier mass-balance modes
# ---------------------------------------------------------------------------
# Each calibration scores the discharge with the 2012 KGE (inverted for the
# minimizer). The 'weighted' and 'constraint' modes use SCE-UA (single score); the
# 'pareto' mode uses NSGAII (a [discharge, annual, winter, summer] objective
# vector). Each mass-balance signal carries its own metric/weight/mode and is scored
# separately by the setup, whatever the combination: 'objective' signals each
# contribute their own weighted term (or their own component of the Pareto vector),
# 'constraint' signals each reject runs on their own tolerance.
model = build_model()
forcing = build_forcing()
parameters = build_parameters()

# Collects the discharge KGE and per-type glacier mass-balance RMSE of each mode's
# best run for the comparison table printed at the end.
summary = {}


def print_mass_balance_rmse(rmse):
    """Print the per-balance-type mass-balance RMSE of a run."""
    for balance_type in BALANCE_TYPES:
        value = rmse[balance_type]
        print(f"  Glacier MB RMSE, {balance_type:<6} [mm w.e.]: {value:8.1f}")


print("\n=== combine='weighted': single score combining discharge and mass balance ===")
spot_setup = trainer.SpotpySetup(
    model,
    parameters,
    forcing,
    obs,
    warmup=WARMUP,
    obj_func="kge_2012",
    extra_observations=glacier_mb,
    combine="weighted",
    discharge_weight=DISCHARGE_WEIGHT,
)
sampler = trainer.calibrate(spot_setup, "sceua", CALIBRATION_MAX_REP, dbformat="ram")
best = trainer.get_best(sampler)
# Score in skill space (higher is better): the combined discharge + mass-balance
# objective, not a sign-flipped value.
print(f"Best combined objective: {best['score']:.3f}")
print_real_parameters(best["parameters"])
kge_w, rmse_w = evaluate_run(best["parameters"])
print(f"  Discharge KGE (2012):      {kge_w:.3f}")
print_mass_balance_rmse(rmse_w)
summary["weighted"] = {"kge": kge_w, "rmse": rmse_w, "combined": best["score"]}

print("\n=== constraint: reject runs with a poor mass balance ===")
# Keep the discharge KGE as the objective, but reject any run whose mean absolute
# mass-balance error exceeds the tolerance (mm w.e.). The behavioural filter lives
# on the observation object itself (mode='constraint'). Split per balance type, a
# run must satisfy all three filters, and each tolerance is relative to that type's
# own mean absolute observed value (~800 / 1560 / 2370 mm w.e.) instead of the
# pooled one -- so the annual balance, the hardest to reproduce and the most
# informative, is no longer given a tolerance inflated by the summer values.
glacier_mb_constraint = [
    load_mass_balance(
        bt,
        mode="constraint",
        # Accept runs whose mass balance is within 50% of that type's observed mean.
        # A tighter value (e.g. 0.3) can reject the entire random burn-in
        # population, leaving SCE-UA with no fitness gradient to evolve from; loosen
        # it if every run scores the rejection penalty.
        relative_tolerance=0.5,
    )
    for bt in BALANCE_TYPES
]
spot_setup_c = trainer.SpotpySetup(
    model,
    parameters,
    forcing,
    obs,
    warmup=WARMUP,
    obj_func="kge_2012",
    extra_observations=glacier_mb_constraint,
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
    print_mass_balance_rmse(rmse_c)
    summary["constraint"] = {"kge": kge_c, "rmse": rmse_c, "combined": None}
else:
    print("  No behavioural run: skipping (relax the tolerance or run more reps).")

print("\n=== combine='pareto': discharge vs mass balance trade-off (NSGAII) ===")
# Multi-objective: NSGAII returns a Pareto set rather than a single best run. With
# the mass balance split, the objective vector has one component per 'objective'
# signal plus the discharge, i.e. [discharge, annual, winter, summer].
spot_setup_p = trainer.SpotpySetup(
    model,
    parameters,
    forcing,
    obs,
    warmup=WARMUP,
    obj_func="kge_2012",
    extra_observations=glacier_mb,
    combine="pareto",
)
N_OBJECTIVES = 1 + sum(1 for o in glacier_mb if o.mode == "objective")
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
    sample_kwargs={"n_obj": N_OBJECTIVES, "n_pop": NSGAII_POP},
)
results_p = sampler_p.getdata()
print(f"Pareto sampler produced {len(results_p)} evaluations.")

# get_best is undefined for a multi-objective run, so pick one representative point:
# the compromise that maximizes the weighted sum of the skills. get_results returns
# every score in skill space (higher is better) whatever the metric: an error metric
# such as the mass-balance RMSE was turned into a benchmark skill (1 - value
# / reference) before the optimizer ever saw it, so the columns are directly
# comparable and summing them is meaningful -- they are not raw RMSEs to be
# minimized. NSGA-II itself optimized the components unweighted (that is the point
# of a Pareto front); the weights enter only here, to name one point on it with the
# same preference the 'weighted' mode expresses.
df_p = trainer.get_results(sampler_p)
param_names = [c for c in df_p.columns if not c.startswith("score")]
score_cols = [f"score{i + 1}" for i in range(N_OBJECTIVES)]
score_weights = [DISCHARGE_WEIGHT] + [signal.weight for signal in glacier_mb]
idx_p = (df_p[score_cols] * score_weights).sum(axis=1).idxmax()
pareto_params = {n: float(df_p.loc[idx_p, n]) for n in param_names}
print("Representative Pareto point (max weighted sum of the skills):")
print_real_parameters(pareto_params)
kge_p, rmse_p = evaluate_run(pareto_params)
print(f"  Discharge KGE (2012):      {kge_p:.3f}")
print_mass_balance_rmse(rmse_p)
summary["pareto*"] = {"kge": kge_p, "rmse": rmse_p, "combined": None}

# ---------------------------------------------------------------------------
# Summary: compare the discharge KGE and glacier MB RMSE across the three modes
# ---------------------------------------------------------------------------
print("\n=== Summary: discharge KGE and glacier MB RMSE by mode ===")
header = f"{'Mode':<12} {'Discharge KGE':>14}"
for balance_type in BALANCE_TYPES:
    header += f" {balance_type + ' RMSE':>14}"
print(header + f" {'Combined':>10}")
for mode, m in summary.items():
    combined = f"{m['combined']:.3f}" if m["combined"] is not None else "-"
    row = f"{mode:<12} {m['kge']:>14.3f}"
    for balance_type in BALANCE_TYPES:
        row += f" {m['rmse'][balance_type]:>14.1f}"
    print(row + f" {combined:>10}")
print("RMSE in mm w.e., one column per GLAMOS balance type (separate signals)")
print("* pareto: one representative point (max of the summed skills) on the front")

# ---------------------------------------------------------------------------
# 4. Inspect the best 'weighted' run: observed vs simulated mass balance
# ---------------------------------------------------------------------------
# Re-run the model with the best parameter set and compute the simulated mass
# balance to compare it with the observations.
parameters.set_values(best["parameters"])
model.run(parameters=parameters, forcing=forcing)

# One panel per available balance type, sharing the x axis. Each target is placed on
# the hydrological year it *ends* in: for the fixed-date GLAMOS periods that is the
# same year for the three types (annual Oct Y-1 to Sep Y, winter Oct Y-1 to Apr Y,
# summer Apr Y to Sep Y), whereas the period start would put the summer balance one
# year off the other two.
fig, axes = plt.subplots(
    len(BALANCE_TYPES),
    1,
    sharex=True,
    figsize=(12, 3.2 * len(BALANCE_TYPES)),
    squeeze=False,
)
colors = {"annual": "tab:blue", "winter": "tab:cyan", "summer": "tab:red"}
for ax, balance_type, signal in zip(axes[:, 0], BALANCE_TYPES, glacier_mb):
    sim_mb = signal.simulated(model)
    years = [t["t1"].year for t in signal.targets]
    obs_vals = [t["value"] for t in signal.targets]
    ax.plot(years, obs_vals, "o-", color="black", label="Observed (GLAMOS)")
    ax.plot(
        years,
        sim_mb,
        "s--",
        color=colors.get(balance_type, "tab:green"),
        label="Simulated",
    )
    ax.axhline(0, color="gray", linewidth=0.8)
    ax.set_title(
        f"{balance_type.capitalize()} balance "
        f"(RMSE {rmse_w[balance_type]:.0f} mm w.e.)"
    )
    ax.set_ylabel("Mass balance [mm w.e.]")
    ax.legend(loc="best", fontsize="small")
axes[-1, 0].set_xlabel("Hydrological year (Oct-Sep, labelled by its end year)")
fig.suptitle("Rhonegletscher glacier mass balance, best 'weighted' run")
fig.tight_layout()
plt.show()

hb.close_log()
