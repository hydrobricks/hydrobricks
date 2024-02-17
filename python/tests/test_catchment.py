import os.path
import shutil
import tempfile
from pathlib import Path

import numpy as np

import hydrobricks as hb

FILES_DIR = Path(
    os.path.dirname(os.path.realpath(__file__)),
    '..', '..', 'tests', 'files'
)
CATCHMENT_OUTLINE = FILES_DIR / 'catchments' / 'ch_sitter_appenzell' / 'outline.shp'
CATCHMENT_DEM = FILES_DIR / 'catchments' / 'ch_sitter_appenzell' / 'dem.tif'


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
    catchment.create_elevation_bands(method='isohypse', distance=50)
    area_sum = catchment.hydro_units.hydro_units['area'].sum()
    assert 74430000 < area_sum.iloc[0] < 74450000


def test_elevation_bands_quantiles():
    if not has_required_packages():
        return
    catchment = hb.Catchment(CATCHMENT_OUTLINE)
    catchment.extract_dem(CATCHMENT_DEM)
    catchment.create_elevation_bands(method='quantiles', number=25)
    area_sum = catchment.hydro_units.hydro_units['area'].sum()
    assert 74430000 < area_sum.iloc[0] < 74450000


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
        catchment.save_unit_ids_raster(Path(tmp_dir))

        catchment2 = hb.Catchment(CATCHMENT_OUTLINE)
        catchment2.extract_dem(CATCHMENT_DEM)
        catchment2.load_unit_ids_from_raster(Path(tmp_dir))
        assert np.allclose(catchment2.map_unit_ids, catchment.map_unit_ids)


def test_load_units_from_raster_prepare_attributes():
    catchment = hb.Catchment(CATCHMENT_OUTLINE)
    catchment.extract_dem(CATCHMENT_DEM)
    catchment.create_elevation_bands(method='isohypse', distance=50)
    df1 = catchment.hydro_units.hydro_units

    with tempfile.TemporaryDirectory() as tmp_dir:
        catchment.save_unit_ids_raster(Path(tmp_dir))

        catchment2 = hb.Catchment(CATCHMENT_OUTLINE)
        catchment2.extract_dem(CATCHMENT_DEM)
        catchment2.load_unit_ids_from_raster(Path(tmp_dir))
        df2 = catchment2.get_hydro_units_attributes().hydro_units

        assert np.allclose(df1['area'], df2['area'])
        assert np.allclose(df1['elevation_mean'], df2['elevation_mean'])
        assert np.allclose(df1['slope'], df2['slope'])
        assert np.allclose(df1['aspect'], df2['aspect'])


def test_discretize_by_elevation_and_aspect():
    catchment = hb.Catchment(CATCHMENT_OUTLINE)
    catchment.extract_dem(CATCHMENT_DEM)
    catchment.discretize_by(criteria=['elevation', 'aspect'],
                            elevation_method='isohypse', elevation_distance=100)
    assert len(catchment.hydro_units.hydro_units) == 72  # 4 classes were empty


def test_radiation_calculation():
    dem_path = FILES_DIR / 'others' / 'dem_small_tile.tif'
    ref_radiation_path = FILES_DIR / 'others' / 'radiation_annual_mean.tif'

    with tempfile.TemporaryDirectory() as tmp_dir:
        catchment = hb.Catchment()
        catchment.extract_dem(dem_path)

        catchment.calculate_daily_potential_radiation(str(Path(tmp_dir)))

        assert (Path(tmp_dir) / 'annual_potential_radiation.tif').exists()
        assert (Path(tmp_dir) / 'daily_potential_radiation.nc').exists()

        ref_radiation = hb.rasterio.open(ref_radiation_path).read(1)
        calc_radiation = hb.rasterio.open(
            Path(tmp_dir) / 'annual_potential_radiation.tif').read(1)

        # Shift the calculated radiation to match the reference (likely due to the
        # slope and aspect calculations)
        calc_radiation = np.roll(calc_radiation, 1, axis=0)
        calc_radiation = np.roll(calc_radiation, -1, axis=1)

        # Crop 2 pixels around the edges for both arrays
        ref_radiation = ref_radiation[2:-2, 2:-2]
        calc_radiation = calc_radiation[2:-2, 2:-2]

        diff = ref_radiation - calc_radiation
        average_diff = np.mean(diff)

        assert abs(average_diff) < 4


def test_radiation_calculation_resolution():
    dem_path = FILES_DIR / 'others' / 'dem_small_tile.tif'

    with tempfile.TemporaryDirectory() as tmp_dir_name:
        tmp_dir = tmp_dir_name
    os.mkdir(tmp_dir)
    working_dir = Path(tmp_dir)

    catchment = hb.Catchment()
    catchment.extract_dem(dem_path)

    catchment.calculate_daily_potential_radiation(str(working_dir),
                                                  resolution=100)

    assert (working_dir / 'annual_potential_radiation.tif').exists()
    assert (working_dir / 'daily_potential_radiation.nc').exists()

    try:
        shutil.rmtree(working_dir)
    except Exception:
        print("Failed to clean up.")
