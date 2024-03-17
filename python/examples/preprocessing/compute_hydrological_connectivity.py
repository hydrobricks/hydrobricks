import os.path
import tempfile
from pathlib import Path

import hydrobricks as hb

# Paths
TEST_FILES_DIR = Path(
    os.path.dirname(os.path.realpath(__file__)),
    '..', '..', '..', 'tests', 'files', 'catchments'
)
CATCHMENT_OUTLINE = TEST_FILES_DIR / 'ch_rhone_gletsch' / 'outline.shp'
CATCHMENT_DEM = TEST_FILES_DIR / 'ch_rhone_gletsch' / 'dem.tif'
CATCHMENT_METEO = TEST_FILES_DIR / 'ch_rhone_gletsch' / 'meteo.csv'

# Create temporary directory
with tempfile.TemporaryDirectory() as tmp_dir_name:
    tmp_dir = tmp_dir_name

os.mkdir(tmp_dir)
working_dir = Path(tmp_dir)

# Prepare catchment data
catchment = hb.Catchment(CATCHMENT_OUTLINE)
catchment.extract_dem(CATCHMENT_DEM)

# Compute hydrological units or load them from a raster with
# catchment.load_unit_ids_from_raster(working_dir, 'unit_ids.tif')
catchment.discretize_by(['elevation', 'aspect'])
catchment.save_hydro_units_to_csv(working_dir / 'hydro_units.csv')
catchment.save_unit_ids_raster(working_dir)

# Compute connectivity between hydro units
connectivity = catchment.calculate_connectivity()
