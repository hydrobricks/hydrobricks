"""Tests for the benchmark-skill normalization of combined objectives."""

import types

import numpy as np
import pytest

import hydrobricks.trainer as trainer
from hydrobricks.evaluation.metrics import evaluate, reference_value, to_skill

pytest.importorskip("HydroErr")


# --------------------------------------------------------------------------- #
# to_skill
# --------------------------------------------------------------------------- #
def test_to_skill_error_metric_is_benchmark_relative():
    obs = np.array([0.0, 1.0, 2.0, 3.0, 4.0])
    # A perfect fit (error 0) is a skill of 1.
    assert to_skill(0.0, "rmse", obs) == pytest.approx(1.0)
    # The mean predictor's error is the reference -> skill 0.
    ref = reference_value(obs, "rmse")
    assert to_skill(ref, "rmse", obs) == pytest.approx(0.0)
    # Worse than the mean predictor -> negative skill.
    assert to_skill(2 * ref, "rmse", obs) < 0


def test_to_skill_passthrough_for_skill_metric():
    obs = np.array([1.0, 2.0, 3.0])
    assert to_skill(0.7, "nse", obs) == 0.7
    assert to_skill(0.7, "kge_2012", obs) == 0.7


def test_to_skill_constant_obs_falls_back_to_negated_error():
    obs = np.array([5.0, 5.0, 5.0])  # std 0 -> degenerate benchmark
    assert to_skill(0.3, "rmse", obs) == pytest.approx(-0.3)


# --------------------------------------------------------------------------- #
# Weighted combination in the trainer
# --------------------------------------------------------------------------- #
def _bare_setup(normalize, obj_func="kge_2012", weight=2.0, discharge_weight=1.0):
    """A minimal SpotpySetup with only the attributes the objective needs."""
    s = trainer.SpotpySetup.__new__(trainer.SpotpySetup)
    s.obj_func = obj_func
    s.transform = trainer.DischargeTransform.from_spec(None)
    s.combine = "weighted"
    s.discharge_weight = discharge_weight
    s.normalize = normalize
    s._minimize = False
    s.extra_observations = [
        types.SimpleNamespace(mode="objective", metric="rmse", weight=weight)
    ]
    return s


_Q_OBS = np.array([1.0, 2.0, 3.0, 4.0, 5.0, 6.0])
_Q_SIM = _Q_OBS + 0.3
_A_OBS = np.array([0.1, 0.4, 0.2, 0.8, 0.5, 0.3])
_A_SIM = _A_OBS + 0.05
_COMBINED = [np.concatenate([_Q_SIM, _A_SIM])]
_OBS_COMBINED = [np.concatenate([_Q_OBS, _A_OBS])]


def test_weighted_objective_normalizes_auxiliary_term():
    q_kge = evaluate(_Q_SIM, _Q_OBS, "kge_2012")  # skill metric -> unchanged
    a_rmse = evaluate(_A_SIM, _A_OBS, "rmse")
    a_skill = 1 - a_rmse / reference_value(_A_OBS, "rmse")

    s = _bare_setup(True)
    s._extra_lengths = [len(_A_OBS)]
    score = s._objective_with_extra_observations(_COMBINED, _OBS_COMBINED, None)
    assert score == pytest.approx(1.0 * q_kge + 2.0 * a_skill)

    # The normalized auxiliary term is on a comparable (<= 1) scale, unlike -rmse.
    assert a_skill <= 1.0


def test_normalize_false_reproduces_raw_combination():
    q_kge = evaluate(_Q_SIM, _Q_OBS, "kge_2012")
    a_rmse = evaluate(_A_SIM, _A_OBS, "rmse")

    s = _bare_setup(False)
    s._extra_lengths = [len(_A_OBS)]
    score = s._objective_with_extra_observations(_COMBINED, _OBS_COMBINED, None)
    # Old behavior: discharge skill plus the weighted negated error.
    assert score == pytest.approx(1.0 * q_kge + 2.0 * (-a_rmse))


def test_weighted_objective_normalizes_discharge_error_metric():
    s = _bare_setup(True, obj_func="rmse")
    s._extra_lengths = [len(_A_OBS)]
    score = s._objective_with_extra_observations(_COMBINED, _OBS_COMBINED, None)

    q_skill = 1 - evaluate(_Q_SIM, _Q_OBS, "rmse") / reference_value(_Q_OBS, "rmse")
    a_skill = 1 - evaluate(_A_SIM, _A_OBS, "rmse") / reference_value(_A_OBS, "rmse")
    assert score == pytest.approx(1.0 * q_skill + 2.0 * a_skill)
