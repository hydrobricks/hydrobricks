import _hydrobricks as _hb
import hydrobricks.models as models

no_log = _hb.LogNull()


def test_initialization():
    assert _hb.init()


def test_parameter_creation():
    param = _hb.Parameter("my param", 0)
    assert param.name == "my param"
    assert param.value == 0


def test_parameter_rename_with_property():
    param = _hb.Parameter("my param", 0)
    param.name = "new name"
    assert param.name == "new name"


def test_parameter_rename_with_function():
    param = _hb.Parameter("my param", 0)
    param.set_name("new name")
    assert param.get_name() == "new name"


def test_parameter_change_value_with_property():
    param = _hb.Parameter("my param", 0)
    param.value = 2
    assert param.value == 2


def test_parameter_change_value_with_function():
    param = _hb.Parameter("my param", 0)
    param.set_value(2)
    assert param.get_value() == 2


def test_parameter_variable_yearly_creation():
    param = _hb.ParameterVariableYearly("param yearly variable")
    assert param.name == "param yearly variable"


def test_parameter_variable_yearly_value_assignment():
    param = _hb.ParameterVariableYearly("param yearly variable")
    assert param.set_values(
        year_start=2020,
        year_end=2025,
        values=[1.0, 1.1, 1.2, 1.3, 1.4, 1.5]
    )


def test_parameter_variable_yearly_value_assignment_fails_if_wrong_size():
    param = _hb.ParameterVariableYearly("param yearly variable")
    assert not param.set_values(
        year_start=2020,
        year_end=2025,
        values=[1.0, 1.1, 1.2, 1.3, 1.4]
    )


def test_build_socont_model_structure():
    try:
        models.Socont(
            land_cover_types=['ground', 'glacier', 'glacier'],
            land_cover_names=['ground', 'glacier_ice', 'glacier_debris'],
            soil_storage_nb=2,
            surface_runoff='linear_storage'
        )
    except Exception as exc:
        AssertionError(f"Raised an exception {exc}")
