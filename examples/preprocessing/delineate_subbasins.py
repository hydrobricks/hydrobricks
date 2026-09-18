"""Delineate the subbasins of a river network from gauge locations.

The Sitter at Appenzell is nested in the Sitter at St. Gallen: one outlet at the
Appenzell gauge splits the St. Gallen catchment into two subbasins, whose reach
geometry (length, slope) is measured on the DEM. The resulting files (hydro units
with their subbasin, the subbasin table, the subbasin ids raster) are what a
project file declares under ``hydro_units`` and ``subbasins``.
"""

import logging
import os.path
import sys
import tempfile
import uuid
from pathlib import Path

import hydrobricks as hb

logging.basicConfig(
    level=logging.INFO,
    stream=sys.stdout,
    force=True,
    format="%(levelname)s - %(name)s - %(message)s",
)

# Paths
TEST_FILES_DIR = Path(
    os.path.dirname(os.path.realpath(__file__)),
    "..",
    "..",
    "tests",
    "files",
    "catchments",
)
CATCHMENT_OUTLINE = TEST_FILES_DIR / "ch_sitter_stgallen" / "outline.shp"
CATCHMENT_DEM = TEST_FILES_DIR / "ch_sitter_stgallen" / "dem.tif"

# The gauges inside the catchment, in the catchment CRS (EPSG:2056). The catchment
# outlet (St. Gallen) is added automatically for the area no gauge drains.
GAUGES = {"Appenzell": (2749050.0, 1244200.0)}

# Create temporary directory
working_dir = Path(tempfile.gettempdir()) / f"tmp_{uuid.uuid4().hex}"
working_dir.mkdir(parents=True, exist_ok=True)

# Prepare catchment data
catchment = hb.Catchment(CATCHMENT_OUTLINE)
catchment.extract_dem(CATCHMENT_DEM)

# Hydro units: elevation bands here; any discretization works. The units spanning
# several subbasins are split (one part per subbasin) so that every unit drains to
# a single outlet.
catchment.create_elevation_bands(method="equal_intervals", distance=100)

# Delineate the subbasins: the outlets are snapped to the streams derived from the
# DEM (cells draining more than 1 km2), their upstream areas carved into a tree,
# the reaches measured along the D8 flow paths.
subbasins = catchment.delineate_subbasins(GAUGES, split_units=True)
print(subbasins)

# Save what a project file needs: the hydro units (with their 'subbasin' column),
# the subbasin table and the rasters.
catchment.save_hydro_units_to_csv(working_dir / "hydro_units.csv")
subbasins.to_csv(working_dir / "subbasins.csv", index=False)
catchment.save_unit_ids_raster(working_dir)
catchment.save_subbasin_ids_raster(working_dir)
print("Results were saved in: ", working_dir)
