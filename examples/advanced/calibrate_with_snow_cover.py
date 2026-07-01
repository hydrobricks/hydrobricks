"""Calibrate a hydrological model on discharge AND observed snow cover (MODIS).

Remote-sensing snow cover fraction (here the MODIS/Terra daily snow product,
MOD10A1) is an independent constraint on the snow accumulation and melt
parameters. This example calibrates a SOCONT model on the Sitter @ Appenzell
catchment using both the observed discharge and the observed per-hydro-unit snow
cover fraction, scored by RMSE.

From SWE to a snow-cover fraction
---------------------------------
The model stores only the snow water equivalent (SWE) per hydro unit; it has no
explicit snow-cover-fraction state. The covered fraction is therefore derived from
the recorded SWE at evaluation time through a linear depletion curve
``f = min(1, SWE / swe_full)`` (everything happens on the Python side; the
simulation itself is unchanged). ``swe_full`` is the SWE at which a unit is fully
snow-covered (a fixed, configurable argument of the observation).

Reading the MODIS data
----------------------
MOD10A1 tiles are HDF-EOS files in the MODIS sinusoidal projection, one date per
file (the date is in the file name, e.g. ``A2025361`` = 2025 day-of-year 361).
``SnowCoverObservations.from_modis`` reads the ``NDSI_Snow_Cover`` field, mosaics
the tiles of a date (here a single tile, h18v04, covers the catchment), reprojects
to the catchment CRS using the catchment's ``unit_ids.tif`` raster, and aggregates
per hydro unit. The product encodes clouds / water / no-decision as values above
100, so ``valid_max=100`` drops those quality codes and ``value_scale=0.01``
rescales the remaining 0-100 % NDSI snow cover to a 0-1 fraction. Reading uses
xarray's ``netcdf4`` engine (no separate HDF4/GDAL build needed).

Note: this script runs a calibration and can take a while. ``CALIBRATION_MAX_REP``
is kept small for a quick look; raise it for a real calibration.
"""

import logging
import sys
from collections import defaultdict
from pathlib import Path

import matplotlib.pyplot as plt
import numpy as np
import pandas as pd

import hydrobricks as hb
import hydrobricks.trainer as trainer
from examples._helpers.models_setup_helper import ModelSetupHelper

logging.basicConfig(
    level=logging.INFO,
    stream=sys.stdout,
    force=True,
    format="%(levelname)s - %(name)s - %(message)s",
)

# --------------------------------------------------------------------------- #
# Configuration
# --------------------------------------------------------------------------- #
CATCHMENT = "ch_sitter_appenzell"
START_DATE = "2003-01-01"
END_DATE = "2014-12-31"
REF_ELEVATION = 1250  # Reference altitude of the meteorological station [m]
WARMUP = 365
CALIBRATION_MAX_REP = 300  # Increase for a real calibration

# SWE [mm w.e.] at which a hydro unit is considered fully snow-covered (the
# threshold of the SWE -> cover-fraction depletion curve).
SWE_FULL = 100.0

# MODIS snow-cover data (MOD10A1, HDF-EOS). The full archive covering the
# simulation period lives on the institute share; a few sample tiles are bundled in
# the repository's tmp/ folder so this example runs out of the box (those sample
# dates fall outside the discharge period, so the script then only demonstrates the
# loading/aggregation rather than a full calibration).
MODIS_DIR = Path("path/to/data")
MODIS_FILE_PATTERN = "MOD10A1*.hdf"

# --------------------------------------------------------------------------- #
# 1. Catchment, hydro units, model, forcing, discharge
# --------------------------------------------------------------------------- #
helper = ModelSetupHelper(CATCHMENT, start_date=START_DATE, end_date=END_DATE)
helper.create_hydro_units_from_csv_file(filename="hydro_units_elevation.csv")
hydro_units = helper.hydro_units
catchment_dir = helper.get_catchment_dir()
unit_ids_raster = catchment_dir / "unit_ids.tif"

forcing = helper.get_forcing_data_from_csv_file(
    ref_elevation=REF_ELEVATION, use_precip_gradient=True
)
discharge = helper.get_obs_data_from_csv_file()
# record_all=True so the snowpack SWE (and land-cover fractions) are recorded and the
# simulated snow cover can be read from memory each iteration.
socont, parameters = helper.get_model_and_params_socont(record_all=True)

parameters.allow_changing = [
    "a_snow",
    "k_quick",
    "A",
    "k_slow_1",
    "percol",
    "k_slow_2",
    "precip_corr_factor",
    "temp_gradients",
]

# --------------------------------------------------------------------------- #
# 2. Observed snow cover (MODIS), aggregated per hydro unit
# --------------------------------------------------------------------------- #
if not MODIS_DIR.exists():
    print(
        f"MODIS data not found at {MODIS_DIR}.\n"
        "Update MODIS_DIR to point at the MOD10A1 tiles, then re-run."
    )
    sys.exit(0)

print(f"Loading and aggregating the MODIS snow cover from {MODIS_DIR} ...")
snow_cover = hb.SnowCoverObservations.from_modis(
    MODIS_DIR,
    raster_hydro_units=unit_ids_raster,
    hydro_units=hydro_units,
    file_pattern=MODIS_FILE_PATTERN,
    variable="NDSI_Snow_Cover",
    value_scale=0.01,  # 0-100 % -> 0-1
    valid_max=100,  # drop quality/error codes (> 100)
    valid_min=0,
    min_valid_ratio=0.5,  # require half of a unit's pixels cloud-free on a date
    start_date=START_DATE,  # only read tiles within the simulation period
    end_date=END_DATE,
    swe_full=SWE_FULL,
    metric="rmse",  # the default; lower error -> higher skill internally
    weight=1.0,
    mode="objective",
)
print(f"Loaded {len(snow_cover)} (unit, date) snow-cover observations.")

# Do the observation dates overlap the simulation period?
in_period = [
    t
    for t in snow_cover.targets
    if pd.Timestamp(START_DATE) <= t["t"] <= pd.Timestamp(END_DATE)
]

# --------------------------------------------------------------------------- #
# 3a. Sample data only: show the observed snow cover vs elevation
# --------------------------------------------------------------------------- #
if not in_period:
    print(
        "The available MODIS dates fall outside the simulation period, so this run "
        "only demonstrates the loading/aggregation (no calibration). Point MODIS_DIR "
        "at an archive covering the simulation period for a full calibration."
    )
    hu = pd.read_csv(catchment_dir / "hydro_units_elevation.csv", skiprows=[1])
    elev_by_id = dict(zip(hu["id"].astype(int), hu["elevation"].astype(float)))

    fig, ax = plt.subplots(figsize=(9, 6))
    for date in sorted({t["t"] for t in snow_cover.targets}):
        rows = [
            (elev_by_id[t["unit_id"]], t["value"])
            for t in snow_cover.targets
            if t["t"] == date and t["unit_id"] in elev_by_id
        ]
        rows.sort()
        if rows:
            elev, scf = zip(*rows)
            ax.plot(scf, elev, "o-", markersize=4, label=str(date.date()))
    ax.set_xlabel("Observed snow cover fraction [-]")
    ax.set_ylabel("Hydro-unit elevation [m a.s.l.]")
    ax.set_title(f"{CATCHMENT}: MODIS snow cover vs elevation")
    ax.legend(title="Date")
    fig.tight_layout()
    plt.show()
    hb.close_log()
    sys.exit(0)

# --------------------------------------------------------------------------- #
# 3b. Calibrate on discharge + snow cover (weighted combination)
# --------------------------------------------------------------------------- #
# Discharge stays the primary signal; the snow cover is an auxiliary 'objective'
# term combined into a single score (discharge KGE + weighted snow-cover skill).
spot_setup = trainer.SpotpySetup(
    socont,
    parameters,
    forcing,
    discharge,
    warmup=WARMUP,
    obj_func="kge_2012",
    extra_observations=[snow_cover],
    combine="weighted",
    discharge_weight=1.0,
)
sampler = trainer.calibrate(spot_setup, "sceua", CALIBRATION_MAX_REP, dbformat="ram")
best = trainer.get_best(sampler)
print(f"Best combined objective (discharge + snow cover): {best['score']:.3f}")
print("Best parameters:", best["parameters"])

# --------------------------------------------------------------------------- #
# 4. Inspect the best run: observed vs simulated catchment-mean snow cover
# --------------------------------------------------------------------------- #
parameters.set_values(best["parameters"])
socont.run(parameters=parameters, forcing=forcing)
sim = snow_cover.simulated(socont)
obs = snow_cover.observed()

# Average the per-unit values to a catchment-mean snow cover per date for plotting.
obs_by_date = defaultdict(list)
sim_by_date = defaultdict(list)
for target, o, s in zip(snow_cover.targets, obs, sim):
    if np.isfinite(o) and np.isfinite(s):
        obs_by_date[target["t"]].append(o)
        sim_by_date[target["t"]].append(s)

dates = sorted(obs_by_date)
obs_mean = [float(np.mean(obs_by_date[d])) for d in dates]
sim_mean = [float(np.mean(sim_by_date[d])) for d in dates]

fig, ax = plt.subplots(figsize=(14, 6))
ax.plot(dates, obs_mean, ".", color="black", markersize=3, label="Observed (MODIS)")
ax.plot(dates, sim_mean, "-", color="tab:blue", linewidth=1, label="Simulated")
ax.set_title(f"{CATCHMENT}: catchment-mean snow cover fraction")
ax.set_xlabel("Date")
ax.set_ylabel("Snow cover fraction [-]")
ax.set_ylim(-0.02, 1.02)
ax.legend()
fig.tight_layout()
plt.show()

hb.close_log()
