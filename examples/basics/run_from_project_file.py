"""Run a model declared in a YAML project file.

The whole setup (model choice, input files, forcing spatialization, periods and
parameter values) lives in ``sitter_project.yaml``; :func:`hb.load_project`
validates it, builds the wired-up objects and returns them. Everything the
project file does not cover can still be done in Python on the returned
objects (``project.model``, ``project.forcing``, ...).
"""

import logging
import sys
from pathlib import Path

import hydrobricks as hb

logging.basicConfig(
    level=logging.INFO,
    stream=sys.stdout,
    force=True,
    format="%(levelname)s - %(name)s - %(message)s",
)

project = hb.load_project(Path(__file__).parent / "sitter_project.yaml")

# Run over the full simulation span and get the outlet discharge.
sim = project.run()
print(sim.describe())

# Split-sample evaluation on the declared periods.
scores = hb.evaluate_periods(
    project.model,
    project.observations,
    project.periods,
    metrics=("nse", "kge_2012"),
)
print(scores)
