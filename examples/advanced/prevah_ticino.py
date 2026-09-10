"""PREVAH on the Ticino-Bellinzona catchment (570 HRUs, distributed).

A real, spatially-distributed PREVAH setup: 570 hydrotopes (HRUs) with three land
covers (open / forest / wetland), meteo-zone forcing, a **per-HRU field capacity**
taken from the soil data via a spatial parameter (``set_spatial``), and PREVAH's
**monthly vegetation tables** applied per cover (``apply_land_use``) and a soil
moisture capacity that varies per hydrotope *and* per month
(``apply_land_use_field_capacity``).

The reference discharge (``discharge_prevah.csv``) is the *Fortran PREVAH* simulated
total runoff for this case (not a gauge series), so this example is a
cross-implementation reproduction: hydrobricks PREVAH-UniBE against the original Fortran
PREVAH (1984-2000). It reaches NSE ~= 0.98 over the validation period.

Every parameter below is the original PREVAH one (``cal_2020pest.inp``) and every
option follows the method switches of that control file; **nothing is calibrated
here**. The processes are the faithful ones throughout: the vapour-density Hamon PET,
the albedo-reduced soil, canopy and snow evaporation, the PREVAH snow water release
(CEXLIQ) and the wet-surface evaporation from the groundwater.

The snowmelt is the radiation-corrected (Hock) one selected by the control file. It
is driven by ``potential_radiation.csv``, the potential clear-sky radiation of each
hydrotope for every day of the year, which PREVAH derives from the latitude, slope and
aspect of the hydrotope with no terrain shading. That table comes with the dataset
because the calibrated radiation melt factor of the control file multiplies exactly
that quantity: a radiation computed from a DEM is a different measure (it carries the
atmospheric attenuation and another daily-integration convention) and would need its
own coefficient.

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

# PREVAH parameterizes its vegetation through monthly tables, one per land use.
# Each cover of this dataset aggregates several of them (area shares of the source
# hydrotope table), so each is given the land use that represents it best:
#   forest  = 60 % coniferous, 29 % deciduous, 9 % mixed  -> the dominant one
#   wetland = 97 % wetland                                -> the dominant one
#   open    = 39 % rough pasture, 30 % rock, 18 % bare soil, 7 % pasture, 5 % urban.
#             No land use dominates this catch-all, so it takes the one closest to
#             the mixture's vegetation cover (0.43 in winter, 0.49 in summer):
#             alpine meadow, at 0.5 / 0.6. Rough pasture, the largest single class,
#             would be far too lush at 0.7 / 0.8.
LAND_USES = {
    "open": "alpine_meadow",
    "forest": "coniferous_forest",
    "wetland": "wetland",
}

# ---------------------------------------------------------------------------
# 1. Hydro units (570 HRUs): elevation, per-cover area, per-HRU soil
# ---------------------------------------------------------------------------
hydro_units = hb.HydroUnits(land_cover_types=COVERS, land_cover_names=COVERS)
hydro_units.load_from_csv(
    DATA / "hydro_units.csv",
    column_elevation="elevation",
    columns_areas={c: f"area_{c}" for c in COVERS},
)

# The remaining columns are read alongside: the soil data (available water content,
# soil depth) and the land use of each hydrotope, which together give the soil
# moisture capacity further down, and mez to map each HRU to its forcing series.
units_df = pd.read_csv(DATA / "hydro_units.csv", header=0, skiprows=[1])
mez = units_df["mez"].to_numpy()

# PREVAH's wet surfaces evaporate from the groundwater at et_pot * wet_surface
# (0.7 on wetlands, 0.9 on open water, 0 elsewhere). Each hydrotope of this dataset
# carries a single cover, so the fraction follows the wetland area.
wet = np.where(units_df["area_wetland"].to_numpy() > 0, 0.7, 0.0)
hydro_units.add_property(("wet", "-"), wet)

# ---------------------------------------------------------------------------
# 2. Forcing: each HRU reads its meteo-zone precipitation and temperature series,
#    plus the potential radiation of its own slope and aspect (day of the year).
#    PET is the vapour-density Hamon used by PREVAH (the default 'Hamon' of pyet is
#    an exponential variant that runs markedly hotter).
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
forcing.compute_pet(method="Hamon_vapor_density", use=["t", "lat"], lat=LATITUDE)

# Potential radiation: one row per day of the year, one column per hydro unit.
rad = pd.read_csv(DATA / "potential_radiation.csv")
rad_by_doy = rad.drop(columns="day_of_year").to_numpy()
doys = pd.DatetimeIndex(precip["date"]).dayofyear.to_numpy()
forcing.data2D.data_name.append(forcing.Variable.R_SOLAR)
forcing.data2D.data.append(rad_by_doy[doys - 1, :])  # (n_time, n_units)

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

# The gauged discharge of the Ticino at Bellinzona, for reference: the point of this
# example is the agreement with the Fortran, but both models face the same reality.
measured = hb.DischargeObservations(periods.simulation.start, periods.simulation.end)
measured.load_from_csv(
    DATA / "discharge.csv",
    column_time="date",
    time_format="%d/%m/%Y",
    content={"discharge": "discharge_spec(mm/d)"},
)

# ---------------------------------------------------------------------------
# 4. The PREVAH model with the calibrated parameters
# ---------------------------------------------------------------------------
# Calibrated PREVAH parameters (cal_2020pest.inp), mapped to hydrobricks aliases.
# PREVAH storage times are in hours; hydrobricks uses response factors k = 24 / K_h.
model = models.PrevahUniBE(
    land_cover_names=COVERS,
    land_cover_types=COVERS,
    # PREVAH intercepts on every vegetated cover and evaporates the canopy at
    # et_pot * veg_cov, so the covers all carry a canopy with the PREVAH canopy ET.
    interception_covers=COVERS,
    canopy_et_process="et:open_water_prevah",
    # Radiation-corrected melt: (CSNOMF + CASNO * R_pot) * (T - T0). The refreezing
    # then needs its own seasonal factor, the melt process no longer carrying one.
    snow_melt_process="melt:temperature_index",
    snow_refreezing_process="refreeze:degree_day_seasonal",
    # PREVAH reduces the potential rate by the surface albedo, (1 - albedo)/0.8, on
    # the soil, the canopy and the snow alike; the snow albedo ages between snowfalls.
    soil_et_process="et:prevah",
    snow_sublimation_process="sublimation:prevah",
    # Snow water release of the ablation branch, with the CEXLIQ graded partition.
    snow_water_retention_process="outflow:snow_holding_prevah",
    # Wet-surface evaporation drawn from the groundwater store (PREVAH's EWET).
    wet_et_from_groundwater=True,
    record_all=True,
)
parameters = model.generate_parameters()
# PREVAH melts from -1 degC; the default range of the melt threshold starts at 0.
parameters.change_range("melt_t_snow", -3.0, 5.0)
parameters.set_values(
    {
        # precipitation / snow correction factors
        "rfcf": 1.0 - 22.363 / 100.0,
        "sfcf": 1.0 + 33.502 / 100.0,
        # snow/rain linear transition (all snow below t_start, all rain above t_end)
        "prec_t_start": -0.75,
        "prec_t_end": 0.75,
        # radiation-corrected snow melt (CSNOMF, CASNO)
        "melt_factor": 1.0038092221,
        "r_snow": 5.5527817e-5,
        "melt_t_snow": -1.0,
        # refreezing: CRFR times the seasonal factor between TMFMIN and TMFMAX
        "cwh": 0.1,
        "cfr": 0.1,
        "cfr_ddf_min": 1.0,
        "cfr_ddf_max": 2.0,
        "cfr_melt_t": -1.0,
        # snow water release: the retention collapses above the melt threshold, and
        # CEXLIQ grades how much of the fresh melt passes straight through.
        "holding_melt_t": -1.0,
        "cexliq": 0.5,
        # soil moisture / ET (per-cover beta, all at the calibrated CBETA)
        "fc": 13.7,  # global fallback; the per-HRU monthly capacity overrides it
        "beta_open": 0.5,
        "beta_forest": 0.5,
        "beta_wetland": 0.5,
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
# The control file selects the Hamon evapotranspiration method, whose branch in
# PREVAH evaporates the soil at the potential rate with no soil-moisture limitation.
# Setting the CU limit to ~0 reproduces that (the range has to be opened first).
parameters.change_range("cu", 0.0, 1.0)
parameters.set_values({"cu": 1e-6})

# The headline: each HRU uses its own field capacity instead of one global value.
# PREVAH builds it from the soil (available water content and depth) and from the
# rooting depth of the hydrotope's land use, which varies by month, so the capacity
# varies in space and through the year at once. The soil depth caps the rooting
# depth per unit, which is why the two cannot be separated into a per-unit value
# times a shared monthly shape.
model.apply_land_use_field_capacity(
    parameters,
    hydro_units,
    units_df["land_use"].to_numpy(),  # one land use per hydrotope
    available_water_content=units_df["awc"].to_numpy(),
    soil_depth=units_df["soil_depth"].to_numpy(),
)
parameters.set_spatial("ow_et_factor", "wet")

# PREVAH's monthly vegetation tables: the canopy interception capacity of each cover
# (si_max x veg_cov) and its canopy evaporation factor (veg_cov), month by month.
for cover, land_use in LAND_USES.items():
    model.apply_land_use(parameters, land_use, cover_name=cover)

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
print("\nhydrobricks PREVAH-UniBE vs Fortran PREVAH (Ticino-Bellinzona):")
print(scores.round(3))

measured_scores = hb.evaluate_periods(
    model, measured, periods, metrics=("nse", "kge_2012")
)
print("\nhydrobricks PREVAH-UniBE vs the gauged discharge:")
print(measured_scores.round(3))

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
    gauge = np.asarray(measured.data[0])
    df = pd.DataFrame({"sim": sim, "obs": obs, "gauge": gauge}, index=time)

    fig, ax = plt.subplots(1, 2, figsize=(14, 4))
    yr = df.loc["1998"]
    ax[0].plot(yr.index, yr["gauge"], color="0.65", lw=1.0, label="measured")
    ax[0].plot(yr.index, yr["obs"], "k-", lw=1.0, label="Fortran PREVAH")
    ax[0].plot(yr.index, yr["sim"], "C1-", lw=0.9, label="hydrobricks PREVAH-UniBE")
    ax[0].set_title("Daily discharge — 1998")
    ax[0].set_ylabel("mm/d")
    ax[0].legend(fontsize=8)

    clim = df.dropna().groupby(df.dropna().index.month).mean()
    ax[1].plot(clim.index, clim["gauge"], "^-", color="0.65", label="measured")
    ax[1].plot(clim.index, clim["obs"], "ks-", label="Fortran PREVAH")
    ax[1].plot(clim.index, clim["sim"], "C1o-", label="hydrobricks PREVAH-UniBE")
    ax[1].set_title("Monthly mean discharge")
    ax[1].set_xlabel("month")
    ax[1].set_ylabel("mm/d")
    ax[1].legend(fontsize=8)
    fig.tight_layout()
    fig.savefig(OUT / "prevah_ticino.png", dpi=130)
    print(f"saved {OUT / 'prevah_ticino.png'}")
except ImportError:
    print("(matplotlib not available; skipping the plot)")
