import os.path
import tempfile
from pathlib import Path

import numpy as np

import hydrobricks as hb

TEST_FILES_DIR = Path(
    os.path.dirname(os.path.realpath(__file__)),
    '..', '..', 'tests', 'files', 'catchments'
)
CATCHMENT_OUTLINE = TEST_FILES_DIR / 'ch_sitter_appenzell' / 'outline.shp'
CATCHMENT_DEM = TEST_FILES_DIR / 'ch_sitter_appenzell' / 'dem.tif'


def has_required_packages() -> bool:
    return hb.has_rasterio and hb.has_geopandas and hb.has_shapely


def test_shapefile_parsing():
    if not has_required_packages():
        return
    hb.Catchment(CATCHMENT_OUTLINE)


def test_dem_extraction():
    if not has_required_packages():
        return
    catchment = hb.Catchment(CATCHMENT_OUTLINE)
    assert catchment.extract_dem(CATCHMENT_DEM)


def test_elevation_bands_isohypses():
    if not has_required_packages():
        return
    catchment = hb.Catchment(CATCHMENT_OUTLINE)
    catchment.extract_dem(CATCHMENT_DEM)
    bands = catchment.create_elevation_bands(method='isohypse', distance=50)
    assert 74430000 < bands['area'].sum() < 74450000


def test_elevation_bands_quantiles():
    if not has_required_packages():
        return
    catchment = hb.Catchment(CATCHMENT_OUTLINE)
    catchment.extract_dem(CATCHMENT_DEM)
    bands = catchment.create_elevation_bands(method='quantiles', number=25)
    assert 74430000 < bands['area'].sum() < 74450000


def test_get_mean_elevation():
    if not has_required_packages():
        return
    catchment = hb.Catchment(CATCHMENT_OUTLINE)
    catchment.extract_dem(CATCHMENT_DEM)
    mean_elevation = catchment.get_mean_elevation()
    assert 1200 < mean_elevation < 1300


def test_save_unit_ids_raster():
    if not has_required_packages():
        return
    catchment = hb.Catchment(CATCHMENT_OUTLINE)
    catchment.extract_dem(CATCHMENT_DEM)
    catchment.create_elevation_bands(method='isohypse', distance=50)
    with tempfile.TemporaryDirectory() as tmp_dir:
        catchment.save_unit_ids_raster(Path(tmp_dir) / 'unit_ids.tif')
        assert (Path(tmp_dir) / 'unit_ids.tif').exists()


def test_load_units_from_raster():
    if not has_required_packages():
        return
    catchment = hb.Catchment(CATCHMENT_OUTLINE)
    catchment.extract_dem(CATCHMENT_DEM)
    catchment.create_elevation_bands(method='isohypse', distance=50)
    with tempfile.TemporaryDirectory() as tmp_dir:
        catchment.save_unit_ids_raster(Path(tmp_dir) / 'unit_ids.tif')

        catchment2 = hb.Catchment(CATCHMENT_OUTLINE)
        catchment2.extract_dem(CATCHMENT_DEM)
        catchment2.load_unit_ids_from_raster(Path(tmp_dir) / 'unit_ids.tif')
        assert np.allclose(catchment2.map_unit_ids, catchment.map_unit_ids)


def test_load_units_from_raster_prepare_attributes():
    catchment = hb.Catchment(CATCHMENT_OUTLINE)
    catchment.extract_dem(CATCHMENT_DEM)
    df1 = catchment.create_elevation_bands(method='isohypse', distance=50)

    with tempfile.TemporaryDirectory() as tmp_dir:
        catchment.save_unit_ids_raster(Path(tmp_dir) / 'unit_ids.tif')

        catchment2 = hb.Catchment(CATCHMENT_OUTLINE)
        catchment2.extract_dem(CATCHMENT_DEM)
        catchment2.load_unit_ids_from_raster(Path(tmp_dir) / 'unit_ids.tif')
        df2 = catchment2.get_hydro_units_attributes()

        assert np.allclose(df1['area'], df2['area'])
        assert np.allclose(df1['elevation_mean'], df2['elevation_mean'])
        assert np.allclose(df1['slope'], df2['slope'])
        assert np.allclose(df1['aspect'], df2['aspect'])


def test_discretize_by_elevation_and_aspect():
    catchment = hb.Catchment(CATCHMENT_OUTLINE)
    catchment.extract_dem(CATCHMENT_DEM)
    df = catchment.discretize_by(criteria=['elevation', 'aspect'],
                                 elevation_method='isohypse', elevation_distance=100)
    assert len(df) == 72  # 4 classes were empty
