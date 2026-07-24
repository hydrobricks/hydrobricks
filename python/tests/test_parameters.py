import math
import os.path
import tempfile
from pathlib import Path

import pytest

import hydrobricks as hb
from hydrobricks.parameters import ParamSpec, validate_process_param_specs


def test_parameter_creation():
    hb.ParameterSet()


def test_define_parameter():
    parameter_set = hb.ParameterSet()
    parameter_set.define_parameter(
        component="snowpack",
        name="degree_day_factor",
        unit="mm/d",
        aliases=["a_snow", "sdd"],
        min_val=0,
        max_val=10,
        mandatory=False,
    )
    assert parameter_set.parameters.loc[0].at["component"] == "snowpack"
    assert parameter_set.parameters.loc[0].at["name"] == "degree_day_factor"
    assert parameter_set.parameters.loc[0].at["unit"] == "mm/d"
    assert parameter_set.parameters.loc[0].at["aliases"] == ["a_snow", "sdd"]
    assert parameter_set.parameters.loc[0].at["min"] == 0
    assert parameter_set.parameters.loc[0].at["max"] == 10
    assert not parameter_set.parameters.loc[0].at["mandatory"]
    assert parameter_set.parameters.loc[0].at["value"] is None


def test_define_parameter_min_max_mismatch():
    parameter_set = hb.ParameterSet()
    with pytest.raises(hb.ConfigurationError):
        parameter_set.define_parameter(
            component="snowpack",
            name="degree_day_factor",
            unit="mm/d",
            aliases=["a_snow", "sdd"],
            min_val=3,
            max_val=2,
        )


def test_define_parameter_with_list():
    parameter_set = hb.ParameterSet()
    parameter_set.define_parameter(
        component="snowpack",
        name="degree_day_factor",
        unit="mm/d",
        aliases=["a_snow", "sdd"],
        min_val=[0, 1, 2, 3],
        max_val=[10, 11, 12, 13],
    )
    assert parameter_set.parameters.loc[0].at["min"] == [0, 1, 2, 3]
    assert parameter_set.parameters.loc[0].at["max"] == [10, 11, 12, 13]


def test_define_parameter_with_list_after_float():
    parameter_set = hb.ParameterSet()
    parameter_set.define_parameter(
        component="glacier", name="degree_day_factor", min_val=0, max_val=8
    )
    parameter_set.define_parameter(
        component="snowpack",
        name="degree_day_factor",
        min_val=[0, 1, 2, 3],
        max_val=[10, 11, 12, 13],
    )
    assert parameter_set.parameters.loc[1].at["min"] == [0, 1, 2, 3]
    assert parameter_set.parameters.loc[1].at["max"] == [10, 11, 12, 13]


def test_define_parameter_min_max_mismatch_types():
    parameter_set = hb.ParameterSet()
    with pytest.raises(hb.ConfigurationError):
        parameter_set.define_parameter(
            component="snowpack",
            name="degree_day_factor",
            unit="mm/d",
            aliases=["a_snow", "sdd"],
            min_val=0,
            max_val=[1, 2],
        )
    with pytest.raises(hb.ConfigurationError):
        parameter_set.define_parameter(
            component="snowpack",
            name="degree_day_factor",
            unit="mm/d",
            aliases=["a_snow", "sdd"],
            min_val=[1, 2],
            max_val=3,
        )


def test_define_parameter_min_max_mismatch_list_size():
    parameter_set = hb.ParameterSet()
    with pytest.raises(hb.ConfigurationError):
        parameter_set.define_parameter(
            component="snowpack",
            name="degree_day_factor",
            unit="mm/d",
            aliases=["a_snow", "sdd"],
            min_val=[0, 0],
            max_val=[1, 2, 3],
        )


def test_define_parameter_min_max_mismatch_in_list():
    parameter_set = hb.ParameterSet()
    with pytest.raises(hb.ConfigurationError):
        parameter_set.define_parameter(
            component="snowpack",
            name="degree_day_factor",
            unit="mm/d",
            aliases=["a_snow", "sdd"],
            min_val=[3, 4],
            max_val=[1, 2],
        )


def test_define_parameter_nb_rows_is_correct():
    parameter_set = hb.ParameterSet()
    parameter_set.define_parameter(
        component="snowpack",
        name="degree_day_factor",
        unit="mm/d",
        aliases=["x", "y"],
        default=3,
        min_val=0,
        max_val=10,
        mandatory=True,
    )
    parameter_set.define_parameter(
        component="snowpack",
        name="melting_temperature",
        unit="°C",
        aliases=["z"],
        default=0,
        min_val=0,
        max_val=5,
        mandatory=False,
    )
    assert len(parameter_set.parameters) == 2
    assert parameter_set.parameters.loc[0].at["aliases"] == ["x", "y"]
    assert parameter_set.parameters.loc[1].at["aliases"] == ["z"]


def test_define_parameter_use_default():
    parameter_set = hb.ParameterSet()
    parameter_set.define_parameter(
        component="snowpack",
        name="degree_day_factor",
        unit="mm/d",
        default=3,
        min_val=0,
        max_val=10,
        mandatory=False,
    )
    assert parameter_set.parameters.loc[0].at["value"] == 3


def test_define_parameter_use_default_with_list():
    parameter_set = hb.ParameterSet()
    parameter_set.define_parameter(
        component="snowpack",
        name="degree_day_factor",
        unit="mm/d",
        default=[3, 4, 5],
        mandatory=False,
    )
    assert parameter_set.parameters.loc[0].at["value"] == [3, 4, 5]


def test_define_parameter_not_using_default_if_mandatory():
    parameter_set = hb.ParameterSet()
    parameter_set.define_parameter(
        component="snowpack",
        name="degree_day_factor",
        unit="mm/d",
        default=3,
        min_val=0,
        max_val=10,
        mandatory=True,
    )
    assert parameter_set.parameters.loc[0].at["value"] is None


def test_define_parameter_alias_already_used():
    parameter_set = hb.ParameterSet()
    parameter_set.define_parameter(
        component="snowpack",
        name="degree_day_factor",
        unit="mm/d",
        aliases=["as", "sd"],
    )
    with pytest.raises(hb.ConfigurationError):
        parameter_set.define_parameter(
            component="glacier",
            name="degree_day_factor",
            unit="mm/d",
            aliases=["as", "gdd"],
        )


def test_set_parameter_value_by_alias():
    parameter_set = hb.ParameterSet()
    parameter_set.define_parameter(
        component="snowpack",
        name="degree_day_factor",
        unit="mm/d",
        aliases=["as", "sd"],
    )
    parameter_set.define_parameter(
        component="glacier", name="degree_day_factor", unit="mm/d", aliases=["ag", "gd"]
    )
    parameter_set.set_values({"as": 2, "gd": 3})
    assert parameter_set.get("as") == 2
    assert parameter_set.get("ag") == 3


def test_set_parameter_value_as_list():
    parameter_set = hb.ParameterSet()
    parameter_set.define_parameter(
        component="snowpack",
        name="degree_day_factor",
        unit="mm/d",
        aliases=["as", "sd"],
    )
    parameter_set.define_parameter(
        component="glacier", name="degree_day_factor", unit="mm/d", aliases=["ag", "gd"]
    )
    parameter_set.set_values({"as": [2, 3], "gd": [3, 4]})
    assert parameter_set.get("as") == [2, 3]
    assert parameter_set.get("ag") == [3, 4]


def test_set_parameter_value_by_name():
    parameter_set = hb.ParameterSet()
    parameter_set.define_parameter(
        component="snowpack", name="degree_day_factor", unit="mm/d", aliases=["as"]
    )
    parameter_set.define_parameter(
        component="glacier", name="degree_day_factor", unit="mm/d", aliases=["ag"]
    )
    parameter_set.set_values(
        {"snowpack:degree_day_factor": 2, "glacier:degree_day_factor": 3}
    )
    assert parameter_set.get("as") == 2
    assert parameter_set.get("ag") == 3


def test_set_parameter_value_not_found():
    parameter_set = hb.ParameterSet()
    parameter_set.define_parameter(
        component="snowpack", name="degree_day_factor", unit="mm/d", aliases=["as"]
    )
    with pytest.raises(hb.ConfigurationError):
        parameter_set.set_values({"xy": 2})


def test_set_parameter_value_too_low():
    parameter_set = hb.ParameterSet()
    parameter_set.define_parameter(
        component="snowpack",
        name="degree_day_factor",
        unit="mm/d",
        aliases=["as"],
        min_val=2,
        max_val=6,
    )
    with pytest.raises(hb.ConfigurationError):
        parameter_set.set_values({"as": 1})


def test_set_parameter_value_too_high():
    parameter_set = hb.ParameterSet()
    parameter_set.define_parameter(
        component="snowpack",
        name="degree_day_factor",
        unit="mm/d",
        aliases=["as"],
        min_val=2,
        max_val=6,
    )
    with pytest.raises(hb.ConfigurationError):
        parameter_set.set_values({"as": 10})


def test_set_parameter_value_with_list_too_low():
    parameter_set = hb.ParameterSet()
    parameter_set.define_parameter(
        component="snowpack",
        name="degree_day_factor",
        unit="mm/d",
        aliases=["as"],
        min_val=[0, 1],
        max_val=[2, 3],
    )
    with pytest.raises(hb.ConfigurationError):
        parameter_set.set_values({"as": [1, 0]})


def test_set_parameter_value_with_list_too_high():
    parameter_set = hb.ParameterSet()
    parameter_set.define_parameter(
        component="snowpack",
        name="degree_day_factor",
        unit="mm/d",
        aliases=["as"],
        min_val=[0, 1],
        max_val=[2, 3],
    )
    with pytest.raises(hb.ConfigurationError):
        parameter_set.set_values({"as": [1, 5]})


@pytest.fixture
def parameter_set():
    parameter_set = hb.ParameterSet()
    parameter_set.define_parameter(
        component="snowpack",
        name="degree_day_factor",
        unit="mm/d",
        aliases=["a_snow", "sdd"],
        min_val=0,
        max_val=10,
        mandatory=True,
    )
    parameter_set.define_parameter(
        component="snowpack",
        name="melting_temperature",
        unit="°C",
        min_val=0,
        max_val=5,
        default=0,
        mandatory=False,
    )
    parameter_set.define_parameter(
        component="slow_reservoir",
        name="capacity",
        unit="mm",
        aliases=["A"],
        min_val=0,
        max_val=3000,
        mandatory=True,
    )
    parameter_set.set_values({"a_snow": 3, "A": 200})
    return parameter_set


def test_create_json_parameter_file_created(parameter_set: hb.ParameterSet):
    with tempfile.TemporaryDirectory() as tmp_dir:
        parameter_set.save_as(tmp_dir, "parameters", "json")
        assert os.path.isfile(Path(tmp_dir) / "parameters.json")


def test_create_json_parameter_file_content(parameter_set: hb.ParameterSet):
    with tempfile.TemporaryDirectory() as tmp_dir:
        parameter_set.save_as(tmp_dir, "parameters", "json")
        txt = Path(tmp_dir + "/parameters.json").read_text()
        assert '"degree_day_factor": 3' in txt
        assert '"capacity": 200' in txt


def test_create_yaml_parameter_file_created(parameter_set: hb.ParameterSet):
    with tempfile.TemporaryDirectory() as tmp_dir:
        parameter_set.save_as(tmp_dir, "parameters", "yaml")
        assert os.path.isfile(Path(tmp_dir) / "parameters.yaml")


def test_create_yaml_parameter_file_content(parameter_set: hb.ParameterSet):
    with tempfile.TemporaryDirectory() as tmp_dir:
        parameter_set.save_as(tmp_dir, "parameters", "yaml")
        txt = Path(tmp_dir + "/parameters.yaml").read_text()
        assert "degree_day_factor: 3" in txt
        assert "capacity: 200" in txt


def test_set_random_values(parameter_set: hb.ParameterSet):
    parameter_set = hb.ParameterSet()
    parameter_set.define_parameter(
        component="snowpack", name="melt_factor", aliases=["dd"], min_val=0, max_val=10
    )
    parameter_set.define_parameter(
        component="snowpack", name="melting_temp", aliases=["mt"], min_val=0, max_val=5
    )
    parameter_set.define_parameter(
        component="reservoir", name="capacity", aliases=["A"], min_val=0, max_val=3000
    )
    parameter_set.set_random_values(["dd", "mt", "A"])
    assert parameter_set.get("dd") >= 0
    assert parameter_set.get("dd") <= 10
    assert parameter_set.get("mt") >= 0
    assert parameter_set.get("mt") <= 5
    assert parameter_set.get("A") >= 0
    assert parameter_set.get("A") <= 3000


def test_set_random_values_with_lists(parameter_set: hb.ParameterSet):
    parameter_set = hb.ParameterSet()
    parameter_set.define_parameter(
        component="snowpack",
        name="melt_factor",
        aliases=["dd"],
        min_val=[0, 1],
        max_val=[2, 3],
    )
    parameter_set.define_parameter(
        component="snowpack",
        name="melting_temp",
        aliases=["mt"],
        min_val=[1, 2],
        max_val=[3, 4],
    )
    parameter_set.define_parameter(
        component="reservoir",
        name="capacity",
        aliases=["A"],
        min_val=[0, 10],
        max_val=[10, 200],
    )
    parameter_set.set_random_values(["dd", "mt", "A"])
    assert parameter_set.get("dd")[0] >= 0
    assert parameter_set.get("dd")[0] <= 2
    assert parameter_set.get("dd")[1] >= 1
    assert parameter_set.get("dd")[1] <= 3
    assert parameter_set.get("mt")[0] >= 1
    assert parameter_set.get("mt")[0] <= 3
    assert parameter_set.get("mt")[1] >= 2
    assert parameter_set.get("mt")[1] <= 4
    assert parameter_set.get("A")[0] >= 0
    assert parameter_set.get("A")[0] <= 10
    assert parameter_set.get("A")[1] >= 10
    assert parameter_set.get("A")[1] <= 200


def test_reset_random_values_with_lists(parameter_set: hb.ParameterSet):
    parameter_set = hb.ParameterSet()
    parameter_set.define_parameter(
        component="snowpack",
        name="melt_factor",
        aliases=["dd"],
        min_val=[0, 1],
        max_val=[2, 3],
    )
    parameter_set.set_random_values(["dd"])
    parameter_set.set_random_values(["dd"])
    assert parameter_set.get("dd")[0] >= 0
    assert parameter_set.get("dd")[0] <= 2
    assert parameter_set.get("dd")[1] >= 1
    assert parameter_set.get("dd")[1] <= 3


def test_get_model_only_parameters(parameter_set: hb.ParameterSet):
    parameter_set.add_data_parameter("lapse", 0.6, min_val=0, max_val=1)
    model_params = parameter_set.get_model_parameters()
    assert len(model_params) == 3


def test_change_parameter_range(parameter_set: hb.ParameterSet):
    parameter_set.change_range("a_snow", 2, 8)
    assert parameter_set.parameters.loc[0].at["min"] == 2
    assert parameter_set.parameters.loc[0].at["max"] == 8


@pytest.fixture
def parameter_set_constraints():
    parameter_set = hb.ParameterSet()
    parameter_set.define_parameter(
        component="reservoir",
        name="coefficient_1",
        aliases=["k1"],
        min_val=0,
        max_val=1,
    )
    parameter_set.define_parameter(
        component="reservoir",
        name="coefficient_2",
        aliases=["k2"],
        min_val=0,
        max_val=1,
    )
    parameter_set.define_parameter(
        component="reservoir",
        name="coefficient_3",
        aliases=["k3"],
        min_val=0,
        max_val=1,
    )
    parameter_set.define_constraint("k1", "<", "k2")
    parameter_set.define_constraint("k2", "<", "k3")
    return parameter_set


def test_define_constraints_satisfied(parameter_set_constraints: hb.ParameterSet):
    assert len(parameter_set_constraints.constraints) == 2
    parameter_set_constraints.set_values({"k1": 0.2, "k2": 0.3, "k3": 0.4})
    assert parameter_set_constraints.constraints_satisfied()


def test_define_constraints_not_satisfied(parameter_set_constraints: hb.ParameterSet):
    assert len(parameter_set_constraints.constraints) == 2
    parameter_set_constraints.set_values({"k1": 0.2, "k2": 0.1, "k3": 0.4})
    assert not parameter_set_constraints.constraints_satisfied()


def test_define_constraints_removed(parameter_set_constraints: hb.ParameterSet):
    assert len(parameter_set_constraints.constraints) == 2
    parameter_set_constraints.remove_constraint("k1", "<", "k2")
    assert len(parameter_set_constraints.constraints) == 1


@pytest.fixture
def parameter_set_with_transform():
    parameter_set = hb.ParameterSet()
    parameter_set.define_parameter(
        component="production_store",
        name="capacity",
        unit="mm",
        aliases=["X1"],
        min_val=1,
        max_val=1000,
        mandatory=True,
    )
    parameter_set.set_transform("X1", math.log, math.exp)
    return parameter_set


def test_set_transform_real_to_transformed_round_trip(parameter_set_with_transform):
    ps = parameter_set_with_transform
    ps.set_values({"X1": 100})
    assert ps.get("X1") == 100
    assert ps.get_transformed("X1") == pytest.approx(math.log(100))


def test_set_transform_transformed_input_back_transformed(parameter_set_with_transform):
    ps = parameter_set_with_transform
    ps.set_values({"X1": math.log(100)}, transformed=True)
    assert ps.get("X1") == pytest.approx(100)
    assert ps.get_transformed("X1") == pytest.approx(math.log(100))


def test_set_transform_transformed_input_out_of_real_range(
    parameter_set_with_transform,
):
    ps = parameter_set_with_transform
    # exp(10) ~ 22026 > max (1000), so the back-transformed real value is out of range.
    with pytest.raises(hb.ConfigurationError):
        ps.set_values({"X1": 10}, transformed=True)


def test_set_values_transformed_without_transform_is_identity():
    ps = hb.ParameterSet()
    ps.define_parameter(
        component="snowpack",
        name="degree_day_factor",
        aliases=["a_snow"],
        min_val=0,
        max_val=10,
    )
    ps.set_values({"a_snow": 3}, transformed=True)
    assert ps.get("a_snow") == 3
    assert ps.get_transformed("a_snow") == 3


def test_get_for_spotpy_uses_transformed_bounds(parameter_set_with_transform):
    pytest.importorskip("spotpy")
    ps = parameter_set_with_transform
    ps.allow_changing = ["X1"]
    spotpy_params = ps.get_for_spotpy()
    assert len(spotpy_params) == 1
    param = spotpy_params[0]
    # spotpy adds a tiny step offset to minbound, so use an absolute tolerance.
    assert param.minbound == pytest.approx(math.log(1), abs=1e-3)
    assert param.maxbound == pytest.approx(math.log(1000), abs=1e-3)


def test_set_transform_on_list_parameter_raises():
    ps = hb.ParameterSet()
    ps.define_parameter(
        component="snowpack",
        name="degree_day_factor",
        aliases=["a_snow"],
        min_val=[0, 1],
        max_val=[2, 3],
    )
    with pytest.raises(hb.ConfigurationError):
        ps.set_transform("a_snow", math.log, math.exp)


def test_range_and_constraints_use_real_values_with_transform():
    ps = hb.ParameterSet()
    ps.define_parameter(
        component="reservoir", name="k1", aliases=["k1"], min_val=1, max_val=1000
    )
    ps.define_parameter(
        component="reservoir", name="k2", aliases=["k2"], min_val=1, max_val=1000
    )
    ps.set_transform("k1", math.log, math.exp)
    ps.define_constraint("k1", "<", "k2")
    ps.set_values({"k1": 10, "k2": 100})
    assert ps.range_satisfied()
    assert ps.constraints_satisfied()
    ps.set_values({"k1": 200, "k2": 100})
    assert not ps.constraints_satisfied()


def test_validate_process_param_specs():
    # Ensure current PROCESS_PARAM_SPECS structure is valid
    validate_process_param_specs()


def test_validate_process_param_specs_duplicate_detection():
    # Ensure duplicate parameter names within same process raise an error
    bad_specs = {
        "test:proc": [ParamSpec(name="dup"), ParamSpec(name="dup")]  # duplicate name
    }
    with pytest.raises(hb.ConfigurationError):
        validate_process_param_specs(bad_specs)


def test_get_for_spotpy_with_and_without_prior():
    # Parameters without a prior become Uniform; a parameter with a set prior keeps
    # that prior object (regression: the NaN check must not call np.isnan on it).
    spotpy = pytest.importorskip("spotpy")
    parameter_set = hb.ParameterSet()
    parameter_set.define_parameter(
        component="snowpack",
        name="degree_day_factor",
        unit="mm/d",
        aliases=["a_snow"],
        min_val=0,
        max_val=10,
    )
    parameter_set.define_parameter(
        component="surface_runoff",
        name="response_factor",
        unit="1/d",
        aliases=["k_quick"],
        min_val=0.05,
        max_val=0.5,
    )
    parameter_set.allow_changing = ["a_snow", "k_quick"]

    # Without any prior: all Uniform.
    spotpy_params = parameter_set.get_for_spotpy()
    assert all(isinstance(p, spotpy.parameter.Uniform) for p in spotpy_params)

    # With a prior on a_snow: that one keeps the supplied prior object.
    parameter_set.set_prior("a_snow", spotpy.parameter.Normal(mean=4, stddev=2))
    spotpy_params = parameter_set.get_for_spotpy()
    assert isinstance(spotpy_params[0], spotpy.parameter.Normal)
    assert isinstance(spotpy_params[1], spotpy.parameter.Uniform)
