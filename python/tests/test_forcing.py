import os
from pathlib import Path

import pytest

import hydrobricks as hb

CATCHMENT_DIR = Path(
    os.path.dirname(os.path.realpath(__file__)),
    '..', '..', 'tests', 'files', 'catchments', 'ch_sitter_appenzell'
)


@pytest.fixture
def hydro_units():
    # Preparation of the hydro units
    hydro_units = hb.HydroUnits()
    hydro_units.load_from_csv(
        CATCHMENT_DIR / 'elevation_bands.csv',
        area_unit='m2', column_elevation='elevation', column_area='area')
    return hydro_units


@pytest.fixture
def forcing(hydro_units):
    forcing = hb.Forcing(hydro_units)
    forcing.load_station_data_from_csv(
        CATCHMENT_DIR / 'meteo.csv', column_time='Date', time_format='%d/%m/%Y',
        content={'precipitation': 'precip(mm/day)', 'temperature': 'temp(C)',
                 'pet': 'pet_sim(mm/day)'})
    return forcing


@pytest.fixture
def parameters():
    parameters = hb.ParameterSet()
    return parameters


def test_forcing_get_variable_enum(hydro_units):
    forcing = hb.Forcing(hydro_units)
    assert forcing.get_variable_enum('precipitation') == hb.Forcing.Variable.P
    assert forcing.get_variable_enum('precip') == hb.Forcing.Variable.P
    assert forcing.get_variable_enum('p') == hb.Forcing.Variable.P
    assert forcing.get_variable_enum('temperature') == hb.Forcing.Variable.T
    assert forcing.get_variable_enum('temp') == hb.Forcing.Variable.T
    assert forcing.get_variable_enum('t') == hb.Forcing.Variable.T
    assert forcing.get_variable_enum('temperature_min') == hb.Forcing.Variable.T_MIN
    assert forcing.get_variable_enum('min_temperature') == hb.Forcing.Variable.T_MIN
    assert forcing.get_variable_enum('t_min') == hb.Forcing.Variable.T_MIN
    assert forcing.get_variable_enum('tmin') == hb.Forcing.Variable.T_MIN
    assert forcing.get_variable_enum('temperature_max') == hb.Forcing.Variable.T_MAX
    assert forcing.get_variable_enum('max_temperature') == hb.Forcing.Variable.T_MAX
    assert forcing.get_variable_enum('t_max') == hb.Forcing.Variable.T_MAX
    assert forcing.get_variable_enum('tmax') == hb.Forcing.Variable.T_MAX
    assert forcing.get_variable_enum('pet') == hb.Forcing.Variable.PET
    assert forcing.get_variable_enum('relative_humidity') == hb.Forcing.Variable.RH
    assert forcing.get_variable_enum('rh') == hb.Forcing.Variable.RH
    rh_min = hb.Forcing.Variable.RH_MIN
    assert forcing.get_variable_enum('relative_humidity_min') == rh_min
    assert forcing.get_variable_enum('min_relative_humidity') == rh_min
    assert forcing.get_variable_enum('rh_min') == rh_min
    assert forcing.get_variable_enum('rhmin') == rh_min
    rh_max = hb.Forcing.Variable.RH_MAX
    assert forcing.get_variable_enum('relative_humidity_max') == rh_max
    assert forcing.get_variable_enum('max_relative_humidity') == rh_max
    assert forcing.get_variable_enum('rh_max') == rh_max
    assert forcing.get_variable_enum('rhmax') == rh_max
    assert forcing.get_variable_enum('net_radiation') == hb.Forcing.Variable.R_NET
    assert forcing.get_variable_enum('r_net') == hb.Forcing.Variable.R_NET
    assert forcing.get_variable_enum('r_n') == hb.Forcing.Variable.R_NET
    assert forcing.get_variable_enum('rn') == hb.Forcing.Variable.R_NET
    assert forcing.get_variable_enum('solar_radiation') == hb.Forcing.Variable.R_SOLAR
    assert forcing.get_variable_enum('r_solar') == hb.Forcing.Variable.R_SOLAR
    assert forcing.get_variable_enum('r_s') == hb.Forcing.Variable.R_SOLAR
    assert forcing.get_variable_enum('rs') == hb.Forcing.Variable.R_SOLAR
    assert forcing.get_variable_enum('sunshine_duration') == hb.Forcing.Variable.SD
    assert forcing.get_variable_enum('sd') == hb.Forcing.Variable.SD
    assert forcing.get_variable_enum('wind_speed') == hb.Forcing.Variable.WIND
    assert forcing.get_variable_enum('wind') == hb.Forcing.Variable.WIND


def test_forcing_load_station_data_from_csv(hydro_units):
    forcing = hb.Forcing(hydro_units)
    forcing.load_station_data_from_csv(
        CATCHMENT_DIR / 'meteo.csv', column_time='Date', time_format='%d/%m/%Y',
        content={'precipitation': 'precip(mm/day)', 'temperature': 'temp(C)',
                 'pet': 'pet_sim(mm/day)'})
    assert len(forcing.data1D.data_name) == 3
    assert forcing.data1D.data_name == [hb.Forcing.Variable.P, hb.Forcing.Variable.T,
                                        hb.Forcing.Variable.PET]
    assert forcing.data1D.data[0].shape[0] > 0
    assert forcing.data1D.data[1].shape[0] > 0
    assert forcing.data1D.data[2].shape[0] > 0


def test_set_prior_correction(forcing):
    forcing.set_prior_correction(
        variable='precipitation', method='additive',
        correction='param:precip_corr')
    assert len(forcing._operations) == 1
    assert forcing._operations[0]['type'] == 'prior_correction'
    assert forcing._operations[0]['method'] == 'additive'


def test_set_spatialization_from_station(forcing):
    forcing.set_spatialization_from_station_data(
        variable='temperature', method='additive_elevation_gradient',
        ref_elevation=1250, gradient='param:temp_gradients')
    forcing.set_spatialization_from_station_data(
        variable='precipitation', method='multiplicative_elevation_gradient',
        ref_elevation=1250, gradient='param:precip_gradient',
        correction_factor='param:precip_corr_factor'
    )
    assert len(forcing._operations) == 2
    assert forcing._operations[0]['type'] == 'spatialize_from_station'
    assert forcing._operations[0]['method'] == 'additive_elevation_gradient'


def test_set_pet_computation(forcing):
    forcing.set_pet_computation(
        method='Priestley-Taylor', use=['t', 'rs', 'tmax', 'tmin', 'rh', 'lat'])
    assert len(forcing._operations) == 1
    assert forcing._operations[0]['type'] == 'pet_computation'
    assert forcing._operations[0]['method'] == 'Priestley-Taylor'


def test_apply_prior_correction_multiplicative(forcing, parameters):
    parameters.add_data_parameter('precip_corr_factor', 0.85)
    forcing.set_prior_correction(
        variable='precipitation', method='multiplicative',
        correction_factor='param:precip_corr_factor')
    sum_before = forcing.data1D.data[0].sum()
    forcing.apply_operations(parameters)
    sum_after = forcing.data1D.data[0].sum()
    assert sum_after == pytest.approx(sum_before * 0.85)


def test_apply_prior_correction_additive(forcing, parameters):
    parameters.add_data_parameter('temp_corr_factor', 1.1)
    forcing.set_prior_correction(
        variable='temperature', method='additive',
        correction_factor='param:temp_corr_factor')
    sum_before = forcing.data1D.data[1].sum()
    forcing.apply_operations(parameters)
    sum_after = forcing.data1D.data[1].sum()
    data_length = forcing.data1D.data[1].shape[0]
    assert sum_after == pytest.approx(sum_before + data_length * 1.1)


def test_apply_spatialization_from_station_data_temperature(forcing, parameters):
    parameters.add_data_parameter('temp_gradients', -0.6)
    forcing.set_spatialization_from_station_data(
        variable='temperature', ref_elevation=1250,
        gradient='param:temp_gradients')
    forcing.apply_operations(parameters)
    assert len(forcing.data2D.data) == 1
    assert forcing.data2D.data[0].shape == (len(forcing.data1D.time),
                                            len(forcing.hydro_units))
    diff_50m = forcing.data2D.data[0][0, 0] - forcing.data2D.data[0][0, 1]
    assert diff_50m == pytest.approx(0.6/2)


def test_apply_spatialization_from_station_data_precipitation(forcing, parameters):
    parameters.add_data_parameter('precip_gradient', 1.1)
    forcing.set_spatialization_from_station_data(
        variable='precipitation', ref_elevation=1250,
        gradient='param:precip_gradient')
    forcing.apply_operations(parameters)
    assert len(forcing.data2D.data) == 1
    assert forcing.data2D.data[0].shape == (len(forcing.data1D.time),
                                            len(forcing.hydro_units))


def test_apply_pet_computation_wrong_variable_name(forcing, parameters):
    if not hb.has_pyet:
        return
    forcing.set_pet_computation(
        method='Priestley-Taylor', use=['xy', 'rs', 'tmax', 'tmin', 'rh', 'lat'])
    with pytest.raises(ValueError):
        forcing.apply_operations(parameters)


def test_apply_pet_computation_variables_not_available(forcing, parameters):
    if not hb.has_pyet:
        return
    forcing.set_pet_computation(
        method='Priestley-Taylor', use=['t', 'rs', 'tmax', 'tmin', 'rh', 'lat'],
        lat=47.3)
    with pytest.raises(ValueError):
        forcing.apply_operations(parameters)


def test_apply_pet_computation_hamon(forcing, parameters):
    if not hb.has_pyet:
        return
    forcing.set_spatialization_from_station_data(
        variable='temperature', method='additive_elevation_gradient',
        ref_elevation=1250, gradient=-0.6)
    forcing.set_pet_computation(
        method='Hamon', use=['t', 'lat'], lat=47.3)
    forcing.apply_operations(parameters)
