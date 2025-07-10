import os.path
import tempfile
from pathlib import Path

import pytest

import hydrobricks as hb


def test_parameter_creation():
    hb.ParameterSet()


def test_define_parameter():
    parameter_set = hb.ParameterSet()
    parameter_set.define_parameter(
        component='snowpack',
        name='degree_day_factor',
        unit='mm/d',
        aliases=['a_snow', 'sdd'],
        min_value=0,
        max_value=10,
        mandatory=False
    )
    assert parameter_set.parameters.loc[0].at['component'] == 'snowpack'
    assert parameter_set.parameters.loc[0].at['name'] == 'degree_day_factor'
    assert parameter_set.parameters.loc[0].at['unit'] == 'mm/d'
    assert parameter_set.parameters.loc[0].at['aliases'] == ['a_snow', 'sdd']
    assert parameter_set.parameters.loc[0].at['min'] == 0
    assert parameter_set.parameters.loc[0].at['max'] == 10
    assert not parameter_set.parameters.loc[0].at['mandatory']
    assert parameter_set.parameters.loc[0].at['value'] is None


def test_define_parameter_min_max_mismatch():
    parameter_set = hb.ParameterSet()
    with pytest.raises(ValueError):
        parameter_set.define_parameter(
            component='snowpack',
            name='degree_day_factor',
            unit='mm/d',
            aliases=['a_snow', 'sdd'],
            min_value=3,
            max_value=2
        )


def test_define_parameter_with_list():
    parameter_set = hb.ParameterSet()
    parameter_set.define_parameter(
        component='snowpack',
        name='degree_day_factor',
        unit='mm/d',
        aliases=['a_snow', 'sdd'],
        min_value=[0, 1, 2, 3],
        max_value=[10, 11, 12, 13]
    )
    assert parameter_set.parameters.loc[0].at['min'] == [0, 1, 2, 3]
    assert parameter_set.parameters.loc[0].at['max'] == [10, 11, 12, 13]


def test_define_parameter_with_list_after_float():
    parameter_set = hb.ParameterSet()
    parameter_set.define_parameter(
        component='glacier',
        name='degree_day_factor',
        min_value=0,
        max_value=8
    )
    parameter_set.define_parameter(
        component='snowpack',
        name='degree_day_factor',
        min_value=[0, 1, 2, 3],
        max_value=[10, 11, 12, 13]
    )
    assert parameter_set.parameters.loc[1].at['min'] == [0, 1, 2, 3]
    assert parameter_set.parameters.loc[1].at['max'] == [10, 11, 12, 13]


def test_define_parameter_min_max_mismatch_types():
    parameter_set = hb.ParameterSet()
    with pytest.raises(TypeError):
        parameter_set.define_parameter(
            component='snowpack',
            name='degree_day_factor',
            unit='mm/d',
            aliases=['a_snow', 'sdd'],
            min_value=0,
            max_value=[1, 2]
        )
    with pytest.raises(TypeError):
        parameter_set.define_parameter(
            component='snowpack',
            name='degree_day_factor',
            unit='mm/d',
            aliases=['a_snow', 'sdd'],
            min_value=[1, 2],
            max_value=3
        )


def test_define_parameter_min_max_mismatch_list_size():
    parameter_set = hb.ParameterSet()
    with pytest.raises(ValueError):
        parameter_set.define_parameter(
            component='snowpack',
            name='degree_day_factor',
            unit='mm/d',
            aliases=['a_snow', 'sdd'],
            min_value=[0, 0],
            max_value=[1, 2, 3]
        )


def test_define_parameter_min_max_mismatch_in_list():
    parameter_set = hb.ParameterSet()
    with pytest.raises(ValueError):
        parameter_set.define_parameter(
            component='snowpack',
            name='degree_day_factor',
            unit='mm/d',
            aliases=['a_snow', 'sdd'],
            min_value=[3, 4],
            max_value=[1, 2]
        )


def test_define_parameter_nb_rows_is_correct():
    parameter_set = hb.ParameterSet()
    parameter_set.define_parameter(
        component='snowpack',
        name='degree_day_factor',
        unit='mm/d',
        aliases=['x', 'y'],
        default_value=3,
        min_value=0,
        max_value=10,
        mandatory=True
    )
    parameter_set.define_parameter(
        component='snowpack',
        name='melting_temperature',
        unit='°C',
        aliases=['z'],
        default_value=0,
        min_value=0,
        max_value=5,
        mandatory=False
    )
    assert len(parameter_set.parameters) == 2
    assert parameter_set.parameters.loc[0].at['aliases'] == ['x', 'y']
    assert parameter_set.parameters.loc[1].at['aliases'] == ['z']


def test_define_parameter_use_default():
    parameter_set = hb.ParameterSet()
    parameter_set.define_parameter(
        component='snowpack',
        name='degree_day_factor',
        unit='mm/d',
        default_value=3,
        min_value=0,
        max_value=10,
        mandatory=False
    )
    assert parameter_set.parameters.loc[0].at['value'] == 3


def test_define_parameter_use_default_with_list():
    parameter_set = hb.ParameterSet()
    parameter_set.define_parameter(
        component='snowpack',
        name='degree_day_factor',
        unit='mm/d',
        default_value=[3, 4, 5],
        mandatory=False
    )
    assert parameter_set.parameters.loc[0].at['value'] == [3, 4, 5]


def test_define_parameter_not_using_default_if_mandatory():
    parameter_set = hb.ParameterSet()
    parameter_set.define_parameter(
        component='snowpack',
        name='degree_day_factor',
        unit='mm/d',
        default_value=3,
        min_value=0,
        max_value=10,
        mandatory=True
    )
    assert parameter_set.parameters.loc[0].at['value'] is None


def test_define_parameter_alias_already_used():
    parameter_set = hb.ParameterSet()
    parameter_set.define_parameter(
        component='snowpack',
        name='degree_day_factor',
        unit='mm/d',
        aliases=['as', 'sd']
    )
    with pytest.raises(ValueError):
        parameter_set.define_parameter(
            component='glacier',
            name='degree_day_factor',
            unit='mm/d',
            aliases=['as', 'gdd']
        )


def test_set_parameter_value_by_alias():
    parameter_set = hb.ParameterSet()
    parameter_set.define_parameter(
        component='snowpack',
        name='degree_day_factor',
        unit='mm/d',
        aliases=['as', 'sd']
    )
    parameter_set.define_parameter(
        component='glacier',
        name='degree_day_factor',
        unit='mm/d',
        aliases=['ag', 'gd']
    )
    parameter_set.set_values({'as': 2, 'gd': 3})
    assert parameter_set.get('as') == 2
    assert parameter_set.get('ag') == 3


def test_set_parameter_value_as_list():
    parameter_set = hb.ParameterSet()
    parameter_set.define_parameter(
        component='snowpack',
        name='degree_day_factor',
        unit='mm/d',
        aliases=['as', 'sd']
    )
    parameter_set.define_parameter(
        component='glacier',
        name='degree_day_factor',
        unit='mm/d',
        aliases=['ag', 'gd']
    )
    parameter_set.set_values({'as': [2, 3], 'gd': [3, 4]})
    assert parameter_set.get('as') == [2, 3]
    assert parameter_set.get('ag') == [3, 4]


def test_set_parameter_value_by_name():
    parameter_set = hb.ParameterSet()
    parameter_set.define_parameter(
        component='snowpack',
        name='degree_day_factor',
        unit='mm/d',
        aliases=['as']
    )
    parameter_set.define_parameter(
        component='glacier',
        name='degree_day_factor',
        unit='mm/d',
        aliases=['ag']
    )
    parameter_set.set_values({
        'snowpack:degree_day_factor': 2,
        'glacier:degree_day_factor': 3
    })
    assert parameter_set.get('as') == 2
    assert parameter_set.get('ag') == 3


def test_set_parameter_value_not_found():
    parameter_set = hb.ParameterSet()
    parameter_set.define_parameter(
        component='snowpack',
        name='degree_day_factor',
        unit='mm/d',
        aliases=['as']
    )
    with pytest.raises(ValueError):
        parameter_set.set_values({'xy': 2})


def test_set_parameter_value_too_low():
    parameter_set = hb.ParameterSet()
    parameter_set.define_parameter(
        component='snowpack',
        name='degree_day_factor',
        unit='mm/d',
        aliases=['as'],
        min_value=2,
        max_value=6
    )
    with pytest.raises(ValueError):
        parameter_set.set_values({'as': 1})


def test_set_parameter_value_too_high():
    parameter_set = hb.ParameterSet()
    parameter_set.define_parameter(
        component='snowpack',
        name='degree_day_factor',
        unit='mm/d',
        aliases=['as'],
        min_value=2,
        max_value=6
    )
    with pytest.raises(ValueError):
        parameter_set.set_values({'as': 10})


def test_set_parameter_value_with_list_too_low():
    parameter_set = hb.ParameterSet()
    parameter_set.define_parameter(
        component='snowpack',
        name='degree_day_factor',
        unit='mm/d',
        aliases=['as'],
        min_value=[0, 1],
        max_value=[2, 3]
    )
    with pytest.raises(ValueError):
        parameter_set.set_values({'as': [1, 0]})


def test_set_parameter_value_with_list_too_high():
    parameter_set = hb.ParameterSet()
    parameter_set.define_parameter(
        component='snowpack',
        name='degree_day_factor',
        unit='mm/d',
        aliases=['as'],
        min_value=[0, 1],
        max_value=[2, 3]
    )
    with pytest.raises(ValueError):
        parameter_set.set_values({'as': [1, 5]})


@pytest.fixture
def parameter_set():
    parameter_set = hb.ParameterSet()
    parameter_set.define_parameter(
        component='snowpack',
        name='degree_day_factor',
        unit='mm/d',
        aliases=['a_snow', 'sdd'],
        min_value=0,
        max_value=10,
        mandatory=True
    )
    parameter_set.define_parameter(
        component='snowpack',
        name='melting_temperature',
        unit='°C',
        min_value=0,
        max_value=5,
        default_value=0,
        mandatory=False
    )
    parameter_set.define_parameter(
        component='slow_reservoir',
        name='capacity',
        unit='mm',
        aliases=['A'],
        min_value=0,
        max_value=3000,
        mandatory=True
    )
    parameter_set.set_values({'a_snow': 3, 'A': 200})
    return parameter_set


def test_create_json_parameter_file_created(parameter_set: hb.ParameterSet):
    with tempfile.TemporaryDirectory() as tmp_dir:
        parameter_set.save_as(tmp_dir, 'parameters', 'json')
        assert os.path.isfile(Path(tmp_dir) / 'parameters.json')


def test_create_json_parameter_file_content(parameter_set: hb.ParameterSet):
    with tempfile.TemporaryDirectory() as tmp_dir:
        parameter_set.save_as(tmp_dir, 'parameters', 'json')
        txt = Path(tmp_dir + '/parameters.json').read_text()
        assert '"degree_day_factor": 3' in txt
        assert '"capacity": 200' in txt


def test_create_yaml_parameter_file_created(parameter_set: hb.ParameterSet):
    with tempfile.TemporaryDirectory() as tmp_dir:
        parameter_set.save_as(tmp_dir, 'parameters', 'yaml')
        assert os.path.isfile(Path(tmp_dir) / 'parameters.yaml')


def test_create_yaml_parameter_file_content(parameter_set: hb.ParameterSet):
    with tempfile.TemporaryDirectory() as tmp_dir:
        parameter_set.save_as(tmp_dir, 'parameters', 'yaml')
        txt = Path(tmp_dir + '/parameters.yaml').read_text()
        assert 'degree_day_factor: 3' in txt
        assert 'capacity: 200' in txt


def test_set_random_values(parameter_set: hb.ParameterSet):
    parameter_set = hb.ParameterSet()
    parameter_set.define_parameter(
        component='snowpack',
        name='melt_factor',
        aliases=['dd'],
        min_value=0,
        max_value=10
    )
    parameter_set.define_parameter(
        component='snowpack',
        name='melting_temp',
        aliases=['mt'],
        min_value=0,
        max_value=5
    )
    parameter_set.define_parameter(
        component='reservoir',
        name='capacity',
        aliases=['A'],
        min_value=0,
        max_value=3000
    )
    parameter_set.set_random_values(['dd', 'mt', 'A'])
    assert parameter_set.get('dd') >= 0
    assert parameter_set.get('dd') <= 10
    assert parameter_set.get('mt') >= 0
    assert parameter_set.get('mt') <= 5
    assert parameter_set.get('A') >= 0
    assert parameter_set.get('A') <= 3000


def test_set_random_values_with_lists(parameter_set: hb.ParameterSet):
    parameter_set = hb.ParameterSet()
    parameter_set.define_parameter(
        component='snowpack',
        name='melt_factor',
        aliases=['dd'],
        min_value=[0, 1],
        max_value=[2, 3]
    )
    parameter_set.define_parameter(
        component='snowpack',
        name='melting_temp',
        aliases=['mt'],
        min_value=[1, 2],
        max_value=[3, 4]
    )
    parameter_set.define_parameter(
        component='reservoir',
        name='capacity',
        aliases=['A'],
        min_value=[0, 10],
        max_value=[10, 200]
    )
    parameter_set.set_random_values(['dd', 'mt', 'A'])
    assert parameter_set.get('dd')[0] >= 0
    assert parameter_set.get('dd')[0] <= 2
    assert parameter_set.get('dd')[1] >= 1
    assert parameter_set.get('dd')[1] <= 3
    assert parameter_set.get('mt')[0] >= 1
    assert parameter_set.get('mt')[0] <= 3
    assert parameter_set.get('mt')[1] >= 2
    assert parameter_set.get('mt')[1] <= 4
    assert parameter_set.get('A')[0] >= 0
    assert parameter_set.get('A')[0] <= 10
    assert parameter_set.get('A')[1] >= 10
    assert parameter_set.get('A')[1] <= 200


def test_reset_random_values_with_lists(parameter_set: hb.ParameterSet):
    parameter_set = hb.ParameterSet()
    parameter_set.define_parameter(
        component='snowpack',
        name='melt_factor',
        aliases=['dd'],
        min_value=[0, 1],
        max_value=[2, 3]
    )
    parameter_set.set_random_values(['dd'])
    parameter_set.set_random_values(['dd'])
    assert parameter_set.get('dd')[0] >= 0
    assert parameter_set.get('dd')[0] <= 2
    assert parameter_set.get('dd')[1] >= 1
    assert parameter_set.get('dd')[1] <= 3


def test_get_model_only_parameters(parameter_set: hb.ParameterSet):
    parameter_set.add_data_parameter('lapse', 0.6, min_value=0, max_value=1)
    model_params = parameter_set.get_model_parameters()
    assert len(model_params) == 3


def test_change_parameter_range(parameter_set: hb.ParameterSet):
    parameter_set.change_range('a_snow', 2, 8)
    assert parameter_set.parameters.loc[0].at['min'] == 2
    assert parameter_set.parameters.loc[0].at['max'] == 8


@pytest.fixture
def parameter_set_constraints():
    parameter_set = hb.ParameterSet()
    parameter_set.define_parameter(
        component='reservoir',
        name='coefficient_1',
        aliases=['k1'],
        min_value=0,
        max_value=1
    )
    parameter_set.define_parameter(
        component='reservoir',
        name='coefficient_2',
        aliases=['k2'],
        min_value=0,
        max_value=1
    )
    parameter_set.define_parameter(
        component='reservoir',
        name='coefficient_3',
        aliases=['k3'],
        min_value=0,
        max_value=1
    )
    parameter_set.define_constraint('k1', '<', 'k2')
    parameter_set.define_constraint('k2', '<', 'k3')
    return parameter_set


def test_define_constraints_satisfied(parameter_set_constraints: hb.ParameterSet):
    assert len(parameter_set_constraints.constraints) == 2
    parameter_set_constraints.set_values({'k1': 0.2, 'k2': 0.3, 'k3': 0.4})
    assert parameter_set_constraints.constraints_satisfied()


def test_define_constraints_not_satisfied(parameter_set_constraints: hb.ParameterSet):
    assert len(parameter_set_constraints.constraints) == 2
    parameter_set_constraints.set_values({'k1': 0.2, 'k2': 0.1, 'k3': 0.4})
    assert not parameter_set_constraints.constraints_satisfied()


def test_define_constraints_removed(parameter_set_constraints: hb.ParameterSet):
    assert len(parameter_set_constraints.constraints) == 2
    parameter_set_constraints.remove_constraint('k1', '<', 'k2')
    assert len(parameter_set_constraints.constraints) == 1
