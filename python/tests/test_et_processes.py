import pytest

import hydrobricks as hb


def _register_et(kind: str) -> hb.ParameterSet:
    """Register the parameters of a soil brick carrying the given ET process."""
    parameter_set = hb.ParameterSet()
    brick = {"processes": {"et": {"kind": kind}}}
    parameter_set._generate_process_parameters("soil", brick)
    return parameter_set


def test_et_linear_is_recognised_and_parameter_free():
    parameter_set = _register_et("et:linear")
    # Parameter-free reduction: nothing should be registered, but it must not raise.
    assert "soil" not in parameter_set.parameters["component"].tolist()


def test_et_power_law_registers_exponent():
    parameter_set = _register_et("et:power_law")
    names = parameter_set.parameters["name"].tolist()
    assert "exponent" in names


def test_et_power_law_exponent_alias_does_not_clash_with_socont_beta():
    # runoff:socont already uses the 'beta' alias; the power-law exponent must use a
    # distinct alias so both can coexist in a single Socont-style parameter set.
    parameter_set = hb.ParameterSet()
    parameter_set._generate_process_parameters(
        "soil", {"processes": {"et": {"kind": "et:power_law"}}}
    )
    parameter_set._generate_process_parameters(
        "surface_runoff", {"processes": {"runoff": {"kind": "runoff:socont"}}}
    )
    aliases = parameter_set.parameters["aliases"].explode().dropna().tolist()
    assert "beta" in aliases
    assert "et_beta" in aliases


def test_et_exponential_registers_alpha():
    parameter_set = _register_et("et:exponential")
    names = parameter_set.parameters["name"].tolist()
    assert "alpha" in names


def test_unknown_et_process_raises():
    with pytest.raises(hb.ConfigurationError):
        _register_et("et:does_not_exist")
