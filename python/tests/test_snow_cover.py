import os.path
import tempfile
import uuid
from pathlib import Path

import numpy as np
import pandas as pd
import pytest

import hydrobricks as hb
import hydrobricks.models as models
import hydrobricks.trainer as trainer
from hydrobricks._exceptions import ConfigurationError
from hydrobricks.evaluation import SnowCoverObservations
from hydrobricks.evaluation.snow_cover import (
    _cache_key,
    _date_from_name,
    _parse_struct_metadata,
    _swe_to_fraction,
)

TEST_FILES_DIR = Path(
    os.path.dirname(os.path.realpath(__file__)),
    "..",
    "..",
    "tests",
    "files",
    "catchments",
)
GLETSCH_DIR = TEST_FILES_DIR / "ch_rhone_gletsch"
GLACIER_DIR = GLETSCH_DIR / "glaciers"

START_DATE = "2014-01-01"
END_DATE = "2018-12-31"


# --------------------------------------------------------------------------- #
# Depletion curve
# --------------------------------------------------------------------------- #
def test_swe_to_fraction_curve():
    out = _swe_to_fraction(np.array([0.0, 25.0, 50.0, 100.0, np.nan, -5.0]), 50.0)
    assert out.tolist() == [0.0, 0.5, 1.0, 1.0, 0.0, 0.0]


def test_invalid_swe_full_raises():
    with pytest.raises(ConfigurationError):
        SnowCoverObservations(swe_full=0)
    with pytest.raises(ConfigurationError):
        SnowCoverObservations(swe_full=-10)


# --------------------------------------------------------------------------- #
# CSV loader (no model needed)
# --------------------------------------------------------------------------- #
def test_from_csv_long_format(tmp_path):
    csv = tmp_path / "sc.csv"
    csv.write_text(
        "date,unit,scf\n"
        "2016-01-15,1,80\n"
        "2016-01-15,2,20\n"
        "2016-02-15,1,\n"  # missing value -> dropped
    )
    obs = SnowCoverObservations.from_csv(
        csv, date_col="date", unit_col="unit", value_col="scf", value_scale=0.01
    )
    assert len(obs) == 2
    assert obs.targets[0]["unit_id"] == 1
    assert obs.targets[0]["t"] == pd.Timestamp("2016-01-15")
    assert obs.values.tolist() == [0.8, 0.2]


def test_from_csv_restrict_to_period(tmp_path):
    csv = tmp_path / "sc.csv"
    csv.write_text(
        "date,unit,scf\n" "2014-01-15,1,0.5\n" "2016-01-15,1,0.6\n" "2020-01-15,1,0.7\n"
    )
    obs = SnowCoverObservations.from_csv(
        csv,
        date_col="date",
        unit_col="unit",
        value_col="scf",
        start_date="2015-01-01",
        end_date="2018-12-31",
    )
    assert len(obs) == 1
    assert obs.targets[0]["t"] == pd.Timestamp("2016-01-15")


# --------------------------------------------------------------------------- #
# Raster/NetCDF loader
# --------------------------------------------------------------------------- #
def _write_synthetic_modis(tmp_path):
    """Write a 4x4 unit-ids GeoTIFF and a 2-date snow-cover NetCDF on the same grid."""
    xr = pytest.importorskip("xarray")
    pytest.importorskip("rioxarray")  # registers the .rio accessor
    pytest.importorskip("rasterio")
    pytest.importorskip("netCDF4")

    x0, y0 = 2600000.0, 1200000.0
    xs = x0 + np.array([0.5, 1.5, 2.5, 3.5])
    ys = y0 - np.array([0.5, 1.5, 2.5, 3.5])  # descending (north-up)
    unit = np.array(
        [[1, 1, 1, 1], [1, 1, 1, 1], [2, 2, 2, 2], [2, 2, 2, 2]], dtype="int32"
    )
    uda = xr.DataArray(unit, coords={"y": ys, "x": xs}, dims=("y", "x"))
    uda = uda.rio.write_crs("epsg:2056")
    uda.rio.write_nodata(0, inplace=True)
    tif = tmp_path / "units.tif"
    uda.rio.to_raster(tif)

    # Snow cover [%], with one cloud (nodata=255) pixel in unit 1 at the first date.
    scf0 = np.where(unit == 1, 80.0, 20.0)
    scf0[0, 0] = 255.0
    scf1 = np.full((4, 4), 100.0)
    data = np.stack([scf0, scf1])  # (time, y, x)
    times = pd.to_datetime(["2016-01-15", "2016-02-15"])
    da = xr.DataArray(
        data,
        coords={"time": times, "y": ys, "x": xs},
        dims=("time", "y", "x"),
        name="scf",
    )
    nc = tmp_path / "modis.nc"
    da.to_dataset().to_netcdf(nc)
    return nc, tif


def test_from_netcdf_aggregates_per_unit(tmp_path):
    nc, tif = _write_synthetic_modis(tmp_path)
    obs = SnowCoverObservations.from_netcdf(
        nc, tif, var_name="scf", data_crs=2056, value_scale=0.01, nodata=255.0
    )
    by_key = {(t["t"], t["unit_id"]): t["value"] for t in obs.targets}
    assert len(obs) == 4
    # Unit 1, first date: the cloud pixel is ignored, the remaining 7 are 80 % -> 0.8.
    assert abs(by_key[(pd.Timestamp("2016-01-15"), 1)] - 0.8) < 1e-6
    assert abs(by_key[(pd.Timestamp("2016-01-15"), 2)] - 0.2) < 1e-6
    assert abs(by_key[(pd.Timestamp("2016-02-15"), 1)] - 1.0) < 1e-6
    assert abs(by_key[(pd.Timestamp("2016-02-15"), 2)] - 1.0) < 1e-6


def test_from_netcdf_min_valid_ratio_drops_cloudy(tmp_path):
    nc, tif = _write_synthetic_modis(tmp_path)
    # Unit 1 on the first date has 7/8 = 0.875 valid pixels; require 0.9 -> dropped.
    obs = SnowCoverObservations.from_netcdf(
        nc,
        tif,
        var_name="scf",
        data_crs=2056,
        value_scale=0.01,
        nodata=255.0,
        min_valid_ratio=0.9,
    )
    keys = {(t["t"], t["unit_id"]) for t in obs.targets}
    assert (pd.Timestamp("2016-01-15"), 1) not in keys
    assert (pd.Timestamp("2016-02-15"), 1) in keys


def test_from_netcdf_cache_roundtrip(tmp_path):
    nc, tif = _write_synthetic_modis(tmp_path)
    cache = tmp_path / "cache"
    kw = dict(
        raster_hydro_units=tif,
        var_name="scf",
        data_crs=2056,
        value_scale=0.01,
        nodata=255.0,
        cache_dir=str(cache),
    )
    first = SnowCoverObservations.from_netcdf(nc, **kw)
    assert len(list(cache.glob("snow_cover_*.csv"))) == 1

    # Second call reads the cache and yields identical observations.
    second = SnowCoverObservations.from_netcdf(nc, **kw)
    assert len(second) == len(first)
    assert np.allclose(np.sort(second.observed()), np.sort(first.observed()))

    # A different option is a cache miss -> a second, distinct cache file.
    SnowCoverObservations.from_netcdf(nc, **{**kw, "valid_max": 50})
    assert len(list(cache.glob("snow_cover_*.csv"))) == 2


def _write_modis_with_quality_code(tmp_path):
    """Like the synthetic MODIS but with a quality/error code (>100) in unit 2."""
    xr = pytest.importorskip("xarray")
    pytest.importorskip("rioxarray")
    pytest.importorskip("rasterio")
    pytest.importorskip("netCDF4")

    x0, y0 = 2600000.0, 1200000.0
    xs = x0 + np.array([0.5, 1.5, 2.5, 3.5])
    ys = y0 - np.array([0.5, 1.5, 2.5, 3.5])
    unit = np.array(
        [[1, 1, 1, 1], [1, 1, 1, 1], [2, 2, 2, 2], [2, 2, 2, 2]], dtype="int32"
    )
    uda = xr.DataArray(unit, coords={"y": ys, "x": xs}, dims=("y", "x"))
    uda = uda.rio.write_crs("epsg:2056")
    uda.rio.write_nodata(0, inplace=True)
    tif = tmp_path / "units.tif"
    uda.rio.to_raster(tif)

    scf0 = np.where(unit == 1, 80.0, 20.0)
    scf0[2, 0] = 200.0  # a quality/error code in unit 2
    scf1 = np.full((4, 4), 100.0)
    data = np.stack([scf0, scf1])
    times = pd.to_datetime(["2016-01-15", "2016-02-15"])
    da = xr.DataArray(
        data,
        coords={"time": times, "y": ys, "x": xs},
        dims=("time", "y", "x"),
        name="scf",
    )
    nc = tmp_path / "modis.nc"
    da.to_dataset().to_netcdf(nc)
    return nc, tif


def test_from_netcdf_valid_max_filters_quality_codes(tmp_path):
    nc, tif = _write_modis_with_quality_code(tmp_path)
    # Without filtering, the 200 code inflates unit 2: (7*20 + 200)/8 * 0.01 = 0.425.
    raw = SnowCoverObservations.from_netcdf(
        nc, tif, var_name="scf", data_crs=2056, value_scale=0.01
    )
    raw_by_key = {(t["t"], t["unit_id"]): t["value"] for t in raw.targets}
    assert abs(raw_by_key[(pd.Timestamp("2016-01-15"), 2)] - 0.425) < 1e-6
    # With valid_max=100 the code is dropped, leaving the seven 20 % pixels -> 0.2.
    obs = SnowCoverObservations.from_netcdf(
        nc, tif, var_name="scf", data_crs=2056, value_scale=0.01, valid_max=100.0
    )
    by_key = {(t["t"], t["unit_id"]): t["value"] for t in obs.targets}
    assert abs(by_key[(pd.Timestamp("2016-01-15"), 2)] - 0.2) < 1e-6


def test_from_hdf5_matches_netcdf(tmp_path):
    pytest.importorskip("h5netcdf")
    pytest.importorskip("h5py")  # h5netcdf's default backend for read/write
    xr = pytest.importorskip("xarray")
    pytest.importorskip("rioxarray")
    pytest.importorskip("rasterio")

    nc, tif = _write_synthetic_modis(tmp_path)
    # Re-encode the same data as HDF5 and read it back through from_hdf5.
    h5 = tmp_path / "modis.h5"
    xr.open_dataset(nc).to_netcdf(h5, engine="h5netcdf")

    obs = SnowCoverObservations.from_hdf5(
        h5,
        tif,
        var_name="scf",
        data_crs=2056,
        value_scale=0.01,
        nodata=255.0,
        engine="h5netcdf",
    )
    by_key = {(t["t"], t["unit_id"]): t["value"] for t in obs.targets}
    assert len(obs) == 4
    assert abs(by_key[(pd.Timestamp("2016-01-15"), 1)] - 0.8) < 1e-6
    assert abs(by_key[(pd.Timestamp("2016-02-15"), 2)] - 1.0) < 1e-6


# --------------------------------------------------------------------------- #
# MODIS (HDF-EOS) loader
# --------------------------------------------------------------------------- #
_STRUCT_META = (
    "GROUP=GridStructure\n"
    "\tGROUP=GRID_1\n"
    '\t\tGridName="MOD_Grid_Snow_500m"\n'
    "\t\tXDim=2400\n"
    "\t\tYDim=2400\n"
    "\t\tUpperLeftPointMtrs=(0.000000,5559752.598333)\n"
    "\t\tLowerRightMtrs=(1111950.519667,4447802.078667)\n"
    "\t\tProjection=GCTP_SNSOID\n"
    "\t\tProjParams=(6371007.181000,0,0,0,0,0,0,0,0,0,0,0,0)\n"
)


def test_parse_struct_metadata():
    ul, lr, nx, ny, radius = _parse_struct_metadata(_STRUCT_META)
    assert ul == [0.0, 5559752.598333]
    assert lr == [1111950.519667, 4447802.078667]
    assert nx == 2400 and ny == 2400
    assert radius == 6371007.181


def test_date_from_modis_filename():
    name = "MOD10A1.A2025361.h18v04.061.2025363025242.hdf"
    d = _date_from_name(name, r"A(\d{7})", "%Y%j", None)
    assert d == pd.Timestamp("2025-12-27")  # 2025, day-of-year 361


def test_cache_key_distinguishes_config_and_discretization(tmp_path):
    raster_a = tmp_path / "units_a.tif"
    raster_a.write_bytes(b"discretization-A")
    raster_b = tmp_path / "units_b.tif"
    raster_b.write_bytes(b"discretization-B")
    cfg = {"variable": "NDSI_Snow_Cover", "valid_max": 100}
    src = [("MOD10A1.A2025361.hdf", 123, 456)]

    key = _cache_key(raster_a, cfg, src)
    # Deterministic for identical inputs.
    assert key == _cache_key(raster_a, cfg, src)
    # A different discretization (raster) gives a different key.
    assert key != _cache_key(raster_b, cfg, src)
    # A different option gives a different key.
    assert key != _cache_key(raster_a, {**cfg, "valid_max": 50}, src)
    # A changed input-file signature gives a different key.
    assert key != _cache_key(raster_a, cfg, [("MOD10A1.A2025361.hdf", 999, 456)])


# Sample MOD10A1 tiles may be dropped in the repo's tmp/ folder for a real read.
_REPO_ROOT = Path(__file__).resolve().parents[2]
_MODIS_SAMPLES = sorted((_REPO_ROOT / "tmp").glob("MOD10A1*.hdf"))
_SITTER_UNIT_IDS = TEST_FILES_DIR / "ch_sitter_appenzell" / "unit_ids.tif"


@pytest.mark.skipif(
    not _MODIS_SAMPLES or not _SITTER_UNIT_IDS.exists(),
    reason="No MOD10A1 sample tiles in tmp/ (or missing Sitter unit_ids.tif)",
)
def test_from_modis_sample_tiles():
    pytest.importorskip("rioxarray")
    pytest.importorskip("netCDF4")
    obs = SnowCoverObservations.from_modis(
        _REPO_ROOT / "tmp",
        raster_hydro_units=_SITTER_UNIT_IDS,
        file_pattern="MOD10A1*.hdf",
        value_scale=0.01,
        valid_min=0,
        valid_max=100,
    )
    assert len(obs) > 0
    vals = obs.observed()
    # Quality codes (>100) were filtered and the % rescaled to a [0, 1] fraction.
    assert np.all((vals >= 0.0) & (vals <= 1.0))
    # Snow cover should increase with elevation: the lowest unit (id 1) has less snow
    # than a mid-elevation unit, on the first available date.
    first_date = sorted({t["t"] for t in obs.targets})[0]
    by_unit = {t["unit_id"]: t["value"] for t in obs.targets if t["t"] == first_date}
    if 1 in by_unit and 15 in by_unit:
        assert by_unit[1] < by_unit[15]


@pytest.mark.skipif(
    not _MODIS_SAMPLES or not _SITTER_UNIT_IDS.exists(),
    reason="No MOD10A1 sample tiles in tmp/ (or missing Sitter unit_ids.tif)",
)
def test_from_modis_cache_roundtrip(tmp_path):
    pytest.importorskip("rioxarray")
    pytest.importorskip("netCDF4")
    kwargs = dict(
        raster_hydro_units=_SITTER_UNIT_IDS,
        file_pattern="MOD10A1*.hdf",
        value_scale=0.01,
        valid_min=0,
        valid_max=100,
        cache_dir=str(tmp_path),
    )
    first = SnowCoverObservations.from_modis(_REPO_ROOT / "tmp", **kwargs)
    cache_files = list(tmp_path.glob("snow_cover_*.csv"))
    assert len(cache_files) == 1  # aggregation was cached

    # Second call reads the cache and yields identical observations.
    second = SnowCoverObservations.from_modis(_REPO_ROOT / "tmp", **kwargs)
    assert len(second) == len(first)
    assert np.allclose(np.sort(second.observed()), np.sort(first.observed()))

    # A different option is a cache miss -> a second, distinct cache file.
    SnowCoverObservations.from_modis(
        _REPO_ROOT / "tmp", **{**kwargs, "min_valid_ratio": 0.9}
    )
    assert len(list(tmp_path.glob("snow_cover_*.csv"))) == 2


# --------------------------------------------------------------------------- #
# Model-based tests (a small SOCONT run, reused across tests)
# --------------------------------------------------------------------------- #
@pytest.fixture(scope="module")
def snow_run():
    """Set up and run a small open+glacier SOCONT model on the Rhone catchment."""
    pytest.importorskip("rasterio")
    work_dir = Path(tempfile.gettempdir()) / f"hb_sctest_{uuid.uuid4().hex}"
    work_dir.mkdir(parents=True, exist_ok=True)

    catchment = hb.Catchment(
        GLETSCH_DIR / "outline.shp",
        land_cover_types=["open", "glacier"],
        land_cover_names=["open", "glacier"],
    )
    catchment.extract_dem(GLETSCH_DIR / "dem.tif")
    catchment.create_elevation_bands(method="equal_intervals", distance=200)

    ge = hb.preprocessing.GlacierEvolutionDeltaH()
    ge.compute_initial_ice_thickness(
        catchment, ice_thickness=GLACIER_DIR / "ice_thickness.tif"
    )

    model = models.Socont(
        soil_storage_nb=2,
        surface_runoff="linear_storage",
        record_all=True,
        land_cover_types=["open", "glacier"],
        land_cover_names=["open", "glacier"],
    )
    params = model.generate_parameters()
    params.set_values(
        {
            "A": 458,
            "a_snow": 4,
            "a_ice": 8,
            "k_slow_1": 0.9,
            "k_slow_2": 0.8,
            "k_quick": 1,
            "percol": 9.8,
            "k_snow": 0.1,
            "k_ice": 0.5,
        }
    )

    forcing = hb.Forcing(catchment.hydro_units)
    forcing.load_station_data_from_csv(
        GLETSCH_DIR / "meteo.csv",
        column_time="date",
        time_format="%d/%m/%Y",
        content={"precipitation": "precip(mm/day)", "temperature": "temp(C)"},
    )
    forcing.spatialize_from_station_data(
        variable="temperature", ref_elevation=2702, gradient=-0.6
    )
    forcing.spatialize_from_station_data(
        variable="precipitation", ref_elevation=2702, gradient=0.05
    )
    forcing.compute_pet(method="Oudin", use=["t", "lat"], lat=46.6)

    model.setup(
        spatial_structure=catchment.hydro_units,
        output_path=str(work_dir),
        start_date=START_DATE,
        end_date=END_DATE,
    )
    model.run(parameters=params, forcing=forcing)
    return model, params, forcing, catchment


def test_required_recordings(snow_run):
    model, _, _, _ = snow_run
    req = SnowCoverObservations().required_recordings(model)
    assert req.fractions is True
    labels = req.value_labels()
    assert "open_snowpack:snow_content" in labels
    assert "glacier_snowpack:snow_content" in labels


def test_required_recordings_land_cover_filter(snow_run):
    model, _, _, _ = snow_run
    req = SnowCoverObservations(land_covers=["open"]).required_recordings(model)
    assert req.value_labels() == ["open_snowpack:snow_content"]


def _winter_targets(model, value=0.5, n_units=3, date="2016-01-15"):
    ids = model.get_recorded_hydro_unit_ids()
    obs = SnowCoverObservations(swe_full=50.0)
    obs.targets = [
        {"t": pd.Timestamp(date), "unit_id": int(ids[i]), "value": value}
        for i in range(min(n_units, len(ids)))
    ]
    return obs


def test_simulated_is_a_valid_fraction(snow_run):
    model, _, _, _ = snow_run
    obs = _winter_targets(model)
    sim = obs.simulated(model)
    assert sim.shape == (len(obs),)
    finite = sim[np.isfinite(sim)]
    assert finite.size > 0
    assert np.all((finite >= 0.0) & (finite <= 1.0))


def test_simulated_monotonic_in_threshold(snow_run):
    """A smaller full-cover threshold yields an equal-or-larger cover fraction."""
    model, _, _, _ = snow_run
    ids = model.get_recorded_hydro_unit_ids()
    targets = [
        {"t": pd.Timestamp("2016-01-15"), "unit_id": int(ids[i]), "value": 0.5}
        for i in range(min(3, len(ids)))
    ]

    low = SnowCoverObservations(swe_full=1e-6)
    low.targets = [dict(t) for t in targets]
    high = SnowCoverObservations(swe_full=1e9)
    high.targets = [dict(t) for t in targets]
    mid = SnowCoverObservations(swe_full=50.0)
    mid.targets = [dict(t) for t in targets]

    s_low = low.simulated(model)
    s_high = high.simulated(model)
    s_mid = mid.simulated(model)
    assert np.all(s_high <= s_mid + 1e-9)
    assert np.all(s_mid <= s_low + 1e-9)
    # A near-zero threshold saturates a snowy winter day to (almost) full cover.
    assert np.nanmax(s_low) > 0.99
    # A huge threshold collapses the cover fraction towards zero.
    assert np.nanmax(s_high) < 1e-3


def test_simulated_out_of_range_and_unknown_unit_are_nan(snow_run):
    model, _, _, _ = snow_run
    ids = model.get_recorded_hydro_unit_ids()
    obs = SnowCoverObservations(swe_full=50.0)
    obs.targets = [
        {"t": pd.Timestamp("1990-01-01"), "unit_id": int(ids[0]), "value": 0.5},
        {"t": pd.Timestamp("2016-01-15"), "unit_id": 999999, "value": 0.5},
    ]
    sim = obs.simulated(model)
    assert np.isnan(sim).all()


def _load_discharge():
    """Load and trim the Gletsch discharge to the test period."""
    obs = hb.DischargeObservations()
    obs.load_from_csv(
        GLETSCH_DIR / "discharge.csv",
        column_time="Date",
        time_format="%d/%m/%Y",
        content={"discharge": "Discharge (mm/d)"},
    )
    sel = np.asarray(
        (obs.time >= pd.Timestamp(START_DATE)) & (obs.time <= pd.Timestamp(END_DATE))
    )
    obs.data = [d[sel] for d in obs.data]
    obs.time = obs.time[sel].reset_index(drop=True)
    return obs


@pytest.mark.parametrize("mode", ["objective", "constraint"])
def test_calibration_with_snow_cover_runs(snow_run, mode):
    """A small calibration with a snow-cover term runs end-to-end."""
    pytest.importorskip("spotpy")
    model, params, forcing, _ = snow_run
    params.allow_changing = ["a_snow", "a_ice"]
    obs = _load_discharge()

    ids = model.get_recorded_hydro_unit_ids()
    sc = SnowCoverObservations(swe_full=50.0, mode=mode)
    if mode == "constraint":
        sc.tolerance = 1.0  # generous: snow cover fraction is in [0, 1]
    for date in ("2015-01-15", "2016-01-15", "2017-01-15"):
        for i in range(min(3, len(ids))):
            sc.targets.append(
                {"t": pd.Timestamp(date), "unit_id": int(ids[i]), "value": 0.6}
            )

    spot_setup = trainer.SpotpySetup(
        model,
        params,
        forcing,
        obs,
        warmup=180,
        obj_func="kge_2012",
        extra_observations=[sc],
        combine="weighted",
    )
    assert sum(spot_setup._extra_lengths) > 0
    sampler = trainer.calibrate(spot_setup, "sceua", 12, dbformat="ram")
    results = sampler.getdata()
    assert len(results) > 0
    assert np.isfinite(results["like1"]).any()
