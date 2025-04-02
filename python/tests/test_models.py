import os.path
import tempfile
from pathlib import Path

import pytest

import hydrobricks as hb
import hydrobricks.models as models

TEST_FILES_DIR = Path(
    os.path.dirname(os.path.realpath(__file__)),
    '..', '..', 'tests', 'files', 'catchments'
)
CATCHMENT_BANDS = TEST_FILES_DIR / 'ch_sitter_appenzell' / 'hydro_units.csv'
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


def test_socont_with_1_soil_storage_closes_water_balance():
    tmp_dir = tempfile.TemporaryDirectory()

    # Model options
    socont = models.Socont(soil_storage_nb=1, surface_runoff="linear_storage",
                           record_all=True)

    # Parameters
    parameters = socont.generate_parameters()
    parameters.set_values({'a_snow': 3, 'k_quick': 0.05, 'A': 200, 'k_slow': 0.001})
    parameters.add_data_parameter('precip_correction_factor', 1)
    parameters.add_data_parameter('precip_gradient', 0.05)
    parameters.add_data_parameter('temp_gradients', -0.6)

    # Preparation of the hydro units
    hydro_units = hb.HydroUnits()
    hydro_units.load_from_csv(
        CATCHMENT_BANDS, column_elevation='elevation',
        column_area='area')

    # Preparation of the forcing data
    station_temp_alt = 1250  # Temperature reference altitude
    station_precip_alt = 1250  # Precipitation reference altitude
    forcing = hb.Forcing(hydro_units)
    forcing.load_station_data_from_csv(
        CATCHMENT_METEO, column_time='Date', time_format='%d/%m/%Y',
        content={'precipitation': 'precip(mm/day)', 'temperature': 'temp(C)',
                 'pet': 'pet_sim(mm/day)'})
    forcing.spatialize_from_station_data(
        variable='temperature', ref_elevation=station_temp_alt,
        gradient=parameters.get('temp_gradients'))
    forcing.spatialize_from_station_data(variable='pet')
    forcing.correct_station_data(
        variable='precipitation',
        correction_factor=parameters.get('precip_correction_factor'))
    forcing.spatialize_from_station_data(
        variable='precipitation', ref_elevation=station_precip_alt,
        gradient=parameters.get('precip_gradient'))

    socont.setup(spatial_structure=hydro_units, output_path=tmp_dir.name,
                 start_date='1981-01-01', end_date='2020-12-31')
    socont.run(parameters=parameters, forcing=forcing)

    # Water balance
    precip_total = forcing.get_total_precipitation()
    discharge_total = socont.get_total_outlet_discharge()
    et_total = socont.get_total_et()
    storage_change = socont.get_total_water_storage_changes()
    snow_change = socont.get_total_snow_storage_changes()
    balance = discharge_total + et_total + storage_change + snow_change - precip_total

    assert balance == pytest.approx(0, abs=1e-8)

    try:
        tmp_dir.cleanup()
    except Exception:
        print('Could not remove temporary directory.')


def test_socont_with_2_soil_storages_closes_water_balance():
    tmp_dir = tempfile.TemporaryDirectory()

    # Model options
    socont = models.Socont(soil_storage_nb=2, surface_runoff="linear_storage",
                           record_all=True)

    # Parameters
    parameters = socont.generate_parameters()
    parameters.set_values({'a_snow': 3, 'k_quick': 0.05, 'A': 200, 'k_slow_1': 0.001,
                           'percol': 0.5, 'k_slow_2': 0.005})
    parameters.add_data_parameter('precip_correction_factor', 1)
    parameters.add_data_parameter('precip_gradient', 0.05)
    parameters.add_data_parameter('temp_gradients', -0.6)

    # Preparation of the hydro units
    hydro_units = hb.HydroUnits()
    hydro_units.load_from_csv(
        CATCHMENT_BANDS, column_elevation='elevation',
        column_area='area')

    # Preparation of the forcing data
    station_temp_alt = 1250  # Temperature reference altitude
    station_precip_alt = 1250  # Precipitation reference altitude
    forcing = hb.Forcing(hydro_units)
    forcing.load_station_data_from_csv(
        CATCHMENT_METEO, column_time='Date', time_format='%d/%m/%Y',
        content={'precipitation': 'precip(mm/day)', 'temperature': 'temp(C)',
                 'pet': 'pet_sim(mm/day)'})
    forcing.spatialize_from_station_data(
        variable='temperature', ref_elevation=station_temp_alt,
        gradient=parameters.get('temp_gradients'))
    forcing.spatialize_from_station_data(variable='pet')
    forcing.correct_station_data(
        variable='precipitation',
        correction_factor=parameters.get('precip_correction_factor'))
    forcing.spatialize_from_station_data(
        variable='precipitation', ref_elevation=station_precip_alt,
        gradient=parameters.get('precip_gradient'))

    socont.setup(spatial_structure=hydro_units, output_path=tmp_dir.name,
                 start_date='1981-01-01', end_date='2020-12-31')
    socont.run(parameters=parameters, forcing=forcing)

    # Water balance
    precip_total = forcing.get_total_precipitation()
    discharge_total = socont.get_total_outlet_discharge()
    et_total = socont.get_total_et()
    storage_change = socont.get_total_water_storage_changes()
    snow_change = socont.get_total_snow_storage_changes()
    balance = discharge_total + et_total + storage_change + snow_change - precip_total

    assert balance == pytest.approx(0, abs=1e-8)

    try:
        tmp_dir.cleanup()
    except Exception:
        print('Could not remove temporary directory.')


def test_error_is_raised_if_parameter_missing():
    tmp_dir = tempfile.TemporaryDirectory()

    # Model options
    socont = models.Socont(soil_storage_nb=2, surface_runoff="linear_storage",
                           record_all=True)

    # Parameters
    parameters = socont.generate_parameters()
    parameters.set_values({'a_snow': 3, 'k_quick': 0.05, 'A': 200, 'k_slow_1': 0.001})

    # Preparation of the hydro units
    hydro_units = hb.HydroUnits()
    hydro_units.load_from_csv(
        CATCHMENT_BANDS, column_elevation='elevation',
        column_area='area')

    socont.setup(spatial_structure=hydro_units, output_path=tmp_dir.name,
                 start_date='1981-01-01', end_date='2020-12-31')

    with pytest.raises(RuntimeError, match=r"Some parameters were not defined*"):
        socont.run(parameters=parameters)

    try:
        tmp_dir.cleanup()
    except Exception:
        print('Could not remove temporary directory.')
