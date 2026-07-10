import numpy as np
import pandas as pd
import pytest

import hydrobricks as hb
from hydrobricks._exceptions import ConfigurationError, DataError
from hydrobricks.periods import Period, Periods, spinup_to_days


# ---------------------------------------------------------------------------
# Period
# ---------------------------------------------------------------------------
def test_period_basics():
    period = Period("1981-01-01", "1990-12-31", "calibration")
    assert period.bounds == ("1981-01-01", "1990-12-31")
    assert period.n_days == 3652
    assert period.name == "calibration"
    assert len(period.date_range()) == period.n_days


def test_period_invalid_order_raises():
    with pytest.raises(ConfigurationError):
        Period("1990-01-01", "1981-01-01")


def test_period_coerce_from_tuple_and_period():
    period = Period.coerce(("1981-01-01", "1990-12-31"), "calibration")
    assert period.name == "calibration"
    same = Period.coerce(period)
    assert same is period
    with pytest.raises(ConfigurationError):
        Period.coerce("1981-01-01")


def test_period_mask():
    period = Period("2020-01-05", "2020-01-08")
    time = pd.date_range("2020-01-01", "2020-01-10", freq="D")
    mask = period.mask(time)
    assert mask.sum() == 4
    assert mask[4] and mask[7]
    assert not mask[3] and not mask[8]


# ---------------------------------------------------------------------------
# Spin-up specification
# ---------------------------------------------------------------------------
def test_spinup_to_days_parsing():
    assert spinup_to_days(365, "1981-01-01") == 365
    assert spinup_to_days("365d", "1981-01-01") == 365
    # 4 calendar years from 1981 include one leap year (1984).
    assert spinup_to_days("4y", "1981-01-01") == 1461
    assert spinup_to_days(0, "1981-01-01") == 0


def test_spinup_to_days_invalid_raises():
    with pytest.raises(ConfigurationError):
        spinup_to_days("four years", "1981-01-01")
    with pytest.raises(ConfigurationError):
        spinup_to_days(-1, "1981-01-01")
    with pytest.raises(ConfigurationError):
        spinup_to_days(True, "1981-01-01")


# ---------------------------------------------------------------------------
# Periods
# ---------------------------------------------------------------------------
def test_periods_defaults_simulation_to_union_span():
    periods = Periods(
        calibration=("1981-01-01", "2000-12-31"),
        validation=("2001-01-01", "2020-12-31"),
    )
    assert periods.simulation.bounds == ("1981-01-01", "2020-12-31")
    assert periods.full_span is periods.simulation
    assert set(periods.defined_periods()) == {
        "calibration",
        "validation",
        "simulation",
    }


def test_periods_requires_at_least_one_period():
    with pytest.raises(ConfigurationError):
        Periods()


def test_periods_overlap_warns():
    with pytest.warns(UserWarning, match="overlap"):
        Periods(
            calibration=("1981-01-01", "2001-12-31"),
            validation=("2001-01-01", "2020-12-31"),
        )


def test_periods_outside_simulation_span_raises():
    with pytest.raises(ConfigurationError):
        Periods(
            calibration=("1981-01-01", "2000-12-31"),
            simulation=("1990-01-01", "2020-12-31"),
        )


def test_periods_spinup_clamped_to_period():
    periods = Periods(calibration=("2020-01-01", "2020-12-31"), spinup="4y")
    # The calibration period is 366 days; a 4-year spin-up degrades to one
    # replay of the whole period.
    assert periods.spinup_days_for(periods.calibration) == 366


def test_periods_invalid_spinup_raises_at_construction():
    with pytest.raises(ConfigurationError):
        Periods(calibration=("1981-01-01", "2000-12-31"), spinup="abc")


# ---------------------------------------------------------------------------
# evaluate_periods
# ---------------------------------------------------------------------------
class _StubModel:
    """Duck-typed model exposing a finished simulation."""

    def __init__(self, time, sim):
        self._time = time
        self._sim = sim

    def get_outlet_discharge(self):
        return self._sim

    def get_recorded_time(self):
        return self._time


def test_evaluate_periods_slices_by_dates():
    periods = Periods(
        calibration=("2020-01-01", "2020-06-30"),
        validation=("2020-07-01", "2020-12-31"),
        spinup=0,
    )
    time = pd.date_range("2020-01-01", "2020-12-31", freq="D")
    sim = np.linspace(1.0, 2.0, len(time))
    model = _StubModel(time, sim)

    # Perfect observations: every period scores a perfect NSE.
    scores = hb.evaluate_periods(model, sim.copy(), periods, metrics=("nse",))
    assert list(scores.index) == ["calibration", "validation", "simulation"]
    assert np.allclose(scores["nse"], 1.0)

    # Degrade the validation half only: calibration stays perfect.
    obs = sim.copy()
    obs[200:] += np.random.default_rng(42).normal(0, 0.5, len(obs) - 200)
    scores = hb.evaluate_periods(model, obs, periods, metrics=("nse",))
    assert scores.loc["calibration", "nse"] == pytest.approx(1.0)
    assert scores.loc["validation", "nse"] < 1.0


def test_evaluate_periods_multiple_transforms():
    periods = Periods(
        calibration=("2020-01-01", "2020-06-30"),
        validation=("2020-07-01", "2020-12-31"),
        spinup=0,
    )
    time = pd.date_range("2020-01-01", "2020-12-31", freq="D")
    sim = np.linspace(1.0, 2.0, len(time))
    obs = sim.copy()
    obs[200:] += np.random.default_rng(42).normal(0, 0.5, len(obs) - 200)
    model = _StubModel(time, sim)

    scores = hb.evaluate_periods(
        model,
        obs,
        periods,
        metrics=("nse", "kge_2012"),
        transform=[None, "power(0.2)"],
    )
    assert scores.columns.names == ["transform", "metric"]
    assert list(scores.columns) == [
        ("none", "nse"),
        ("none", "kge_2012"),
        ("power(0.2)", "nse"),
        ("power(0.2)", "kge_2012"),
    ]

    # Each (transform, metric) column equals the single-transform call.
    for transform in (None, "power(0.2)"):
        single = hb.evaluate_periods(
            model, obs, periods, metrics=("nse", "kge_2012"), transform=transform
        )
        label = "none" if transform is None else transform
        assert np.allclose(scores[label].to_numpy(), single.to_numpy())


def test_evaluate_periods_duplicate_transforms_raise():
    periods = Periods(calibration=("2020-01-01", "2020-12-31"), spinup=0)
    time = pd.date_range("2020-01-01", "2020-12-31", freq="D")
    sim = np.ones(len(time))
    model = _StubModel(time, sim)
    with pytest.raises(ConfigurationError):
        hb.evaluate_periods(
            model, sim, periods, metrics=("nse",), transform=[None, "identity"]
        )


def test_evaluate_periods_period_not_covered_raises():
    periods = Periods(calibration=("2019-01-01", "2020-12-31"), spinup=0)
    time = pd.date_range("2020-01-01", "2020-12-31", freq="D")
    sim = np.ones(len(time))
    model = _StubModel(time, sim)
    with pytest.raises(ConfigurationError):
        hb.evaluate_periods(model, sim, periods, metrics=("nse",))


def test_evaluate_periods_length_mismatch_raises():
    periods = Periods(calibration=("2020-01-01", "2020-12-31"), spinup=0)
    time = pd.date_range("2020-01-01", "2020-12-31", freq="D")
    model = _StubModel(time, np.ones(len(time)))
    with pytest.raises(DataError):
        hb.evaluate_periods(model, np.ones(10), periods, metrics=("nse",))


# ---------------------------------------------------------------------------
# DischargeObservations with periods
# ---------------------------------------------------------------------------
def test_discharge_observations_accepts_period_and_periods():
    periods = Periods(
        calibration=("1981-01-01", "2000-12-31"),
        validation=("2001-01-01", "2020-12-31"),
    )
    obs = hb.DischargeObservations(periods)
    assert obs.start_date == pd.Timestamp("1981-01-01")
    assert obs.end_date == pd.Timestamp("2020-12-31")

    obs = hb.DischargeObservations(periods.calibration)
    assert obs.end_date == pd.Timestamp("2000-12-31")

    with pytest.raises(DataError):
        hb.DischargeObservations("1981-01-01")
