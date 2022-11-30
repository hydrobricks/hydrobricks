import os.path
import tempfile
from pathlib import Path

import hydrobricks as hb
from src.hydrobricks.preprocessing.catchment import Catchment


TEST_FILES_DIR = Path(
    os.path.dirname(os.path.realpath(__file__)),
    '..', '..', 'tests', 'files', 'catchments'
)
CATCHMENT_OUTLINE = TEST_FILES_DIR / 'ch_sitter_appenzell' / 'outline.shp'
CATCHMENT_DEM = TEST_FILES_DIR / 'ch_sitter_appenzell' / 'dem.tif'


def test_shapefile_parsing():
    if not hb.has_rasterio or not hb.has_geopandas or not hb.has_shapely:
        return
    Catchment(CATCHMENT_OUTLINE)


def test_shapefile_parsing():
    if not hb.has_rasterio or not hb.has_geopandas or not hb.has_shapely:
        return
    catchment = Catchment(CATCHMENT_OUTLINE)
    assert catchment.extract_dem(CATCHMENT_DEM)


