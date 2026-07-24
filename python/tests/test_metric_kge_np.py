"""Tests for the non-parametric KGE metric (``kge_np``) exposed via ``evaluate``."""

import numpy as np
import pytest

from hydrobricks.evaluation.metrics import evaluate, is_error_metric

pytest.importorskip("HydroErr")
spotpy = pytest.importorskip("spotpy")


_OBS = np.array([1.0, 2.0, 3.0, 4.0, 5.0, 4.0, 3.0, 2.0, 1.0, 2.0, 3.0])
_SIM = _OBS * 1.1 + 0.05


def test_kge_np_matches_spotpy_with_correct_argument_order():
    # SPOTPY expects (evaluation, simulation) == (observation, simulation), the reverse
    # of the HydroErr (simulation, observation) convention used by evaluate().
    expected = spotpy.objectivefunctions.kge_non_parametric(_OBS, _SIM)
    assert evaluate(_SIM, _OBS, "kge_np") == pytest.approx(expected)


def test_kge_np_perfect_simulation_is_one():
    assert evaluate(_OBS, _OBS, "kge_np") == pytest.approx(1.0)


def test_kge_non_parametric_alias_matches_kge_np():
    assert evaluate(_SIM, _OBS, "kge_non_parametric") == pytest.approx(
        evaluate(_SIM, _OBS, "kge_np")
    )


def test_kge_np_name_is_case_insensitive():
    assert evaluate(_SIM, _OBS, "KGE_NP") == pytest.approx(
        evaluate(_SIM, _OBS, "kge_np")
    )


def test_kge_np_is_not_symmetric_in_its_arguments():
    # Guards against an accidental argument-order swap: KGE's beta/alpha terms make it
    # non-symmetric, so evaluating with sim and obs swapped must give a different score.
    assert evaluate(_SIM, _OBS, "kge_np") != pytest.approx(
        evaluate(_OBS, _SIM, "kge_np")
    )


def test_kge_np_is_a_skill_metric():
    assert is_error_metric("kge_np") is False
    assert is_error_metric("kge_non_parametric") is False


def test_kge_np_raises_clear_error_without_spotpy(monkeypatch):
    import hydrobricks._optional as optional

    monkeypatch.setattr(optional, "HAS_SPOTPY", False)
    with pytest.raises(ImportError, match="spotpy"):
        evaluate(_SIM, _OBS, "kge_np")
