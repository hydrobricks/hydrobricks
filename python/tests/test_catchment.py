import math
import os.path
import shutil
import tempfile
from pathlib import Path

import numpy as np
import pytest

import hydrobricks as hb

FILES_DIR = Path(
    os.path.dirname(os.path.realpath(__file__)),
    '..', '..', 'tests', 'files', 'catchments'
)
SITTER_OUTLINE = FILES_DIR / 'ch_sitter_appenzell' / 'outline.shp'
SITTER_DEM = FILES_DIR / 'ch_sitter_appenzell' / 'dem.tif'
RHONE_OUTLINE = FILES_DIR / 'ch_rhone_gletsch' / 'outline.shp'
RHONE_DEM = FILES_DIR / 'ch_rhone_gletsch' / 'dem.tif'
RHONE_UIDS = FILES_DIR / 'ch_rhone_gletsch' / 'unit_ids_radiation.tif'
RHONE_HUS = FILES_DIR / 'ch_rhone_gletsch' / 'hydro_units_elevation_radiation.csv'


def has_required_packages() -> bool:
    return hb.has_rasterio and hb.has_geopandas and hb.has_shapely


def test_shapefile_parsing():
    if not has_required_packages():
        return
    hb.Catchment(SITTER_OUTLINE)


def test_dem_extraction():
    if not has_required_packages():
        return
    catchment = hb.Catchment(SITTER_OUTLINE)
    assert catchment.extract_dem(SITTER_DEM)


def test_elevation_bands_equal_intervalss():
    if not has_required_packages():
        return
    catchment = hb.Catchment(SITTER_OUTLINE)
    catchment.extract_dem(SITTER_DEM)
    catchment.create_elevation_bands(
        method='equal_intervals',
        distance=50
    )
    area_sum = catchment.hydro_units.hydro_units['area'].sum()
    assert 74430000 < area_sum.iloc[0] < 74450000


def test_elevation_bands_quantiles():
    if not has_required_packages():
        return
    catchment = hb.Catchment(SITTER_OUTLINE)
    catchment.extract_dem(SITTER_DEM)
    catchment.create_elevation_bands(
        method='quantiles',
        number=25
    )
    area_sum = catchment.hydro_units.hydro_units['area'].sum()
    assert 74430000 < area_sum.iloc[0] < 74450000


def test_get_mean_elevation():
    if not has_required_packages():
        return
    catchment = hb.Catchment(SITTER_OUTLINE)
    catchment.extract_dem(SITTER_DEM)
    mean_elevation = catchment.get_mean_elevation()
    assert 1200 < mean_elevation < 1300


def test_save_unit_ids_raster():
    if not has_required_packages():
        return
    catchment = hb.Catchment(SITTER_OUTLINE)
    catchment.extract_dem(SITTER_DEM)
    catchment.create_elevation_bands(
        method='equal_intervals',
        distance=50
    )
    with tempfile.TemporaryDirectory() as tmp_dir:
        catchment.save_unit_ids_raster(Path(tmp_dir))
        assert (Path(tmp_dir) / 'unit_ids.tif').exists()


def test_load_units_from_raster():
    if not has_required_packages():
        return
    catchment = hb.Catchment(SITTER_OUTLINE)
    catchment.extract_dem(SITTER_DEM)
    catchment.create_elevation_bands(
        method='equal_intervals',
        distance=50
    )
    with tempfile.TemporaryDirectory() as tmp_dir:
        catchment.save_unit_ids_raster(Path(tmp_dir))

        catchment2 = hb.Catchment(SITTER_OUTLINE)
        catchment2.extract_dem(SITTER_DEM)
        catchment2.load_unit_ids_from_raster(Path(tmp_dir))
        assert np.allclose(catchment2.map_unit_ids, catchment.map_unit_ids)


def test_load_units_from_raster_prepare_attributes():
    catchment = hb.Catchment(SITTER_OUTLINE)
    catchment.extract_dem(SITTER_DEM)
    catchment.create_elevation_bands(
        method='equal_intervals',
        distance=50
    )
    df1 = catchment.hydro_units.hydro_units

    with tempfile.TemporaryDirectory() as tmp_dir:
        catchment.save_unit_ids_raster(Path(tmp_dir))

        catchment2 = hb.Catchment(SITTER_OUTLINE)
        catchment2.extract_dem(SITTER_DEM)
        catchment2.load_unit_ids_from_raster(Path(tmp_dir))
        df2 = catchment2.get_hydro_units_attributes().hydro_units

        assert np.allclose(df1['area'], df2['area'])
        assert np.allclose(df1['elevation_mean'], df2['elevation_mean'])
        assert np.allclose(df1['slope'], df2['slope'])
        assert np.allclose(df1['aspect'], df2['aspect'])


def test_discretize_by_elevation_and_aspect():
    catchment = hb.Catchment(SITTER_OUTLINE)
    catchment.extract_dem(SITTER_DEM)
    catchment.discretize_by(
        criteria=['elevation', 'aspect'],
        elevation_method='equal_intervals',
        elevation_distance=100
    )
    assert len(catchment.hydro_units.hydro_units) == 72  # 4 classes were empty


def test_solar_declination_jan():
    res = math.radians(-22.019)  # From https://www.suncalc.org/
    assert hb.Catchment.get_solar_declination_rad(10) == pytest.approx(res, abs=0.001)


def test_solar_declination_aug():
    doy = 218  # August 6th
    res = math.radians(16.523)  # From https://www.suncalc.org/
    assert hb.Catchment.get_solar_declination_rad(doy) == pytest.approx(res, abs=0.001)


def test_solar_zenith_jan():
    lat_rad = math.radians(47)
    solar_declination = hb.Catchment.get_solar_declination_rad(10)
    # Solar noon for location and date: 12:35:06 (https://gml.noaa.gov/grad/solcalc/)
    noon_dt = 35 / 60 + 6 / 3600
    hour_dt = -2  # 10h local time
    hour_angle = math.radians(15 * (hour_dt - noon_dt))
    zenith = hb.Catchment.get_solar_zenith(hour_angle, lat_rad, solar_declination)
    res = 90 - 12.69  # From https://www.suncalc.org/
    assert zenith == pytest.approx(res, abs=0.05)


def test_solar_zenith_aug():
    lat_rad = math.radians(47.31759)
    solar_declination = hb.Catchment.get_solar_declination_rad(218)
    # Solar noon for location and date: 13:28:22 (https://gml.noaa.gov/grad/solcalc/)
    noon_dt = 1 + 28 / 60 + 22 / 3600
    hour_dt = 7  # 19h local time
    hour_angle = math.radians(15 * (hour_dt - noon_dt))
    zenith = hb.Catchment.get_solar_zenith(hour_angle, lat_rad, solar_declination)
    res = 90 - 16.86  # From https://www.suncalc.org/
    assert zenith == pytest.approx(res, abs=0.08)


def test_solar_azimuth_jan():
    lat_rad = math.radians(47)
    solar_declin = hb.Catchment.get_solar_declination_rad(10)
    # Solar noon for location and date: 12:35:06 (https://gml.noaa.gov/grad/solcalc/)
    noon_dt = 35 / 60 + 6 / 3600
    hour_dt = -2  # 10h local time
    hour_angle = math.radians(15 * (hour_dt - noon_dt))
    azimuth = hb.Catchment.get_solar_azimuth_to_north(hour_angle, lat_rad, solar_declin)
    res = 143.45  # From https://www.suncalc.org/
    assert azimuth == pytest.approx(res, abs=0.04)


def test_solar_azimuth_aug():
    lat_rad = math.radians(47.31759)
    solar_declin = hb.Catchment.get_solar_declination_rad(218)
    # Solar noon for location and date: 13:28:22 (https://gml.noaa.gov/grad/solcalc/)
    noon_dt = 1 + 28 / 60 + 22 / 3600
    hour_dt = 7  # 19h local time
    hour_angle = math.radians(15 * (hour_dt - noon_dt))
    azimuth = hb.Catchment.get_solar_azimuth_to_north(hour_angle, lat_rad, solar_declin)
    res = 276.36  # From https://www.suncalc.org/
    assert azimuth == pytest.approx(res, abs=0.06)


def test_radiation_calculation():
    dem_path = FILES_DIR / 'others' / 'dem_small_tile.tif'
    ref_radiation_path = FILES_DIR / 'others' / 'radiation_annual_mean.tif'

    with tempfile.TemporaryDirectory() as tmp_dir:
        catchment = hb.Catchment()
        catchment.extract_dem(dem_path)

        catchment.calculate_daily_potential_radiation(
            str(Path(tmp_dir)),
            with_cast_shadows=False
        )

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


def test_radiation_calculation_with_cast_shadows():
    dem_path = FILES_DIR / 'others' / 'dem_small_tile.tif'
    ref_radiation_path = FILES_DIR / 'others' / 'radiation_annual_mean.tif'

    with tempfile.TemporaryDirectory() as tmp_dir:
        catchment = hb.Catchment()
        catchment.extract_dem(dem_path)

        catchment.calculate_daily_potential_radiation(
            str(Path(tmp_dir)),
            with_cast_shadows=True
        )

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

        assert abs(average_diff) < 0.1  # Different from the previous test


def test_radiation_calculation_resolution():
    dem_path = FILES_DIR / 'others' / 'dem_small_tile.tif'

    with tempfile.TemporaryDirectory() as tmp_dir_name:
        tmp_dir = tmp_dir_name
    os.mkdir(tmp_dir)
    working_dir = Path(tmp_dir)

    catchment = hb.Catchment()
    catchment.extract_dem(dem_path)

    catchment.calculate_daily_potential_radiation(
        str(working_dir),
        resolution=100,
        with_cast_shadows=False
    )

    assert (working_dir / 'annual_potential_radiation.tif').exists()
    assert (working_dir / 'daily_potential_radiation.nc').exists()

    try:
        shutil.rmtree(working_dir)
    except Exception:
        print("Failed to clean up.")


@pytest.mark.filterwarnings("ignore:`in1d` is deprecated:DeprecationWarning")
def test_single_connectivity_on_elevation_bands():
    catchment = hb.Catchment(SITTER_OUTLINE)
    catchment.extract_dem(SITTER_DEM)
    catchment.discretize_by(
        ['elevation'],
        elevation_distance=100
    )

    df = catchment.calculate_connectivity(
        mode='single',
        force_connectivity=False
    )

    assert df.loc[df[('id', '-')] == 1, ('connectivity', '-')][0] == {}

    for i in range(2, 19):
        keys = df.loc[df[('id', '-')] == i, ('connectivity', '-')].values[0].keys()
        key = list(keys)[0]
        assert key == i - 1


@pytest.mark.filterwarnings("ignore:`in1d` is deprecated:DeprecationWarning")
def test_connectivity_on_discontinuous_hydro_units():
    catchment = hb.Catchment(RHONE_OUTLINE)
    catchment.extract_dem(RHONE_DEM)
    catchment.load_hydro_units_from_csv(RHONE_HUS)
    catchment.load_unit_ids_from_raster(RHONE_UIDS)

    df = catchment.calculate_connectivity(
        mode='multiple',
        force_connectivity=True
    )

    assert df.empty is False
