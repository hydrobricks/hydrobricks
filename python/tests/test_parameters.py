import os.path
import tempfile
from pathlib import Path

import hydrobricks as hb
import pytest


def test_parameter_creation():
    hb.ParameterSet()


def test_define_parameter():
    parameter_set = hb.ParameterSet()
    parameter_set.define_parameter(
        component='snowpack', name='degreeDayFactor', unit='mm/d',
        aliases=['an', 'sdd'], min_value=0, max_value=10, mandatory=False)
    assert parameter_set.parameters.loc[0].at['component'] == 'snowpack'
    assert parameter_set.parameters.loc[0].at['name'] == 'degreeDayFactor'
    assert parameter_set.parameters.loc[0].at['unit'] == 'mm/d'
    assert parameter_set.parameters.loc[0].at['aliases'] == ['an', 'sdd']
    assert parameter_set.parameters.loc[0].at['min'] == 0
    assert parameter_set.parameters.loc[0].at['max'] == 10
    assert not parameter_set.parameters.loc[0].at['mandatory']
    assert parameter_set.parameters.loc[0].at['value'] is None


def test_define_parameter_nb_rows_is_correct():
    parameter_set = hb.ParameterSet()
    parameter_set.define_parameter(
        component='snowpack', name='degreeDayFactor', unit='mm/d', aliases=['x', 'y'],
        default_value=3, min_value=0, max_value=10, mandatory=True)
    parameter_set.define_parameter(
        component='snowpack', name='meltingTemperature', unit='°C', aliases=['z'],
        default_value=0, min_value=0, max_value=5, mandatory=False)
    assert len(parameter_set.parameters) == 2
    assert parameter_set.parameters.loc[0].at['aliases'] == ['x', 'y']
    assert parameter_set.parameters.loc[1].at['aliases'] == ['z']


def test_define_parameter_use_default():
    parameter_set = hb.ParameterSet()
    parameter_set.define_parameter(
        component='snowpack', name='degreeDayFactor', unit='mm/d',
        default_value=3, min_value=0, max_value=10, mandatory=False)
    assert parameter_set.parameters.loc[0].at['value'] == 3


def test_define_parameter_not_using_default_if_mandatory():
    parameter_set = hb.ParameterSet()
    parameter_set.define_parameter(
        component='snowpack', name='degreeDayFactor', unit='mm/d',
        default_value=3, min_value=0, max_value=10, mandatory=True)
    assert parameter_set.parameters.loc[0].at['value'] is None


def test_define_parameter_alias_already_used():
    parameter_set = hb.ParameterSet()
    parameter_set.define_parameter(
        component='snowpack', name='degreeDayFactor', unit='mm/d', aliases=['as', 'sd'])
    with pytest.raises(Exception):
        parameter_set.define_parameter(
            component='glacier', name='degreeDayFactor', unit='mm/d',
            aliases=['as', 'gdd'])


def test_set_parameter_value_by_alias():
    parameter_set = hb.ParameterSet()
    parameter_set.define_parameter(
        component='snowpack', name='degreeDayFactor', unit='mm/d', aliases=['as', 'sd'])
    parameter_set.define_parameter(
        component='glacier', name='degreeDayFactor', unit='mm/d', aliases=['ag', 'gd'])
    parameter_set.set_values({'as': 2, 'gd': 3})
    assert parameter_set.parameters.loc[0].at['value'] == 2
    assert parameter_set.parameters.loc[1].at['value'] == 3


def test_set_parameter_value_by_name():
    parameter_set = hb.ParameterSet()
    parameter_set.define_parameter(
        component='snowpack', name='degreeDayFactor', unit='mm/d', aliases=['as'])
    parameter_set.define_parameter(
        component='glacier', name='degreeDayFactor', unit='mm/d', aliases=['ag'])
    parameter_set.set_values({'snowpack:degreeDayFactor': 2,
                              'glacier:degreeDayFactor': 3})
    assert parameter_set.parameters.loc[0].at['value'] == 2
    assert parameter_set.parameters.loc[1].at['value'] == 3


def test_set_parameter_value_not_found():
    parameter_set = hb.ParameterSet()
    parameter_set.define_parameter(
        component='snowpack', name='degreeDayFactor', unit='mm/d', aliases=['as'])
    with pytest.raises(Exception):
        parameter_set.set_values({'xy': 2})


@pytest.fixture
def parameter_set():
    parameter_set = hb.ParameterSet()
    parameter_set.define_parameter(
        component='snowpack', name='degreeDayFactor', unit='mm/d',
        aliases=['an', 'sdd'], min_value=0, max_value=10, mandatory=True)
    parameter_set.define_parameter(
        component='snowpack', name='meltingTemperature', unit='°C',
        min_value=0, max_value=5, default_value=0, mandatory=False)
    parameter_set.define_parameter(
        component='slow-reservoir', name='capacity', unit='mm', aliases=['A'],
        min_value=0, max_value=3000, mandatory=True)
    parameter_set.set_values({'an': 3, 'A': 200})
    return parameter_set


def test_create_json_parameter_file_created(parameter_set):
    with tempfile.TemporaryDirectory() as tmp_dir:
        parameter_set.create_file(tmp_dir, 'parameters', 'json')
        assert os.path.isfile(Path(tmp_dir) / 'parameters.json')


def test_create_json_parameter_file_content(parameter_set):
    with tempfile.TemporaryDirectory() as tmp_dir:
        parameter_set.create_file(tmp_dir, 'parameters', 'json')
        txt = Path(tmp_dir + '/parameters.json').read_text()
        assert '"degreeDayFactor": 3' in txt
        assert '"capacity": 200' in txt


def test_create_yaml_parameter_file_created(parameter_set):
    with tempfile.TemporaryDirectory() as tmp_dir:
        parameter_set.create_file(tmp_dir, 'parameters', 'yaml')
        assert os.path.isfile(Path(tmp_dir) / 'parameters.yaml')


def test_create_yaml_parameter_file_content(parameter_set):
    with tempfile.TemporaryDirectory() as tmp_dir:
        parameter_set.create_file(tmp_dir, 'parameters', 'yaml')
        txt = Path(tmp_dir + '/parameters.yaml').read_text()
        assert 'degreeDayFactor: 3' in txt
        assert 'capacity: 200' in txt
