import os.path
import tempfile
import uuid
from pathlib import Path

import hydrobricks as hb
from hydrobricks.actions import ActionLandCoverChange

# Paths
TEST_FILES_DIR = Path(
    os.path.dirname(os.path.realpath(__file__)),
    '..', '..', '..', 'tests', 'files', 'catchments'
)
CATCHMENT_OUTLINE = TEST_FILES_DIR / 'ch_rhone_gletsch' / 'outline.shp'
CATCHMENT_DEM = TEST_FILES_DIR / 'ch_rhone_gletsch' / 'dem.tif'
GLACIERS_DIR = TEST_FILES_DIR / 'ch_rhone_gletsch' / 'glaciers'

# Create temporary directory
working_dir = Path(tempfile.gettempdir()) / f"tmp_{uuid.uuid4().hex}"
working_dir.mkdir(parents=True, exist_ok=True)

# Prepare catchment data
catchment = hb.Catchment(
    CATCHMENT_OUTLINE,
    land_cover_types=['ground', 'glacier'],
    land_cover_names=['ground', 'glacier']
)
catchment.extract_dem(CATCHMENT_DEM)

# Create elevation bands
catchment.create_elevation_bands(method='equal_intervals', distance=50)

# List the files and dates
glaciers = [
    f'{GLACIERS_DIR}/sgi_1850.shp',
    f'{GLACIERS_DIR}/sgi_1931.shp',
    f'{GLACIERS_DIR}/sgi_1973.shp',
    f'{GLACIERS_DIR}/sgi_2010.shp',
    f'{GLACIERS_DIR}/sgi_2016.shp'
]
glaciers_debris = [
    None,
    None,
    None,
    None,
    f'{GLACIERS_DIR}/sgi_2016_debriscover.shp'
]
times = [
    '1850-01-01',
    '1931-01-01',
    '1973-01-01',
    '2010-01-01',
    '2016-01-01'
]

# Create the action land cover change object and the corresponding dataframe
changes, changes_df = ActionLandCoverChange.create_action_for_glaciers(
    catchment,
    times,
    glaciers,
    glaciers_debris,
    with_debris=False,
    method='raster',
    interpolate_yearly=True
)

# The 'changes' object can be directly used in hydrobricks:
# model.add_action(changes)

# The dataframe can be saved as csv (without column names)
changes_df[0].to_csv(
    working_dir / 'changes_glacier.csv',
    index=False
)
changes_df[1].to_csv(
    working_dir / 'changes_ground.csv',
    index=False
)

# And can be loaded again to be used in hydrobricks later
changes_glacier = ActionLandCoverChange()
changes_glacier.load_from_csv(
    working_dir / 'changes_glacier.csv',
    hydro_units=catchment.hydro_units,
    land_cover='glacier',
    area_unit='m2',
    match_with='id'
)

# Finally, initialize the HydroUnits cover with the first cover values of the
# ActionLandCoverChange object (no need to do it for the ground land cover type).
catchment.initialize_area_from_land_cover_change('glacier', changes_df[0])
