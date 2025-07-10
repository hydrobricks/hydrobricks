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
GLACIER_ICE_THICKNESS = TEST_FILES_DIR / 'ch_rhone_gletsch' / 'glaciers' / 'ice_thickness.tif'

# Create temporary directory
with tempfile.TemporaryDirectory() as tmp_dir_name:
    tmp_dir = tmp_dir_name

os.mkdir(tmp_dir)
working_dir = Path(tmp_dir)

# Prepare catchment data
catchment = hb.Catchment(
    CATCHMENT_OUTLINE,
    land_cover_types=['ground', 'glacier'],
    land_cover_names=['ground', 'glacier']
)
catchment.extract_dem(CATCHMENT_DEM)

# Create elevation bands
catchment.create_elevation_bands(
    method='equal_intervals',
    distance=100
)

# Glacier evolution
glacier_evolution = hb.preprocessing.GlacierEvolutionDeltaH()
glacier_df = glacier_evolution.compute_initial_ice_thickness(
    catchment,
    ice_thickness=GLACIER_ICE_THICKNESS
)

# It can then optionally be saved as a csv file
glacier_df.to_csv(
    working_dir / 'glacier_profile.csv',
    index=False
)

# The lookup table can be computed and saved as a csv file
glacier_evolution.compute_lookup_table()
glacier_evolution.save_as_csv(working_dir)

print(f"Files saved to: {working_dir}")
