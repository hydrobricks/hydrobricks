import logging
import sys
from pathlib import Path

import matplotlib.pyplot as plt
import spotpy

import hydrobricks as hb
import hydrobricks.trainer as trainer

logging.basicConfig(
    level=logging.INFO,
    stream=sys.stdout,
    force=True,
    format="%(levelname)s - %(name)s - %(message)s",
)

# The whole setup (data, model, forcing spatialization with calibratable
# 'param:' corrections) is declared in the project file.
project = hb.load_project(Path(__file__).parent / "sitter_calibration_project.yaml")

# Select the parameters to optimize/analyze
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

# Set a specific prior distribution instead of the default (Uniform)
project.parameters.set_prior("a_snow", spotpy.parameter.Normal(mean=4, stddev=2))

# Setup SPOTPY
spot_setup = trainer.SpotpySetup(
    project.model,
    project.parameters,
    project.forcing,
    project.observations,
    warmup=365,
    obj_func=spotpy.objectivefunctions.nashsutcliffe,
)

# Select number of runs and run spotpy
nb_runs = 10000
sampler = spotpy.algorithms.mc(
    spot_setup, dbname="spotpy_socont_sitter_MC", dbformat="csv", save_sim=False
)
sampler.sample(nb_runs)

# Load the results
results = sampler.getdata()

# Plot parameter interaction
spotpy.analyser.plot_parameterInteraction(results)
plt.tight_layout()
plt.show()

# Plot posterior parameter distribution
posterior = spotpy.analyser.get_posterior(results, percentage=10)
spotpy.analyser.plot_parameterInteraction(posterior)
plt.tight_layout()
plt.show()
