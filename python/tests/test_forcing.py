import os
import tempfile
from pathlib import Path

import hydrobricks as hb
import pytest

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


def test_forcing_load_station_data_from_csv(hydro_units):
    forcing = hb.Forcing(hydro_units)
    forcing.load_station_data_from_csv(
        CATCHMENT_DIR / 'meteo.csv', column_time='Date', time_format='%d/%m/%Y',
        content={'precipitation': 'precip(mm/day)', 'temperature': 'temp(C)',
                 'pet': 'pet_sim(mm/day)'})
    assert len(forcing.data1D.data_name) == 3
    assert forcing.data1D.data_name == ['precipitation', 'temperature', 'pet']
    assert forcing.data1D.data[0].shape[0] > 0
    assert forcing.data1D.data[1].shape[0] > 0
    assert forcing.data1D.data[2].shape[0] > 0


def test_set_prior_correction(forcing):
    forcing.set_prior_correction(
        variable='precipitation', method='additive',
        correction='param:precip_corr')
    assert len(forcing.operations) == 1
    assert forcing.operations[0]['type'] == 'prior_correction'
    assert forcing.operations[0]['method'] == 'additive'


def test_set_spatialization_from_station(forcing):
    ref_elevation = 1250  # Reference altitude for the meteo data
    forcing.set_spatialization_from_station_data(
        variable='temperature', method='additive_elevation_gradient',
        ref_elevation=ref_elevation, gradient='param:temp_gradients')
    forcing.set_spatialization_from_station_data(
        variable='precipitation', method='multiplicative_elevation_gradient',
        ref_elevation=ref_elevation, gradient='param:precip_gradient',
        correction_factor='param:precip_corr_factor'
    )
    assert len(forcing.operations) == 2
    assert forcing.operations[0]['type'] == 'spatialize_from_station'
    assert forcing.operations[0]['method'] == 'additive_elevation_gradient'


def test_apply_prior_correction_multiplicative(forcing):
    parameters = hb.ParameterSet()
    parameters.add_data_parameter('precip_corr_factor', 0.85)
    forcing.set_prior_correction(
        variable='precipitation', method='multiplicative',
        correction_factor='param:precip_corr_factor')
    sum_before = forcing.data1D.data[0].sum()
    forcing.apply_operations(parameters)
    sum_after = forcing.data1D.data[0].sum()
    assert sum_after == pytest.approx(sum_before * 0.85)


def test_apply_prior_correction_additive(forcing):
    parameters = hb.ParameterSet()
    parameters.add_data_parameter('temp_corr_factor', 1.1)
    forcing.set_prior_correction(
        variable='temperature', method='additive',
        correction_factor='param:temp_corr_factor')
    sum_before = forcing.data1D.data[1].sum()
    forcing.apply_operations(parameters)
    sum_after = forcing.data1D.data[1].sum()
    data_length = forcing.data1D.data[1].shape[0]
    assert sum_after == pytest.approx(sum_before + data_length * 1.1)
