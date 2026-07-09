"""Tests for the discharge transformations (DischargeTransform) and their wiring."""

import numpy as np
import pytest

from hydrobricks._exceptions import ConfigurationError, DataError
from hydrobricks.evaluation.transforms import DischargeTransform

_OBS = np.array([0.0, 0.5, 1.0, 2.0, 4.0, 8.0, 4.0, 2.0, 1.0, 0.5])
_SIM = np.array([0.1, 0.4, 1.2, 1.8, 4.5, 7.0, 4.2, 2.2, 0.9, 0.6])


# --------------------------------------------------------------------------- #
# The transforms themselves
# --------------------------------------------------------------------------- #
def test_identity_is_a_no_op():
    tf = DischargeTransform("identity")
    sim, obs = tf.transform_pair(_SIM, _OBS)
    np.testing.assert_array_equal(sim, _SIM)
    np.testing.assert_array_equal(obs, _OBS)
    assert tf.is_identity


def test_power_transform_matches_manual_computation():
    tf = DischargeTransform("power", exponent=0.2)
    sim, obs = tf.transform_pair(_SIM, _OBS)
    np.testing.assert_allclose(sim, np.power(_SIM, 0.2))
    np.testing.assert_allclose(obs, np.power(_OBS, 0.2))


def test_sqrt_is_power_half():
    sim_sqrt, _ = DischargeTransform("sqrt").transform_pair(_SIM, _OBS)
    sim_pow, _ = DischargeTransform("power", exponent=0.5).transform_pair(_SIM, _OBS)
    np.testing.assert_allclose(sim_sqrt, sim_pow)


def test_power_requires_an_exponent():
    with pytest.raises(ConfigurationError, match="exponent"):
        DischargeTransform("power")


def test_log_with_explicit_epsilon():
    tf = DischargeTransform("log", epsilon=0.01)
    sim, obs = tf.transform_pair(_SIM, _OBS)
    np.testing.assert_allclose(obs, np.log(_OBS + 0.01))
    np.testing.assert_allclose(sim, np.log(_SIM + 0.01))


def test_inverse_with_explicit_epsilon():
    tf = DischargeTransform("inverse", epsilon=0.1)
    _, obs = tf.transform_pair(_SIM, _OBS)
    np.testing.assert_allclose(obs, 1.0 / (_OBS + 0.1))


def test_auto_epsilon_is_shared_and_from_the_observations():
    # log/inverse default to epsilon='auto' = mean(obs) / 100 (Pushpalatha 2012),
    # resolved from the observations and applied to BOTH series.
    tf = DischargeTransform("log")
    assert tf.epsilon == "auto"
    eps = float(np.mean(_OBS)) / 100.0
    sim, obs = tf.transform_pair(_SIM, _OBS)
    np.testing.assert_allclose(sim, np.log(_SIM + eps))
    np.testing.assert_allclose(obs, np.log(_OBS + eps))
    assert np.all(np.isfinite(obs))  # zero flows stay finite


def test_auto_epsilon_single_series_requires_a_reference():
    tf = DischargeTransform("log")
    with pytest.raises(ConfigurationError, match="auto"):
        tf(_SIM)
    # With the reference it matches the pair application.
    np.testing.assert_allclose(tf(_SIM, _OBS), tf.transform_pair(_SIM, _OBS)[0])


def test_auto_epsilon_rejects_all_zero_observations():
    with pytest.raises(DataError, match="mean observed flow"):
        DischargeTransform("log").transform_pair(_SIM, np.zeros_like(_OBS))


def test_negative_flows_are_clipped():
    tf = DischargeTransform("power", exponent=0.2)
    out = tf(np.array([-1.0, 0.0, 1.0]))
    np.testing.assert_allclose(out, [0.0, 0.0, 1.0])
    assert np.all(np.isfinite(out))


def test_negative_epsilon_and_unknown_kind_are_rejected():
    with pytest.raises(ConfigurationError, match="non-negative"):
        DischargeTransform("log", epsilon=-0.1)
    with pytest.raises(ConfigurationError, match="Unknown discharge transform"):
        DischargeTransform("boxcox")


# --------------------------------------------------------------------------- #
# from_spec coercion
# --------------------------------------------------------------------------- #
def test_from_spec_none_is_identity():
    assert DischargeTransform.from_spec(None).is_identity


def test_from_spec_passes_instances_through():
    tf = DischargeTransform("sqrt")
    assert DischargeTransform.from_spec(tf) is tf


def test_from_spec_parses_strings():
    tf = DischargeTransform.from_spec("power(0.2)")
    assert tf.kind == "power" and tf.exponent == pytest.approx(0.2)
    assert DischargeTransform.from_spec("none").is_identity
    assert DischargeTransform.from_spec("sqrt").exponent == pytest.approx(0.5)
    tf = DischargeTransform.from_spec("log(0.01)")
    assert tf.kind == "log" and tf.epsilon == pytest.approx(0.01)
    with pytest.raises(ConfigurationError, match="Cannot parse"):
        DischargeTransform.from_spec("power(0.2")


def test_from_spec_parses_dicts():
    tf = DischargeTransform.from_spec({"type": "power", "exponent": 0.2})
    assert tf.kind == "power" and tf.exponent == pytest.approx(0.2)
    assert DischargeTransform.from_spec({"type": "identity"}).is_identity
    # 'kind' is accepted for 'type'; unknown keys are rejected.
    assert DischargeTransform.from_spec({"kind": "sqrt"}).exponent == pytest.approx(0.5)
    with pytest.raises(ConfigurationError, match="Unknown discharge transform option"):
        DischargeTransform.from_spec({"type": "log", "exp": 2})


def test_from_spec_wraps_callables():
    tf = DischargeTransform.from_spec(np.sqrt)
    sim, obs = tf.transform_pair(_SIM, _OBS)
    np.testing.assert_allclose(sim, np.sqrt(_SIM))
    np.testing.assert_allclose(obs, np.sqrt(_OBS))
    assert not tf.is_identity


def test_transform_is_picklable():
    import pickle

    tf = pickle.loads(pickle.dumps(DischargeTransform("power", exponent=0.2)))
    np.testing.assert_allclose(tf(_SIM), np.power(_SIM, 0.2))


# --------------------------------------------------------------------------- #
# Wiring: evaluate() and the trainer's discharge skill
# --------------------------------------------------------------------------- #
def test_evaluate_applies_the_transform():
    pytest.importorskip("HydroErr")
    from hydrobricks.evaluation.metrics import evaluate

    tf = DischargeTransform("power", exponent=0.2)
    sim_t, obs_t = tf.transform_pair(_SIM, _OBS)
    expected = evaluate(sim_t, obs_t, "kge_2012")
    assert evaluate(_SIM, _OBS, "kge_2012", transform=tf) == pytest.approx(expected)
    assert evaluate(_SIM, _OBS, "kge_2012", transform="power(0.2)") == pytest.approx(
        expected
    )
    # And it actually changes the score relative to the raw series.
    assert expected != pytest.approx(evaluate(_SIM, _OBS, "kge_2012"))


def test_discharge_skill_scores_in_transformed_space():
    pytest.importorskip("HydroErr")
    import hydrobricks.trainer as trainer
    from hydrobricks.evaluation.metrics import evaluate

    s = trainer.SpotpySetup.__new__(trainer.SpotpySetup)
    s.obj_func = "kge_2012"
    s.transform = DischargeTransform.from_spec("power(0.2)")
    sim_t, obs_t = s.transform.transform_pair(_SIM, _OBS)
    assert s._discharge_skill(_SIM, _OBS) == pytest.approx(
        evaluate(sim_t, obs_t, "kge_2012")
    )


def test_discharge_skill_transform_feeds_custom_callable():
    import hydrobricks.trainer as trainer

    received = {}

    def obj(obs, sim):
        received["obs"], received["sim"] = obs, sim
        return 1.0

    s = trainer.SpotpySetup.__new__(trainer.SpotpySetup)
    s.obj_func = obj
    s.transform = DischargeTransform.from_spec("sqrt")
    s._discharge_skill(_SIM, _OBS)
    np.testing.assert_allclose(received["sim"], np.sqrt(_SIM))
    np.testing.assert_allclose(received["obs"], np.sqrt(_OBS))
