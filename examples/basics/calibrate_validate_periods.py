"""Split-sample calibration/validation with explicit periods.

This example shows the recommended period workflow:

1. Declare the periods once (``hb.Periods``): a calibration period, a validation
   period and the spin-up policy. The spin-up replays the first years of a model's
   own period (unlogged) to initialize the stores, so the *whole* declared period
   is evaluated — no warmup year is discarded from the observations.
2. Calibrate on the calibration period only (cheap runs).
3. Re-run the best parameters once over the full span (warm, continuous states)
   and read the calibration/validation scores from ``hb.evaluate_periods``.
"""

import logging
import sys

import matplotlib.pyplot as plt

import hydrobricks as hb
import hydrobricks.trainer as trainer
from examples._helpers.models_setup_helper import ModelSetupHelper

logging.basicConfig(
    level=logging.INFO,
    stream=sys.stdout,
    force=True,
    format="%(levelname)s - %(name)s - %(message)s",
)

# ---------------------------------------------------------------------------
# 1. Declare the periods
# ---------------------------------------------------------------------------
periods = hb.Periods(
    calibration=("1981-01-01", "2000-12-31"),
    validation=("2001-01-01", "2020-12-31"),
    spinup="4y",  # replay the first 4 years of each period to warm the stores
)


def build(period, spinup):
    """Build a SOCONT model, forcing and observations over the given period."""
    helper = ModelSetupHelper(
        "ch_sitter_appenzell",
        start_date=period.bounds[0],
        end_date=period.bounds[1],
        spinup=spinup,
    )
    helper.create_hydro_units_from_csv_file(filename="hydro_units_elevation.csv")
    forcing = helper.get_forcing_data_from_csv_file(
        ref_elevation=1250, use_precip_gradient=True
    )
    obs = helper.get_obs_data_from_csv_file()
    socont, parameters = helper.get_model_and_params_socont()
    return helper, socont, parameters, forcing, obs


# ---------------------------------------------------------------------------
# 2. Calibrate on the calibration period only
# ---------------------------------------------------------------------------
helper, socont, parameters, forcing, obs = build(
    periods.calibration, spinup=periods.spinup
)

parameters.allow_changing = [
    "a_snow",
    "k_quick",
    "A",
    "k_slow_1",
    "percol",
    "k_slow_2",
    "rain_corr_factor",
    "snow_corr_factor",
]

# With periods=..., the whole calibration period is scored (the spin-up replaces
# the warmup) and the setup checks the model spans periods.calibration.
spot_setup = trainer.SpotpySetup(
    socont,
    parameters,
    forcing,
    obs,
    obj_func="kge_2012",
    periods=periods,
)
sampler = trainer.calibrate(spot_setup, "sceua", 2000, dbname=None, dbformat="ram")
best = trainer.get_best(sampler)
print(f"Calibration finished: best KGE = {best['score']:.3f}")

# ---------------------------------------------------------------------------
# 3. Validate: re-run the best parameters over the full span, score per period
# ---------------------------------------------------------------------------
helper_full, socont_full, parameters_full, forcing_full, obs_full = build(
    periods.simulation, spinup=periods.spinup
)
parameters_full.set_values(best["parameters"])
socont_full.run(parameters_full, forcing_full)

scores = hb.evaluate_periods(
    socont_full, obs_full, periods, metrics=("kge_2012", "nse")
)
print("\nSplit-sample scores:")
print(scores.round(3))

# ---------------------------------------------------------------------------
# 4. Plot the simulated vs observed discharge, with the periods shaded
# ---------------------------------------------------------------------------
time = socont_full.get_recorded_time()
sim = socont_full.get_outlet_discharge()

fig, ax = plt.subplots(figsize=(16, 6))
ax.plot(time, sim, color="black", lw=0.8, label="Simulated")
ax.plot(obs_full.time, obs_full.data[0], "r.", markersize=2, label="Observed")
ax.axvspan(
    periods.calibration.start,
    periods.calibration.end,
    color="tab:blue",
    alpha=0.08,
    label=f"Calibration (KGE {scores.loc['calibration', 'kge_2012']:.2f})",
)
ax.axvspan(
    periods.validation.start,
    periods.validation.end,
    color="tab:orange",
    alpha=0.08,
    label=f"Validation (KGE {scores.loc['validation', 'kge_2012']:.2f})",
)
ax.set_ylabel("Discharge [mm/d]")
ax.legend(loc="upper right")
plt.tight_layout()
plt.show()
