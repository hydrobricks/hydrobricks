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
CATCHMENT_OUTLINE = TEST_FILES_DIR / 'ch_rhone_gletsch' / 'outline.shp'
CATCHMENT_DEM = TEST_FILES_DIR / 'ch_rhone_gletsch' / 'dem.tif'

# Create temporary directory
working_dir = Path(tempfile.gettempdir()) / f"tmp_{uuid.uuid4().hex}"
working_dir.mkdir(parents=True, exist_ok=True)

# Prepare catchment data
catchment = hb.Catchment(CATCHMENT_OUTLINE)
catchment.extract_dem(CATCHMENT_DEM)

# Compute hydrological units or load them from a raster with
# catchment.load_unit_ids_from_raster(working_dir, 'unit_ids.tif')
catchment.discretize_by(['elevation', 'aspect'])
catchment.save_hydro_units_to_csv(working_dir / 'hydro_units.csv')
catchment.save_unit_ids_raster(working_dir)

# Compute connectivity between hydro units
connectivity = catchment.calculate_connectivity(mode='multiple')

# Save connectivity to a csv file
connectivity.to_csv(working_dir / 'connectivity.csv', index=False)
print("Results were saved in: ", working_dir)
