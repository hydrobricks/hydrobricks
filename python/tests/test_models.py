import os.path
import tempfile
from pathlib import Path

import hydrobricks as hb
import hydrobricks.models as models

TEST_FILES_DIR = Path(
    os.path.dirname(os.path.realpath(__file__)),
    '..', '..', 'tests', 'files', 'catchments'
)
CATCHMENT_BANDS = TEST_FILES_DIR / 'ch_sitter_appenzell' / 'elevation_bands.csv'
CATCHMENT_METEO = TEST_FILES_DIR / 'ch_sitter_appenzell' / 'meteo.csv'
CATCHMENT_DISCHARGE = TEST_FILES_DIR / 'ch_sitter_appenzell' / 'discharge.csv'


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
    # Model options
    socont = models.Socont(soil_storage_nb=2, surface_runoff="linear_storage",
                           record_all=True)

    # Parameters
    parameters = socont.generate_parameters()

    # Model parameters: a_snow [mm/d/°C], k_slow_1 [1/t], k_slow_2 [1/t],
    # k_quick [1/t], k_snow [1/t], A [mm], percol [mm/d]
    parameters.set_values({'a_snow': 3, 'k_snow': 0.01, 'k_quick': 0.05, 'A': 200,
                           'k_slow_1': 0.001, 'percol': 0.5, 'k_slow_2': 0.005})

    # Add data parameters
    parameters.add_data_parameter('precip_correction_factor', 1)
    parameters.add_data_parameter('precip_gradient', 0.04)
    parameters.add_data_parameter('temp_gradients', -0.6)

    # Preparation of the hydro units
    hydro_units = hb.HydroUnits()
    hydro_units.load_from_csv(
        CATCHMENT_BANDS, area_unit='m2', column_elevation='elevation',
        column_area='area')

    # Preparation of the forcing data
    forcing = hb.Forcing(hydro_units)
    forcing.load_from_csv(
        CATCHMENT_METEO, column_time='Date', time_format='%d/%m/%Y',
        content={'precipitation': 'precip(mm/day)', 'temperature': 'temp(C)',
                 'pet': 'pet_sim(mm/day)'})

    station_temp_alt = 2507  # Temperature measurement station altitude
    station_precip_alt = 2507  # Precipitation measurement station altitude

    forcing.spatialize_temperature(station_temp_alt, parameters.get('temp_gradients'))
    forcing.spatialize_pet()
    forcing.spatialize_precipitation(
        ref_elevation=station_precip_alt,
        gradient=parameters.get('precip_gradient'),
        correction_factor=parameters.get('precip_correction_factor'))

    with tempfile.TemporaryDirectory() as tmp_dir:
        socont.setup(spatial_structure=hydro_units, output_path=tmp_dir,
                     start_date='1981-01-01', end_date='2020-12-31')
        socont.initialize_state_variables(parameters=parameters, forcing=forcing)
        socont.run(parameters=parameters, forcing=forcing)
