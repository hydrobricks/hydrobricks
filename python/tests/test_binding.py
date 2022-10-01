import hydrobricks as hb

no_log = hb.LogNull()

def test_parameter_creation():
    param = hb.Parameter("my param", 0)
    assert param.name == "my param"
    assert param.value == 0

def test_parameter_rename_with_property():
    param = hb.Parameter("my param", 0)
    param.name = "new name"
    assert param.name == "new name"

def test_parameter_rename_with_function():
    param = hb.Parameter("my param", 0)
    param.set_name("new name")
    assert param.get_name() == "new name"

def test_parameter_change_value_with_property():
    param = hb.Parameter("my param", 0)
    param.value = 2
    assert param.value == 2

def test_parameter_change_value_with_function():
    param = hb.Parameter("my param", 0)
    param.set_value(2)
    assert param.get_value() == 2

def test_parameter_variable_yearly_creation():
    param = hb.ParameterVariableYearly("param yearly variable")
    assert param.name == "param yearly variable"

def test_parameter_variable_yearly_value_assignment():
    param = hb.ParameterVariableYearly("param yearly variable")
    assert param.set_values(year_start=2020, year_end=2025, values=[1.0, 1.1, 1.2, 1.3, 1.4, 1.5])

def test_parameter_variable_yearly_value_assignment_fails_if_wrong_size():
    param = hb.ParameterVariableYearly("param yearly variable")
    assert not param.set_values(year_start=2020, year_end=2025, values=[1.0, 1.1, 1.2, 1.3, 1.4])

