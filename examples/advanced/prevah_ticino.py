"""PREVAH on the Ticino-Bellinzona catchment (570 HRUs, distributed).

A real, spatially-distributed PREVAH setup: 570 hydrotopes (HRUs) with three land
covers (open / forest / wetland), meteo-zone forcing, and a **per-HRU field
capacity** taken from the soil data via a spatial parameter (``set_spatial``).

The reference discharge (``discharge_prevah.csv``) is the *Fortran PREVAH* simulated
total runoff for this case (not a gauge series), so this example is a
cross-implementation reproduction: hydrobricks-Prevah against the original Fortran
PREVAH (1984-2000). It reaches NSE ~= 0.93 over the validation period.

The data lives in ``tests/files/catchments/ch_ticino_bellinzona/`` (see its
``_readme.txt`` for provenance). Run from anywhere:

    python examples/advanced/prevah_ticino.py
"""

from pathlib import Path

import numpy as np
import pandas as pd

import hydrobricks as hb
import hydrobricks.models as models

DATA = (
    Path(__file__).parent.parent.parent
    / "tests"
    / "files"
    / "catchments"
    / "ch_ticino_bellinzona"
)
OUT = Path(__file__).parent / "output"
OUT.mkdir(exist_ok=True)

COVERS = ["open", "forest", "wetland"]
LATITUDE = 46.4  # catchment latitude, for the Hamon PET

# ---------------------------------------------------------------------------
# 1. Hydro units (570 HRUs): elevation, per-cover area, per-HRU field capacity
# ---------------------------------------------------------------------------
hydro_units = hb.HydroUnits(land_cover_types=COVERS, land_cover_names=COVERS)
hydro_units.load_from_csv(
    DATA / "hydro_units.csv",
    column_elevation="elevation",
    columns_areas={c: f"area_{c}" for c in COVERS},
)

# The fc (field capacity) and mez (meteo zone) columns are read alongside: fc as a
# per-unit spatial parameter, mez to map each HRU to its meteo-zone forcing series.
units_df = pd.read_csv(DATA / "hydro_units.csv", header=0, skiprows=[1])
hydro_units.add_property(("fc", "mm"), units_df["fc"].to_numpy())
mez = units_df["mez"].to_numpy()

# ---------------------------------------------------------------------------
# 2. Forcing: each HRU reads its meteo-zone precipitation and temperature series;
#    PET is computed with the Hamon method (pyet).
# ---------------------------------------------------------------------------
precip = pd.read_csv(DATA / "precipitation.csv", parse_dates=["date"])
temp = pd.read_csv(DATA / "temperature.csv", parse_dates=["date"])
zone_cols = [c for c in precip.columns if c != "date"]  # zone id -> column
zone_index = {int(z): i for i, z in enumerate(zone_cols)}
col = np.array([zone_index[z] for z in mez])  # HRU -> zone column index

p_zone = precip[zone_cols].to_numpy()
t_zone = temp[zone_cols].to_numpy()

forcing = hb.Forcing(hydro_units)
forcing.data2D.time = precip["date"]
forcing.data2D.data_name = [forcing.Variable.P, forcing.Variable.T]
forcing.data2D.data = [p_zone[:, col], t_zone[:, col]]  # (n_time, n_units)
forcing.compute_pet(method="Hamon", use=["t", "lat"], lat=LATITUDE)
forcing.apply_operations()

# ---------------------------------------------------------------------------
# 3. Observations (Fortran PREVAH reference discharge)
# ---------------------------------------------------------------------------
periods = hb.Periods(
    calibration=("1985-01-01", "1994-12-31"),
    validation=("1995-01-01", "2000-12-31"),
    spinup="1y",
)
observations = hb.DischargeObservations(
    periods.simulation.start, periods.simulation.end
)
observations.load_from_csv(
    DATA / "discharge_prevah.csv",
    column_time="date",
    time_format="%Y-%m-%d",
    content={"discharge": "discharge (mm/d)"},
)

# ---------------------------------------------------------------------------
# 4. The PREVAH model with the calibrated parameters
# ---------------------------------------------------------------------------
# Calibrated PREVAH parameters (cal_2020pest.inp), mapped to hydrobricks aliases.
# PREVAH storage times are in hours; hydrobricks uses response factors k = 24 / K_h.
parameters = models.Prevah(
    land_cover_names=COVERS, land_cover_types=COVERS
).generate_parameters()
parameters.set_values(
    {
        # precipitation / snow correction factors
        "rfcf": 1.0 - 22.363 / 100.0,
        "sfcf": 1.0 + 33.502 / 100.0,
        # snow/rain linear transition (all snow below t_start, all rain above t_end)
        "prec_t_start": -0.75,
        "prec_t_end": 0.75,
        # seasonal degree-day snow melt
        "a_snow_min": 1.0,
        "a_snow_max": 2.0,
        "melt_t_snow": -1.0,
        "cwh": 0.1,
        "cfr": 0.1,
        # pyet-Hamon runs hotter than PREVAH's Hamon; a modest snow-evaporation factor
        # avoids doubling the snow ablation loss.
        "sublimation_pet_factor": 0.3,
        # soil moisture / ET (per-cover beta, all at the calibrated CBETA)
        "fc": 13.7,  # global fallback; the per-HRU fc property overrides this
        "beta_open": 0.5,
        "beta_forest": 0.5,
        "beta_wetland": 0.5,
        "cu": 0.7,
        "wet_fraction": 0.7,  # PREVAH wetland wet-surface fraction
        # upper zone (surface runoff Q0 threshold, interflow Q1)
        "k0": 24.0 / 29.146,
        "sgrluz": 74.655,
        "k1": 24.0 / 150.0,
        # soil-moisture-gated percolation (mm/h -> mm/d)
        "cperc": 0.23576 * 24.0,
        "cu_perc": 0.7,
        # SLOWCOMP three-store groundwater
        "slz1max": 112.903,
        "k_gw1": 24.0 / 1000.0,
        "k_gw2": 24.0 / 3009.72,
        "k_gw3": 24.0 / 9000.0,
    }
)
# The headline: each HRU uses its own field capacity from the soil data (the `fc`
# property) instead of one global value.
parameters.set_spatial("fc", "fc")

model = models.Prevah(land_cover_names=COVERS, land_cover_types=COVERS, record_all=True)
model.setup(
    spatial_structure=hydro_units,
    output_path=str(OUT),
    period=periods.simulation,
    spinup=periods.spinup,
)
model.run(parameters=parameters, forcing=forcing)

# ---------------------------------------------------------------------------
# 5. Evaluate against the Fortran PREVAH reference, per period
# ---------------------------------------------------------------------------
scores = hb.evaluate_periods(model, observations, periods, metrics=("nse", "kge_2012"))
print("\nhydrobricks-Prevah vs Fortran PREVAH (Ticino-Bellinzona):")
print(scores.round(3))

# ---------------------------------------------------------------------------
# 6. Plot the daily hydrograph (a sample year) and the monthly climatology
# ---------------------------------------------------------------------------
try:
    import matplotlib

    matplotlib.use("Agg")
    import matplotlib.pyplot as plt

    time = pd.to_datetime(model.get_recorded_time())
    sim = np.asarray(model.get_outlet_discharge())
    obs = np.asarray(observations.data[0])
    df = pd.DataFrame({"sim": sim, "obs": obs}, index=time)

    fig, ax = plt.subplots(1, 2, figsize=(14, 4))
    yr = df.loc["1998"]
    ax[0].plot(yr.index, yr["obs"], "k-", lw=1.0, label="Fortran PREVAH")
    ax[0].plot(yr.index, yr["sim"], "C1-", lw=0.9, label="hydrobricks-Prevah")
    ax[0].set_title("Daily discharge — 1998")
    ax[0].set_ylabel("mm/d")
    ax[0].legend(fontsize=8)

    clim = df.dropna().groupby(df.dropna().index.month).mean()
    ax[1].plot(clim.index, clim["obs"], "ks-", label="Fortran PREVAH")
    ax[1].plot(clim.index, clim["sim"], "C1o-", label="hydrobricks-Prevah")
    ax[1].set_title("Monthly mean discharge")
    ax[1].set_xlabel("month")
    ax[1].set_ylabel("mm/d")
    ax[1].legend(fontsize=8)
    fig.tight_layout()
    fig.savefig(OUT / "prevah_ticino.png", dpi=130)
    print(f"saved {OUT / 'prevah_ticino.png'}")
except ImportError:
    print("(matplotlib not available; skipping the plot)")
