import os.path
import tempfile
from pathlib import Path

import matplotlib.pyplot as plt
import numpy as np
import pandas as pd

import hydrobricks as hb

#from hydrobricks.preprocessing.glacier_evolution_delta_h import GlacierEvolutionDeltaH
from hydrobricks.constants import WATER_EQ as WE

# Paths
TEST_FILES_DIR = Path(
    os.path.dirname(os.path.realpath(__file__)),
    '..', '..', '..', 'tests', 'files', 'catchments'
)
CATCHMENT_OUTLINE = TEST_FILES_DIR / 'ch_rhone_gletsch' / 'outline.shp'
CATCHMENT_DEM = TEST_FILES_DIR / 'ch_rhone_gletsch' / 'dem.tif'
GLACIER_OUTLINE = TEST_FILES_DIR / 'ch_rhone_gletsch' / 'glaciers' / 'sgi_2016.shp'
GLACIER_ICE_THICKNESS = TEST_FILES_DIR / 'ch_rhone_gletsch' / 'glaciers' / 'IceThickness.tif'

GLACIER_DATA = TEST_FILES_DIR / 'ch_rhone_gletsch' / 'glacier_data.csv'
GLACIER_OUTPUT = TEST_FILES_DIR / 'ch_rhone_gletsch'

# Create temporary directory
with tempfile.TemporaryDirectory() as tmp_dir_name:
    tmp_dir = tmp_dir_name

os.mkdir(tmp_dir)
working_dir = Path(tmp_dir)

# Prepare catchment data
catchment = hb.Catchment(CATCHMENT_OUTLINE, land_cover_types=['ground', 'glacier'],
                         land_cover_names=['ground', 'glacier'])
catchment.extract_raster(CATCHMENT_DEM)

# Create elevation bands
# This discretization has to be compatible with the elevation bands used for the
# hydrological simulation later on.
catchment.create_elevation_bands(method='equal_intervals', distance=10)

# Create temporary directory
with tempfile.TemporaryDirectory() as tmp_dir_name:
    tmp_dir = tmp_dir_name

os.mkdir(tmp_dir)
working_dir = Path(tmp_dir)

# Glacier evolution
glacier_evolution = hb.preprocessing.GlacierEvolutionDeltaH(catchment.hydro_units)
# Compute the lookup table. In Seibert et al. (2018), the glacier width is not updated
# during the iterations.
glacier_evolution.compute_initial_glacier_data(catchment, GLACIER_OUTLINE, GLACIER_DATA,
                                               GLACIER_OUTPUT, ice_thickness=GLACIER_ICE_THICKNESS)
glacier_evolution.compute_lookup_table(GLACIER_DATA, update_width=False)
glacier_evolution.save_as_csv(working_dir)

print(f"Files saved to: {working_dir}")
