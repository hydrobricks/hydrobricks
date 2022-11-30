import os.path
import tempfile
from pathlib import Path

import hydrobricks.models as models


def test_socont_creation():
    model = models.Socont()
    assert model.name == 'socont'


def test_socont_creation_with_soil_storage_nb():
    models.Socont(soil_storage_nb=2)


def test_socont_creation_with_solver():
    models.Socont(solver='runge_kutta')


def test_socont_creation_with_surface_runoff():
    models.Socont(surface_runoff='linear_storage')


def test_socont_creation_with_land_covers():
    cover_names = ['ground', 'aletsch_glacier']
    cover_types = ['ground', 'glacier']
    models.Socont(land_cover_names=cover_names, land_cover_types=cover_types)


def test_create_json_config_file_created():
    model = models.Socont(soil_storage_nb=2, surface_runoff='linear_storage')
    with tempfile.TemporaryDirectory() as tmp_dir:
        model.create_config_file(tmp_dir, 'structure', 'json')
        assert os.path.isfile(Path(tmp_dir) / 'structure.json')


def test_create_json_config_file_content():
    cover_names = ['ground', 'aletsch_glacier']
    cover_types = ['ground', 'glacier']
    model = models.Socont(solver='runge_kutta', soil_storage_nb=2,
                          land_cover_names=cover_names,
                          land_cover_types=cover_types,
                          surface_runoff='linear_storage')
    with tempfile.TemporaryDirectory() as tmp_dir:
        model.create_config_file(tmp_dir, 'structure', 'json')
        txt = Path(tmp_dir + '/structure.json').read_text()
        assert '"base": "socont"' in txt
        assert '"solver": "runge_kutta"' in txt
        assert '"soil_storage_nb": 2' in txt
        assert '"surface_runoff": "linear_storage"' in txt
        assert 'ground' in txt
        assert 'aletsch_glacier' in txt


def test_create_yaml_config_file_created():
    model = models.Socont(soil_storage_nb=2, surface_runoff='linear_storage')
    with tempfile.TemporaryDirectory() as tmp_dir:
        model.create_config_file(tmp_dir, 'structure', 'yaml')
        assert os.path.isfile(Path(tmp_dir) / 'structure.yaml')


def test_create_yaml_config_file_content():
    cover_names = ['ground', 'aletsch_glacier']
    cover_types = ['ground', 'glacier']
    model = models.Socont(solver='runge_kutta', soil_storage_nb=2,
                          land_cover_names=cover_names,
                          land_cover_types=cover_types,
                          surface_runoff='linear_storage')
    with tempfile.TemporaryDirectory() as tmp_dir:
        model.create_config_file(tmp_dir, 'structure', 'yaml')
        txt = Path(tmp_dir + '/structure.yaml').read_text()
        assert 'base: socont' in txt
        assert 'solver: runge_kutta' in txt
        assert 'soil_storage_nb: 2' in txt
        assert 'surface_runoff: linear_storage' in txt
        assert 'ground' in txt
        assert 'aletsch_glacier' in txt


def test_socont_closes_water_balance():
    pass
