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
from pathlib import Path

import matplotlib.pyplot as plt

import hydrobricks as hb
import hydrobricks.trainer as trainer

logging.basicConfig(
    level=logging.INFO,
    stream=sys.stdout,
    force=True,
    format="%(levelname)s - %(name)s - %(message)s",
)

PROJECT_FILE = Path(__file__).parent / "sitter_periods_project.yaml"

# ---------------------------------------------------------------------------
# 1. The periods (calibration/validation/spin-up) are declared in the project
#    file, together with the whole setup.
# ---------------------------------------------------------------------------

# ---------------------------------------------------------------------------
# 2. Calibrate on the calibration period only
# ---------------------------------------------------------------------------
# With setup=False the model is built but not set up; setting it up over the
# calibration period applies the spin-up policy to that period.
project = hb.load_project(PROJECT_FILE, setup=False)
project.setup(period="calibration")
periods = project.periods

project.parameters.allow_changing = [
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
    project.model,
    project.parameters,
    project.forcing,
    project.observations,
    obj_func="kge_2012",
    periods=periods,
)
sampler = trainer.calibrate(spot_setup, "sceua", 2000, dbname=None, dbformat="ram")
best = trainer.get_best(sampler)
print(f"Calibration finished: best KGE = {best['score']:.3f}")

# ---------------------------------------------------------------------------
# 3. Validate: re-run the best parameters over the full span, score per period
# ---------------------------------------------------------------------------
# A fresh load (default setup) spans the full simulation period.
project_full = hb.load_project(PROJECT_FILE)
project_full.parameters.set_values(best["parameters"])
socont_full = project_full.model
obs_full = project_full.observations
socont_full.run(project_full.parameters, project_full.forcing)

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
