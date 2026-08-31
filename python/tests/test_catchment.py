import logging
import math
import os.path
import shutil
import tempfile
import uuid
from pathlib import Path

import numpy as np
import pytest

import hydrobricks as hb
from hydrobricks._exceptions import ConfigurationError
from hydrobricks.preprocessing.catchment_discretization import (
    CatchmentDiscretization,
)

logger = logging.getLogger(__name__)

FILES_DIR = Path(
    os.path.dirname(os.path.realpath(__file__)),
    "..",
    "..",
    "tests",
    "files",
    "catchments",
)
SITTER_OUTLINE = FILES_DIR / "ch_sitter_appenzell" / "outline.shp"
SITTER_DEM = FILES_DIR / "ch_sitter_appenzell" / "dem.tif"
RHONE_OUTLINE = FILES_DIR / "ch_rhone_gletsch" / "outline.shp"
RHONE_DEM = FILES_DIR / "ch_rhone_gletsch" / "dem.tif"
RHONE_UIDS = FILES_DIR / "ch_rhone_gletsch" / "unit_ids_radiation.tif"
RHONE_HUS = FILES_DIR / "ch_rhone_gletsch" / "hydro_units_elevation_radiation.csv"


def has_required_packages() -> bool:
    return hb.HAS_RASTERIO and hb.HAS_GEOPANDAS and hb.HAS_SHAPELY


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
    catchment.create_elevation_bands(method="equal_intervals", distance=50)
    area_sum = catchment.hydro_units.hydro_units["area"].sum()
    assert 74430000 < area_sum.iloc[0] < 74450000


def test_elevation_bands_quantiles():
    if not has_required_packages():
        return
    catchment = hb.Catchment(SITTER_OUTLINE)
    catchment.extract_dem(SITTER_DEM)
    catchment.create_elevation_bands(method="quantiles", number=25)
    area_sum = catchment.hydro_units.hydro_units["area"].sum()
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
    catchment.create_elevation_bands(method="equal_intervals", distance=50)
    with tempfile.TemporaryDirectory() as tmp_dir:
        catchment.save_unit_ids_raster(Path(tmp_dir))
        assert (Path(tmp_dir) / "unit_ids.tif").exists()


def test_load_units_from_raster():
    if not has_required_packages():
        return
    catchment = hb.Catchment(SITTER_OUTLINE)
    catchment.extract_dem(SITTER_DEM)
    catchment.create_elevation_bands(method="equal_intervals", distance=50)
    with tempfile.TemporaryDirectory() as tmp_dir:
        catchment.save_unit_ids_raster(Path(tmp_dir))

        catchment2 = hb.Catchment(SITTER_OUTLINE)
        catchment2.extract_dem(SITTER_DEM)
        catchment2.load_unit_ids_from_raster(Path(tmp_dir))
        assert np.allclose(catchment2.map_unit_ids, catchment.map_unit_ids)


def test_load_units_from_raster_prepare_attributes():
    catchment = hb.Catchment(SITTER_OUTLINE)
    catchment.extract_dem(SITTER_DEM)
    catchment.create_elevation_bands(method="equal_intervals", distance=50)
    df1 = catchment.hydro_units.hydro_units

    with tempfile.TemporaryDirectory() as tmp_dir:
        catchment.save_unit_ids_raster(Path(tmp_dir))

        catchment2 = hb.Catchment(SITTER_OUTLINE)
        catchment2.extract_dem(SITTER_DEM)
        catchment2.load_unit_ids_from_raster(Path(tmp_dir))
        df2 = catchment2.get_hydro_units_attributes().hydro_units

        assert np.allclose(df1["area"], df2["area"])
        assert np.allclose(df1["elevation_mean"], df2["elevation_mean"])
        assert np.allclose(df1["slope"], df2["slope"])
        assert np.allclose(df1["aspect"], df2["aspect"])


def test_discretize_by_elevation_and_aspect():
    catchment = hb.Catchment(SITTER_OUTLINE)
    catchment.extract_dem(SITTER_DEM)
    catchment.discretize_by(
        criteria=["elevation", "aspect"],
        elevation_method="equal_intervals",
        elevation_distance=100,
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
    dem_path = FILES_DIR / ".." / "others" / "dem_small_tile.tif"
    ref_radiation_path = FILES_DIR / ".." / "others" / "radiation_annual_mean.tif"

    with tempfile.TemporaryDirectory() as tmp_dir:
        catchment = hb.Catchment()
        catchment.extract_dem(dem_path)

        catchment.calculate_daily_potential_radiation(
            str(Path(tmp_dir)), with_cast_shadows=False
        )

        assert (Path(tmp_dir) / "annual_potential_radiation.tif").exists()
        assert (Path(tmp_dir) / "daily_potential_radiation.nc").exists()

        ref_radiation = hb.rasterio.open(ref_radiation_path).read(1)
        calc_radiation = hb.rasterio.open(
            Path(tmp_dir) / "annual_potential_radiation.tif"
        ).read(1)

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
    dem_path = FILES_DIR / ".." / "others" / "dem_small_tile.tif"
    ref_radiation_path = FILES_DIR / ".." / "others" / "radiation_annual_mean.tif"

    with tempfile.TemporaryDirectory() as tmp_dir:
        catchment = hb.Catchment()
        catchment.extract_dem(dem_path)

        catchment.calculate_daily_potential_radiation(
            str(Path(tmp_dir)), with_cast_shadows=True
        )

        assert (Path(tmp_dir) / "annual_potential_radiation.tif").exists()
        assert (Path(tmp_dir) / "daily_potential_radiation.nc").exists()

        ref_radiation = hb.rasterio.open(ref_radiation_path).read(1)
        calc_radiation = hb.rasterio.open(
            Path(tmp_dir) / "annual_potential_radiation.tif"
        ).read(1)

        # Shift the calculated radiation to match the reference (likely due to the
        # slope and aspect calculations)
        calc_radiation = np.roll(calc_radiation, 1, axis=0)
        calc_radiation = np.roll(calc_radiation, -1, axis=1)

        # Crop 2 pixels around the edges for both arrays
        ref_radiation = ref_radiation[2:-2, 2:-2]
        calc_radiation = calc_radiation[2:-2, 2:-2]

        diff = ref_radiation - calc_radiation
        average_diff = np.mean(diff)

        # The reference raster predates the correction of the Earth-Sun distance
        # ratio (true anomaly) in _calculate_radiation_hock_equation, which shifts
        # the annual mean by ~0.19 W/m². Tolerance relaxed accordingly.
        assert abs(average_diff) < 0.25  # Different from the previous test


def test_radiation_calculation_resolution():
    dem_path = FILES_DIR / ".." / "others" / "dem_small_tile.tif"

    working_dir = Path(tempfile.gettempdir()) / f"tmp_{uuid.uuid4().hex}"
    working_dir.mkdir(parents=True, exist_ok=True)

    catchment = hb.Catchment()
    catchment.extract_dem(dem_path)

    catchment.calculate_daily_potential_radiation(
        str(working_dir), resolution=100, with_cast_shadows=False
    )

    assert (working_dir / "annual_potential_radiation.tif").exists()
    assert (working_dir / "daily_potential_radiation.nc").exists()

    try:
        shutil.rmtree(working_dir)
    except Exception:
        logger.debug("Failed to clean up temporary directory %s", working_dir)


def test_radiation_cache_roundtrip(tmp_path, monkeypatch):
    dem_path = FILES_DIR / ".." / "others" / "dem_small_tile.tif"
    cache_dir = tmp_path / "cache"
    out1 = tmp_path / "out1"
    out2 = tmp_path / "out2"
    out1.mkdir()
    out2.mkdir()

    catchment = hb.Catchment()
    catchment.extract_dem(dem_path)
    catchment.calculate_daily_potential_radiation(
        str(out1), resolution=100, with_cast_shadows=False, cache_dir=cache_dir
    )
    assert len(list(cache_dir.glob("potential_radiation_*"))) == 1

    # The second run must be served from the cache without any computation.
    catchment2 = hb.Catchment()
    catchment2.extract_dem(dem_path)

    def _fail(*args, **kwargs):
        raise AssertionError("cache miss: the radiation was recomputed")

    monkeypatch.setattr(
        catchment2.topography, "resample_dem_and_calculate_slope_aspect", _fail
    )
    catchment2.calculate_daily_potential_radiation(
        str(out2), resolution=100, with_cast_shadows=False, cache_dir=cache_dir
    )

    assert (out2 / "annual_potential_radiation.tif").exists()
    assert (out2 / "daily_potential_radiation.nc").exists()
    assert catchment2.solar_radiation.mean_annual_radiation is not None

    rad1 = hb.rasterio.open(out1 / "annual_potential_radiation.tif").read(1)
    rad2 = hb.rasterio.open(out2 / "annual_potential_radiation.tif").read(1)
    assert np.allclose(rad1, rad2, equal_nan=True)

    # A different option is a different key.
    catchment.calculate_daily_potential_radiation(
        str(out1), resolution=100, with_cast_shadows=True, cache_dir=cache_dir
    )
    assert len(list(cache_dir.glob("potential_radiation_*"))) == 2


@pytest.mark.filterwarnings("ignore:`in1d` is deprecated:DeprecationWarning")
def test_single_connectivity_on_elevation_bands():
    catchment = hb.Catchment(SITTER_OUTLINE)
    catchment.extract_dem(SITTER_DEM)
    catchment.discretize_by(["elevation"], elevation_distance=100)

    df = catchment.calculate_connectivity(mode="single", force_connectivity=False)

    assert df.loc[df[("id", "-")] == 1, ("connectivity", "-")][0] == {}

    for i in range(2, 19):
        keys = df.loc[df[("id", "-")] == i, ("connectivity", "-")].values[0].keys()
        key = list(keys)[0]
        assert key == i - 1


@pytest.mark.filterwarnings("ignore:`in1d` is deprecated:DeprecationWarning")
def test_connectivity_on_discontinuous_hydro_units():
    catchment = hb.Catchment(RHONE_OUTLINE)
    catchment.extract_dem(RHONE_DEM)
    catchment.load_hydro_units_from_csv(RHONE_HUS)
    catchment.load_unit_ids_from_raster(RHONE_UIDS)

    df = catchment.calculate_connectivity(mode="multiple", force_connectivity=True)

    assert df.empty is False


def _discretize_sitter(**kwargs) -> "hb.Catchment":
    """Discretize the Sitter catchment by elevation and aspect (72 units)."""
    catchment = hb.Catchment(SITTER_OUTLINE)
    catchment.extract_dem(SITTER_DEM)
    catchment.discretize_by(
        criteria=["elevation", "aspect"],
        elevation_method="equal_intervals",
        elevation_distance=100,
        **kwargs,
    )
    return catchment


def _count_patches(map_unit_ids, unit_id, connectivity=8) -> int:
    """Number of spatially connected patches of a given hydro unit."""
    from scipy import ndimage

    structure = ndimage.generate_binary_structure(2, 2 if connectivity == 8 else 1)
    _, n_patches = ndimage.label(map_unit_ids == unit_id, structure=structure)
    return n_patches


def _patches_per_unit(catchment, connectivity=8) -> list[int]:
    ids = catchment.hydro_units.hydro_units["id"].values[:, 0]
    return [_count_patches(catchment.map_unit_ids, i, connectivity) for i in ids]


def test_discretization_units_are_discontinuous_by_default():
    if not has_required_packages() or not hb.HAS_SCIPY:
        return
    catchment = _discretize_sitter()

    # Hydro units are defined by criteria combinations, so they can span several
    # spatially distant patches. This is the default behaviour and must be kept.
    assert max(_patches_per_unit(catchment)) > 1


def test_split_discontinuous_units():
    if not has_required_packages() or not hb.HAS_SCIPY:
        return
    reference = _discretize_sitter()
    catchment = _discretize_sitter(split_discontinuous=True)

    units = catchment.hydro_units.hydro_units
    ids = units["id"].values[:, 0]

    # Every unit is now a single connected patch.
    assert max(_patches_per_unit(catchment)) == 1

    # The splitting refines the discretization and keeps the ids dense.
    assert len(units) > len(reference.hydro_units.hydro_units)
    assert np.array_equal(np.sort(ids), np.arange(1, len(units) + 1))
    assert catchment.map_unit_ids.dtype == np.uint16

    # No area is lost nor duplicated.
    assert catchment.hydro_units.hydro_units["area"].sum().iloc[0] == pytest.approx(
        reference.hydro_units.hydro_units["area"].sum().iloc[0]
    )


def test_split_discontinuous_preserves_criteria_columns():
    if not has_required_packages() or not hb.HAS_SCIPY:
        return
    catchment = _discretize_sitter(split_discontinuous=True)
    units = catchment.hydro_units.hydro_units
    aspect = catchment.topography.aspect

    for _, row in units.iterrows():
        unit_id = int(row["id"].values[0])
        mask = catchment.map_unit_ids == unit_id
        elevations = catchment.dem_data[mask]

        # The criteria values must follow the units through the split.
        assert elevations.min() >= row["elevation_min"].values[0]
        assert elevations.max() < row["elevation_max"].values[0]
        assert row["elevation_min"].values[0] <= row["elevation_mean"].values[0]
        assert row["elevation_mean"].values[0] <= row["elevation_max"].values[0]

        aspects = aspect[mask]
        aspect_class = row["aspect_class"].values[0]
        if aspect_class == "N":
            assert np.all((aspects >= 315) | (aspects < 45))
        elif aspect_class == "E":
            assert np.all((aspects >= 45) & (aspects < 135))
        elif aspect_class == "S":
            assert np.all((aspects >= 135) & (aspects < 225))
        else:
            assert np.all((aspects >= 225) & (aspects < 315))


def test_split_discontinuous_min_patch_area():
    if not has_required_packages() or not hb.HAS_SCIPY:
        return
    reference = _discretize_sitter()
    all_patches = _discretize_sitter(split_discontinuous=True)
    catchment = _discretize_sitter(split_discontinuous=True, min_patch_area=200000)

    n_ref = len(reference.hydro_units.hydro_units)
    n_all = len(all_patches.hydro_units.hydro_units)
    n_units = len(catchment.hydro_units.hydro_units)

    # The threshold keeps the small patches attached to the largest one.
    assert n_ref <= n_units < n_all

    # The small patches are merged back, not dropped: the area is preserved.
    assert catchment.hydro_units.hydro_units["area"].sum().iloc[0] == pytest.approx(
        reference.hydro_units.hydro_units["area"].sum().iloc[0]
    )


def test_split_discontinuous_connectivity_4_vs_8():
    if not has_required_packages() or not hb.HAS_SCIPY:
        return
    units_8 = _discretize_sitter(split_discontinuous=True, connectivity=8)
    units_4 = _discretize_sitter(split_discontinuous=True, connectivity=4)

    # Diagonal neighbours are connected with 8-connectivity, so it splits less.
    assert len(units_4.hydro_units.hydro_units) >= len(units_8.hydro_units.hydro_units)


def test_split_discontinuous_invalid_connectivity():
    if not has_required_packages():
        return
    with pytest.raises(ConfigurationError):
        _discretize_sitter(split_discontinuous=True, connectivity=6)


def test_split_discontinuous_invalid_min_patch_area():
    if not has_required_packages():
        return
    with pytest.raises(ConfigurationError):
        _discretize_sitter(split_discontinuous=True, min_patch_area=0)


class _DummyCatchment:
    """Minimal catchment stub for the unit-splitting logic (100 m2 pixels)."""

    @staticmethod
    def get_dem_pixel_area() -> float:
        return 100.0


def _splitter() -> CatchmentDiscretization:
    splitter = CatchmentDiscretization.__new__(CatchmentDiscretization)
    splitter.catchment = _DummyCatchment()
    return splitter


# Unit 1 is made of a 4-cell patch and an isolated 1-cell patch, unit 2 of a
# single patch.
DISCONTINUOUS_MAP = np.array(
    [
        [1, 1, 0, 1],
        [1, 1, 0, 0],
        [0, 0, 0, 0],
        [2, 2, 0, 0],
    ],
    dtype=float,
)


def test_split_units_relabels_each_patch():
    if not hb.HAS_SCIPY:
        return
    new_map, origins = _splitter()._split_discontinuous_units(DISCONTINUOUS_MAP, 2)

    # The largest patch of a unit keeps the first of the new ids.
    assert np.array_equal(
        new_map,
        [[1, 1, 0, 2], [1, 1, 0, 0], [0, 0, 0, 0], [3, 3, 0, 0]],
    )
    assert origins == [0, 0, 1]


def test_split_units_merges_patches_below_the_threshold():
    if not hb.HAS_SCIPY:
        return
    # 300 m2 = 3 cells: the isolated cell of unit 1 goes back to its largest patch.
    new_map, origins = _splitter()._split_discontinuous_units(
        DISCONTINUOUS_MAP, 2, min_patch_area=300
    )
    assert np.array_equal(new_map, DISCONTINUOUS_MAP)
    assert origins == [0, 1]


def test_split_units_keeps_the_unit_whole_if_no_patch_qualifies():
    if not hb.HAS_SCIPY:
        return
    new_map, origins = _splitter()._split_discontinuous_units(
        DISCONTINUOUS_MAP, 2, min_patch_area=1e9
    )
    assert np.array_equal(new_map, DISCONTINUOUS_MAP)
    assert origins == [0, 1]


def test_split_units_connectivity():
    if not hb.HAS_SCIPY:
        return
    diagonal = np.array([[1, 0, 0], [0, 1, 0], [0, 0, 1]], dtype=float)
    splitter = _splitter()

    # Diagonal cells are a single patch with 8-connectivity, three with 4.
    assert splitter._split_discontinuous_units(diagonal, 1, connectivity=8)[1] == [0]
    assert splitter._split_discontinuous_units(diagonal, 1, connectivity=4)[1] == [
        0,
        0,
        0,
    ]
