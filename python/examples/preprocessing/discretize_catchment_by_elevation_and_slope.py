import os.path
import tempfile
import uuid
from pathlib import Path

import hydrobricks as hb

# Paths
TEST_FILES_DIR = Path(
    os.path.dirname(os.path.realpath(__file__)),
    '..', '..', '..', 'tests', 'files', 'catchments'
)
CATCHMENT_OUTLINE = TEST_FILES_DIR / 'ch_sitter_appenzell' / 'outline.shp'
CATCHMENT_DEM = TEST_FILES_DIR / 'ch_sitter_appenzell' / 'dem.tif'

# Create temporary directory
working_dir = Path(tempfile.gettempdir()) / f"tmp_{uuid.uuid4().hex}"
working_dir.mkdir(parents=True, exist_ok=True)

# Prepare catchment data
catchment = hb.Catchment(CATCHMENT_OUTLINE)
catchment.extract_dem(CATCHMENT_DEM)

# Compute the slope and aspect
catchment.calculate_slope_aspect()

# Discretize the catchment by elevation and aspect
catchment.discretize_by(
    criteria=['elevation', 'slope'],
    elevation_method='equal_intervals',
    elevation_distance=200,
    slope_method='equal_intervals',
    slope_distance=15
)

# Save elevation bands to a raster
catchment.save_unit_ids_raster(working_dir)

# Save the elevation band properties to a csv file
catchment.save_hydro_units_to_csv(working_dir / 'bands.csv')

print(f"Files saved to: {working_dir}")
