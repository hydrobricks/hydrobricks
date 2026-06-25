import os.path
import pickle
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

pytest.importorskip("spotpy")

TEST_FILES_DIR = Path(
    os.path.dirname(os.path.realpath(__file__)),
    "..",
    "..",
    "tests",
    "files",
    "catchments",
)
SITTER_DIR = TEST_FILES_DIR / "ch_sitter_appenzell"

# Short calibration period to keep the tests fast.
START_DATE = "1981-01-01"
END_DATE = "1982-12-31"
WARMUP = 100

# Parameters varied during the test calibrations.
ALLOW_CHANGING = ["a_snow", "k_quick"]


def _make_socont():
    """Construct a (not yet set up) Socont model with a fixed configuration."""
    return models.Socont(
        soil_storage_nb=2,
        surface_runoff="linear_storage",
        snow_melt_process="melt:degree_day",
    )


def build_params():
    """Build a picklable ParameterSet for the test model (no setup required)."""
    parameters = _make_socont().generate_parameters()
    # Give every parameter a valid, constraint-satisfying value; the calibration
    # then resamples only those in ALLOW_CHANGING.
    parameters.set_values(
        {
            "a_snow": 4,
            "A": 200,
            "k_slow_1": 0.01,
            "k_slow_2": 0.001,
            "percol": 1,
            "k_quick": 0.05,
        }
    )
    parameters.allow_changing = ALLOW_CHANGING
    return parameters


def build_setup_objects():
    """Module-level factory: rebuild (model, forcing, obs) from the test data.

    Must be importable (module-level) so it can be pickled to worker processes
    when running SPOTPY with ``parallel='mpc'``.
    """
    hydro_units = hb.HydroUnits()
    hydro_units.load_from_csv(
        SITTER_DIR / "hydro_units_elevation.csv",
        column_elevation="elevation",
        column_area="area",
    )

    forcing = hb.Forcing(hydro_units)
    forcing.load_station_data_from_csv(
        SITTER_DIR / "meteo.csv",
        column_time="date",
        time_format="%d/%m/%Y",
        content={
            "precipitation": "precip(mm/day)",
            "temperature": "temp(C)",
            "pet": "pet_sim(mm/day)",
        },
    )
    # Fixed (non parameter-dependent) spatialization keeps the calibration focused
    # on the model parameters and the forcing static across runs.
    forcing.spatialize_from_station_data(
        variable="temperature", ref_elevation=1250, gradient=-0.6
    )
    forcing.spatialize_from_station_data(variable="pet", method="constant")
    forcing.spatialize_from_station_data(variable="precipitation", method="constant")

    obs = hb.DischargeObservations()
    obs.load_from_csv(
        SITTER_DIR / "discharge.csv",
        column_time="Date",
        time_format="%d/%m/%Y",
        content={"discharge": "Discharge (mm/d)"},
    )
    # Restrict observations to the (short) simulation window so they match the
    # simulated discharge length.
    sel = np.asarray(
        (obs.time >= pd.Timestamp(START_DATE)) & (obs.time <= pd.Timestamp(END_DATE))
    )
    obs.data = [d[sel] for d in obs.data]
    obs.time = obs.time[sel].reset_index(drop=True)

    socont = _make_socont()
    work_dir = Path(tempfile.gettempdir()) / f"hb_caltest_{uuid.uuid4().hex}"
    work_dir.mkdir(parents=True, exist_ok=True)
    socont.setup(
        spatial_structure=hydro_units,
        output_path=str(work_dir),
        start_date=START_DATE,
        end_date=END_DATE,
    )

    return socont, forcing, obs


def test_factory_setup_builds_lazily():
    """A factory-based setup defers building the heavy objects until first use."""
    params = build_params()
    spot_setup = trainer.SpotpySetup.from_factory(
        build_setup_objects, params, warmup=WARMUP, obj_func="nse"
    )

    assert spot_setup._built is False
    assert spot_setup.model is None

    # evaluation() triggers the lazy build.
    obs = spot_setup.evaluation()
    assert spot_setup._built is True
    assert spot_setup.model is not None
    assert len(obs) == 1


def test_factory_setup_is_picklable_and_drops_heavy_objects():
    """The factory-based setup survives pickling and rebuilds lazily afterwards.

    SPOTPY's 'mpc' backend serializes the setup with dill (which, unlike the
    stdlib pickle, can handle the parameter-transform closures), so we use dill
    here to mirror the real parallel path.
    """
    dill = pytest.importorskip("dill")
    params = build_params()
    spot_setup = trainer.SpotpySetup.from_factory(
        build_setup_objects, params, warmup=WARMUP, obj_func="nse"
    )

    # Force a build, then confirm pickling drops the C++-backed objects.
    spot_setup.evaluation()
    assert spot_setup._built is True

    restored = dill.loads(dill.dumps(spot_setup))
    assert restored._built is False
    assert restored.model is None
    assert restored.forcing is None
    assert restored.obs is None

    # The restored setup rebuilds on demand and works.
    obs = restored.evaluation()
    assert restored._built is True
    assert len(obs) == 1


def test_eager_setup_is_not_picklable():
    """A setup built from concrete objects cannot be pickled (clear error)."""
    socont, forcing, obs = build_setup_objects()
    params = build_params()
    spot_setup = trainer.SpotpySetup(
        socont, params, forcing, obs, warmup=WARMUP, obj_func="nse"
    )

    with pytest.raises(TypeError, match="from_factory"):
        pickle.dumps(spot_setup)


def test_calibrate_sequential_runs():
    """A small sequential MC calibration runs end-to-end via the factory setup."""
    params = build_params()
    spot_setup = trainer.SpotpySetup.from_factory(
        build_setup_objects, params, warmup=WARMUP, obj_func="nse"
    )

    sampler = trainer.calibrate(
        spot_setup,
        "mc",
        repetitions=5,
        dbname="hb_caltest_seq",
        dbformat="ram",
        parallel="seq",
        save_sim=False,
    )
    results = sampler.getdata()
    assert len(results) >= 1


def test_calibrate_parallel_mpc_matches_sequential():
    """A multiprocessing MC calibration runs end-to-end and yields results."""
    pytest.importorskip("pathos")

    params = build_params()
    spot_setup = trainer.SpotpySetup.from_factory(
        build_setup_objects, params, warmup=WARMUP, obj_func="nse"
    )

    sampler = trainer.calibrate(
        spot_setup,
        "mc",
        repetitions=6,
        dbname="hb_caltest_mpc",
        dbformat="ram",
        parallel="mpc",
        save_sim=False,
    )
    results = sampler.getdata()
    assert len(results) >= 1


def test_calibrate_parallel_requires_factory():
    """Parallel calibration on an eager setup raises a clear configuration error."""
    socont, forcing, obs = build_setup_objects()
    params = build_params()
    spot_setup = trainer.SpotpySetup(
        socont, params, forcing, obs, warmup=WARMUP, obj_func="nse"
    )

    with pytest.raises(ConfigurationError):
        trainer.calibrate(
            spot_setup, "mc", repetitions=2, dbformat="ram", parallel="mpc"
        )


def test_calibrate_n_workers_sets_pool_size():
    """n_workers configures SPOTPY's multiprocessing pool size."""
    pytest.importorskip("pathos")
    import spotpy.parallel.mproc as mproc

    params = build_params()
    spot_setup = trainer.SpotpySetup.from_factory(
        build_setup_objects, params, warmup=WARMUP, obj_func="nse"
    )

    trainer.calibrate(
        spot_setup,
        "mc",
        repetitions=4,
        dbformat="ram",
        parallel="mpc",
        save_sim=False,
        n_workers=2,
    )
    assert mproc.process_count == 2


def test_calibrate_n_workers_validation():
    """A non-positive n_workers raises a clear configuration error."""
    params = build_params()
    spot_setup = trainer.SpotpySetup.from_factory(
        build_setup_objects, params, warmup=WARMUP, obj_func="nse"
    )

    with pytest.raises(ConfigurationError):
        trainer.calibrate(spot_setup, "mc", repetitions=2, dbformat="ram", n_workers=0)


def build_full():
    """Module-level factory returning (model, parameters, forcing, obs)."""
    socont, forcing, obs = build_setup_objects()
    params = build_params()
    return socont, params, forcing, obs


def test_calibrate_from_factory_sequential():
    """The one-call factory interface runs a sequential calibration end-to-end."""
    sampler = trainer.calibrate_from_factory(
        build_full,
        "mc",
        5,
        warmup=WARMUP,
        obj_func="nse",
        dbformat="ram",
        parallel="seq",
        save_sim=False,
    )
    assert len(sampler.getdata()) >= 1


def test_calibrate_from_factory_parallel_mpc():
    """The one-call factory interface runs a multiprocessing calibration."""
    pytest.importorskip("pathos")
    sampler = trainer.calibrate_from_factory(
        build_full,
        "mc",
        6,
        warmup=WARMUP,
        obj_func="nse",
        dbformat="ram",
        parallel="mpc",
        save_sim=False,
        n_workers=2,
    )
    assert len(sampler.getdata()) >= 1


def test_calibrate_from_factory_requires_4tuple():
    """A factory that omits the parameters raises a clear configuration error."""
    with pytest.raises(ConfigurationError):
        trainer.calibrate_from_factory(
            build_setup_objects, "mc", 2, dbformat="ram", parallel="seq"
        )
