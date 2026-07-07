"""Run a project and explore the results: scores, hydrograph, maps, animations.

The setup comes from ``sitter_project.yaml``; this script only adds the
analysis on top. It also shows the dict escape hatch: the project file is
loaded as a dict and tweaked (``record_all=True``, needed to map internal
states such as the snow water equivalent) before being passed to
``hb.load_project``.
"""

import logging
import sys
from pathlib import Path

import yaml

import hydrobricks as hb

logging.basicConfig(
    level=logging.INFO,
    stream=sys.stdout,
    force=True,
    format="%(levelname)s - %(name)s - %(message)s",
)

# Paths
EXAMPLE_DIR = Path(__file__).parent
CATCHMENT_DIR = (
    EXAMPLE_DIR / ".." / ".." / "tests" / "files" / "catchments" / "ch_sitter_appenzell"
)
CATCHMENT_RASTER = CATCHMENT_DIR / "unit_ids.tif"
DEM_RASTER = CATCHMENT_DIR / "dem.tif"

# Load the project file as a dict and record everything (slower, bigger output,
# but needed to plot the internal states below).
with open(EXAMPLE_DIR / "sitter_project.yaml", encoding="utf-8") as f:
    config = yaml.safe_load(f)
config["model"]["options"]["record_all"] = True

project = hb.load_project(config, base_dir=EXAMPLE_DIR)

# Run and get the simulated outlet discharge
sim_ts = project.run()
socont = project.model
obs = project.observations

# Evaluate
obs_ts = obs.data[0]
nse = socont.eval("nse", obs_ts)
kge_2012 = socont.eval("kge_2012", obs_ts)
print(f"NSE = {nse:.3f}, KGE = {kge_2012:.3f}")

# Compute benchmark metrics (prediction by the mean/climatology of the obs)
ref_nse = obs.compute_reference_metric("nse")
ref_kge = obs.compute_reference_metric("kge_2012")
print(f"Benchmark: NSE = {ref_nse:.3f}, KGE = {ref_kge:.3f}")

# Plot one year of the hydrograph
hb.Plotter.plot_hydrograph(obs_ts, sim_ts.to_numpy(), obs.time, year=2020)

# Dump all outputs and reload them as a Results object
socont.dump_outputs(str(project.output_dir))
results = hb.Results(str(project.output_dir / "results.nc"))

# List the hydro units components available
results.list_hydro_units_components()

# Plot the snow water equivalent on a map
hb.Plotter.plot_map_hydro_unit_value(
    results,
    CATCHMENT_RASTER,
    "open_snowpack:snow_content",
    "1981-01-20",
    dem_path=DEM_RASTER,
    max_val=300,
)

# Create an animated map of the snow water equivalent
hb.Plotter.create_animated_map_hydro_unit_value(
    results,
    CATCHMENT_RASTER,
    "open_snowpack:snow_content",
    "1990-01-01",
    "1990-03-20",
    save_path=str(project.output_dir),
    dem_path=DEM_RASTER,
    max_val=300,
)

# Create an animated map of the slow reservoir 2 water content
hb.Plotter.create_animated_map_hydro_unit_value(
    results,
    CATCHMENT_RASTER,
    "slow_reservoir_2:water_content",
    "2020-01-20",
    "2020-03-20",
    save_path=str(project.output_dir),
    dem_path=DEM_RASTER,
    max_val=10,
)
