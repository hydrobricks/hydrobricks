import os.path
import tempfile
import types
import uuid
from pathlib import Path

import numpy as np
import pandas as pd
import pytest

import hydrobricks as hb
import hydrobricks.models as models
import hydrobricks.trainer as trainer
from hydrobricks._exceptions import ConfigurationError, DataError
from hydrobricks.evaluation import GlacierMassBalanceObservations

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
MB_WHOLE = GLACIER_DIR / "massbalance_fixdate.csv"
MB_BINS = GLACIER_DIR / "massbalance_fixdate_elevationbins.csv"

START_DATE = "2014-01-01"
END_DATE = "2018-12-31"


# --------------------------------------------------------------------------- #
# Loader tests (no model needed)
# --------------------------------------------------------------------------- #
def test_load_whole_glacier_annual():
    obs = GlacierMassBalanceObservations.from_glamos(
        MB_WHOLE, kind="whole", glacier_id="B43-03", balance_types=("annual",)
    )
    assert len(obs) > 0
    assert obs.granularity == "whole"
    # All annual targets, no elevation band, span ~ one hydrological year.
    t = obs.targets[0]
    assert t["balance_type"] == "annual"
    assert t["band_lo"] is None
    assert (t["t1"] - t["t0"]).days > 300


def test_periods_follow_hydrological_year_and_seasons():
    obs = GlacierMassBalanceObservations.from_glamos(
        MB_WHOLE,
        kind="whole",
        glacier_id="B43-03",
        balance_types=("annual", "winter", "summer"),
    )
    # Pick the three balances of one observation row (annual & winter share the
    # start date; summer starts at the winter end and ends with the annual).
    annual = next(t for t in obs.targets if t["balance_type"] == "annual")
    winter = next(
        t
        for t in obs.targets
        if t["balance_type"] == "winter" and t["t0"] == annual["t0"]
    )
    summer = next(
        t
        for t in obs.targets
        if t["balance_type"] == "summer" and t["t1"] == annual["t1"]
    )
    # Hydrological year starts in October, winter ends in spring, summer ends in fall.
    assert annual["t0"].month == 10
    assert annual["t1"].month == 9
    assert winter["t1"] == summer["t0"]


def test_mixed_date_formats_parse_correctly():
    # GLAMOS mixes yyyy-mm-dd and dd/mm/yyyy in the same column. October starts must
    # never be parsed as January (the dayfirst trap), and 1900+ rows use dd/mm/yyyy.
    obs = GlacierMassBalanceObservations.from_glamos(
        MB_WHOLE, kind="whole", glacier_id="B43-03", balance_types=("annual",)
    )
    assert all(t["t0"].month == 10 for t in obs.targets)
    assert any(t["t0"].year >= 1900 for t in obs.targets)


def test_load_elevation_bins():
    obs = GlacierMassBalanceObservations.from_glamos(
        MB_BINS, kind="elevationbins", balance_types=("annual",)
    )
    assert obs.granularity == "elevationbins"
    t = obs.targets[0]
    assert t["band_lo"] is not None
    assert t["band_hi"] > t["band_lo"]


def test_restrict_to_period():
    obs = GlacierMassBalanceObservations.from_glamos(
        MB_WHOLE,
        kind="whole",
        glacier_id="B43-03",
        balance_types=("annual",),
        start_date="2010-01-01",
        end_date="2020-12-31",
    )
    assert len(obs) > 0
    for t in obs.targets:
        assert t["t0"] >= pd.Timestamp("2010-01-01")
        assert t["t1"] <= pd.Timestamp("2020-12-31")


def test_unknown_glacier_id_raises():
    with pytest.raises(DataError):
        GlacierMassBalanceObservations.from_glamos(
            MB_WHOLE, kind="whole", glacier_id="does-not-exist"
        )


def test_invalid_kind_and_balance_type_raise():
    with pytest.raises(ConfigurationError):
        GlacierMassBalanceObservations.from_glamos(MB_WHOLE, kind="nonsense")
    with pytest.raises(ConfigurationError):
        GlacierMassBalanceObservations.from_glamos(
            MB_WHOLE, kind="whole", balance_types=("nonsense",)
        )


def test_from_csv_explicit_dates(tmp_path):
    """A generic (non-GLAMOS) CSV with explicit period columns loads correctly."""
    csv = tmp_path / "mb.csv"
    csv.write_text(
        "start,end,mb\n2015-10-01,2016-09-30,-500\n2016-10-01,2017-09-30,300\n"
    )
    obs = GlacierMassBalanceObservations.from_csv(
        csv, value_col="mb", date_start_col="start", date_end_col="end"
    )
    assert len(obs) == 2
    assert obs.targets[0]["t0"] == pd.Timestamp("2015-10-01")
    assert obs.targets[0]["t1"] == pd.Timestamp("2016-09-30")
    assert obs.values.tolist() == [-500.0, 300.0]


def test_from_csv_year_convention(tmp_path):
    """Periods can be derived from a year column with a hydrological-year start."""
    csv = tmp_path / "mb_year.csv"
    csv.write_text("year,mb\n2015,-0.5\n2016,0.3\n")
    obs = GlacierMassBalanceObservations.from_csv(
        csv,
        value_col="mb",
        year_col="year",
        hydro_year_start="October",
        value_unit="m_we",  # converted to mm w.e.
    )
    assert len(obs) == 2
    assert obs.targets[0]["t0"] == pd.Timestamp("2015-10-01")
    assert obs.targets[0]["t1"] == pd.Timestamp("2016-09-30")
    assert obs.values.tolist() == [-500.0, 300.0]


# --------------------------------------------------------------------------- #
# Model-based tests (a small static-glacier run, reused across tests)
# --------------------------------------------------------------------------- #
@pytest.fixture(scope="module")
def glacier_run():
    """Set up and run a small static-glacier SOCONT model on the Rhone catchment."""
    pytest.importorskip("rasterio")
    work_dir = Path(tempfile.gettempdir()) / f"hb_mbtest_{uuid.uuid4().hex}"
    work_dir.mkdir(parents=True, exist_ok=True)

    catchment = hb.Catchment(
        GLETSCH_DIR / "outline.shp",
        land_cover_types=["open", "glacier"],
        land_cover_names=["open", "glacier"],
    )
    catchment.extract_dem(GLETSCH_DIR / "dem.tif")
    catchment.create_elevation_bands(method="equal_intervals", distance=200)

    ge = hb.preprocessing.GlacierEvolutionDeltaH()
    gdf = ge.compute_initial_ice_thickness(
        catchment, ice_thickness=GLACIER_DIR / "ice_thickness.tif"
    )
    areas = (
        pd.DataFrame(
            {
                "hydro_unit": gdf[("hydro_unit_id", "-")].to_numpy(),
                "area": gdf[("glacier_area", "m2")].to_numpy(),
            }
        )
        .groupby("hydro_unit", as_index=False)["area"]
        .sum()
    )
    catchment.initialize_area_from_land_cover_change("glacier", areas)

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


def test_in_memory_accessor_matches_netcdf(glacier_run):
    """The in-memory glacier series equal the ones read back from netCDF."""
    model, _, _, _ = glacier_run
    work_dir = Path(tempfile.gettempdir()) / f"hb_mbdump_{uuid.uuid4().hex}"
    work_dir.mkdir(parents=True, exist_ok=True)
    model.dump_outputs(str(work_dir))
    results = hb.Results(str(work_dir) + "/results.nc")

    for label in ("glacier:melt:output", "glacier_snowpack:snow_content"):
        mem = model.get_recorded_hydro_unit_values(label)
        nc = results.get_hydro_units_values(label)
        assert mem.shape == nc.shape  # (units, time)
        assert np.allclose(np.nan_to_num(mem), np.nan_to_num(nc), atol=1e-9)


def test_recorded_time_axis_length(glacier_run):
    model, _, _, _ = glacier_run
    time = model.get_recorded_time()
    melt = model.get_recorded_hydro_unit_values("glacier:melt:output")
    assert len(time) == melt.shape[1]


def test_simulated_mass_balance_tracks_observations(glacier_run):
    """The simulated whole-glacier mass balance correlates with the observations."""
    model, _, _, _ = glacier_run
    obs = GlacierMassBalanceObservations.from_glamos(
        MB_WHOLE,
        kind="whole",
        glacier_id="B43-03",
        balance_types=("annual", "winter", "summer"),
        start_date=START_DATE,
        end_date=END_DATE,
    )
    sim = obs.simulated(model)
    assert sim.shape == (len(obs),)
    mask = np.isfinite(sim)
    assert mask.sum() >= 3
    # Winter balances are positive (accumulation), summer balances negative.
    winter = np.array(
        [s for t, s in zip(obs.targets, sim) if t["balance_type"] == "winter"]
    )
    summer = np.array(
        [s for t, s in zip(obs.targets, sim) if t["balance_type"] == "summer"]
    )
    assert np.nanmean(winter) > 0 > np.nanmean(summer)
    # The seasonal/annual signal should track the observations.
    corr = np.corrcoef(obs.values[mask], sim[mask])[0, 1]
    assert corr > 0.7


def test_per_band_mass_balance_runs(glacier_run):
    model, _, _, _ = glacier_run
    obs = GlacierMassBalanceObservations.from_glamos(
        MB_BINS,
        kind="elevationbins",
        balance_types=("annual",),
        start_date=START_DATE,
        end_date=END_DATE,
    )
    sim = obs.simulated(model)
    assert sim.shape == (len(obs),)
    assert np.isfinite(sim).sum() > 0


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
def test_calibration_with_glacier_mb_runs(glacier_run, mode):
    """A small calibration with a glacier mass-balance term runs end-to-end."""
    pytest.importorskip("spotpy")
    model, params, forcing, _ = glacier_run
    params.allow_changing = ["a_snow", "a_ice"]
    obs = _load_discharge()

    kwargs = {"mode": mode}
    if mode == "constraint":
        kwargs["tolerance"] = 5000.0
    mb = GlacierMassBalanceObservations.from_glamos(
        MB_WHOLE, kind="whole", glacier_id="B43-03", balance_types=("annual",), **kwargs
    )

    spot_setup = trainer.SpotpySetup(
        model,
        params,
        forcing,
        obs,
        warmup=180,
        obj_func="kge_2012",
        invert_obj_func=True,
        extra_observations=[mb],
        combine="weighted",
    )
    # The targets were restricted to the post-warmup simulation period.
    assert sum(spot_setup._extra_lengths) > 0
    sampler = trainer.calibrate(spot_setup, "sceua", 12, dbformat="ram")
    results = sampler.getdata()
    assert len(results) > 0
    # At least one behavioural run was found (finite objective).
    assert np.isfinite(results["like1"]).any()


def test_pareto_objective_returns_vector(glacier_run):
    """combine='pareto' yields a [discharge, mass_balance] objective vector."""
    model, params, forcing, _ = glacier_run
    obs = _load_discharge()
    mb = GlacierMassBalanceObservations.from_glamos(
        MB_WHOLE, kind="whole", glacier_id="B43-03", balance_types=("annual",)
    )
    spot_setup = trainer.SpotpySetup(
        model,
        params,
        forcing,
        obs,
        warmup=180,
        obj_func="kge_2012",
        invert_obj_func=True,
        extra_observations=[mb],
        combine="pareto",
    )
    # A rejected run still returns a consistent-length objective vector.
    worst = spot_setup._worst_score()
    assert isinstance(worst, list) and len(worst) == 2

    # A real evaluation returns the [discharge, mass_balance] objective vector.
    # spotpy hands simulation() an object exposing .name/.random; build one here.
    pars = spot_setup.parameters()
    x = types.SimpleNamespace(name=list(pars["name"]), random=list(pars["random"]))
    sim = spot_setup.simulation(x)
    like = spot_setup.objectivefunction(sim, spot_setup.evaluation(), x)
    assert isinstance(like, list) and len(like) == 2
    assert all(np.isfinite(v) for v in like)


def test_record_all_required(glacier_run):
    """Glacier mass-balance calibration requires record_all=True."""
    pytest.importorskip("spotpy")
    _, params, forcing, catchment = glacier_run

    model = models.Socont(
        soil_storage_nb=2,
        surface_runoff="linear_storage",
        record_all=False,  # not recording -> must be rejected
        land_cover_types=["open", "glacier"],
        land_cover_names=["open", "glacier"],
    )
    work_dir = Path(tempfile.gettempdir()) / f"hb_norec_{uuid.uuid4().hex}"
    work_dir.mkdir(parents=True, exist_ok=True)
    model.setup(
        spatial_structure=catchment.hydro_units,
        output_path=str(work_dir),
        start_date=START_DATE,
        end_date=END_DATE,
    )
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
    mb = GlacierMassBalanceObservations.from_glamos(
        MB_WHOLE, kind="whole", glacier_id="B43-03", balance_types=("annual",)
    )

    with pytest.raises(ConfigurationError, match="record_all"):
        trainer.SpotpySetup(
            model, params, forcing, obs, warmup=180, extra_observations=[mb]
        )
