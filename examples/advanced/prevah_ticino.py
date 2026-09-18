"""PREVAH on the Ticino-Bellinzona catchment (570 HRUs, distributed).

A real, spatially-distributed PREVAH setup: 570 hydrotopes (HRUs) with three land
covers (open / forest / wetland), the forcing given per meteo zone and read per HRU,
PREVAH's **monthly vegetation tables** applied per cover (``apply_land_use``), and a
soil moisture capacity that varies per hydrotope *and* per month
(``apply_land_use_field_capacity``).

The whole setup -- model, hydro units, forcing, periods and the calibrated parameters
-- is declared in ``prevah_ticino_project.yaml``. Only PREVAH's land-use tables are
left to Python: they are model-specific calls that the project-file schema does not
cover. So the file is loaded with ``setup=False``, the tables are applied, and the
model is set up and run.

The reference discharge (``discharge_prevah.csv``) is the *Fortran PREVAH* simulated
total runoff for this case (not a gauge series), so this example is a
cross-implementation reproduction: hydrobricks PREVAH-UniBE against the original Fortran
PREVAH (1984-2000). It reaches NSE ~= 0.98 over the validation period.

Every parameter of the project file is the original PREVAH one (``cal_2020pest.inp``)
and every option follows the method switches of that control file; **nothing is
calibrated here**. The processes are the faithful ones throughout: the vapour-density
Hamon PET, the albedo-reduced soil, canopy and snow evaporation, the PREVAH snow water
release (CEXLIQ) and the wet-surface evaporation from the groundwater.

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

HERE = Path(__file__).parent
DATA = HERE.parent.parent / "tests" / "files" / "catchments" / "ch_ticino_bellinzona"

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
# 1. The project file: model, hydro units, forcing, periods, parameters.
#    setup=False, because the land-use tables below must be applied first.
# ---------------------------------------------------------------------------
project = hb.load_project(HERE / "prevah_ticino_project.yaml", setup=False)
model = project.model
parameters = project.parameters
hydro_units = project.hydro_units
periods = project.periods


def unit_column(name):
    """One value per hydro unit, from a column declared in hydro_units.columns."""
    return hydro_units.hydro_units[name].to_numpy().flatten()


# ---------------------------------------------------------------------------
# 2. PREVAH's land-use tables, the part the project file cannot express
# ---------------------------------------------------------------------------
# The soil moisture capacity: PREVAH builds it from the soil (available water content
# and depth) and from the rooting depth of the hydrotope's land use, which varies by
# month, so the capacity varies in space and through the year at once. The soil depth
# caps the rooting depth per unit, which is why the two cannot be separated into a
# per-unit value times a shared monthly shape.
land_use = unit_column("land_use")
model.apply_land_use_field_capacity(
    parameters,
    hydro_units,
    land_use,  # one land use per hydrotope
    available_water_content=unit_column("awc"),
    soil_depth=unit_column("soil_depth"),
)

# The percolation: PREVAH lets the built-up and rock hydrotopes whose soil map promises
# more than their fixed 5 mm or 3 mm capacity percolate at a constant rate while snow
# free. Its conductivity factor KWPER would need the soil conductivity, but PREVAH sets
# it to 1 as soon as a hydrotope has none, as some do here, so it is left out.
model.apply_land_use_percolation(
    parameters,
    hydro_units,
    land_use,
    available_water_content=unit_column("awc"),
    soil_depth=unit_column("soil_depth"),
)

# The wet-surface evaporation needs nothing here: the model reads the wet share of
# each unit from its wetland covers (their area fraction times the 'wet_fraction' of
# the project file), which on this dataset is 0.7 on a wetland hydrotope and 0
# elsewhere, as PREVAH's wet_surface is.

# The monthly vegetation tables: the canopy interception capacity of each cover
# (si_max x veg_cov) and its canopy evaporation factor (veg_cov), month by month.
for cover, cover_land_use in LAND_USES.items():
    model.apply_land_use(parameters, cover_land_use, cover_name=cover)

# ---------------------------------------------------------------------------
# 3. Run
# ---------------------------------------------------------------------------
project.setup()
project.run()

observations = project.observations

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
# 4. Evaluate against the Fortran PREVAH reference, per period
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
# 5. Plot the daily hydrograph (a sample year) and the monthly climatology
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
    figure = project.output_dir / "prevah_ticino.png"
    fig.savefig(figure, dpi=130)
    print(f"saved {figure}")
except ImportError:
    print("(matplotlib not available; skipping the plot)")
