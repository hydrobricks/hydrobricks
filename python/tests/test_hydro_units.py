import hydrobricks as hb
import pytest


def test_hydro_units_creation():
    hb.HydroUnits()


def test_hydro_units_creation_with_surfaces():
    hb.HydroUnits(surface_types=['ground', 'glacier'],
                  surface_names=['ground', 'glacier'])


def test_hydro_units_creation_with_surfaces_size_mismatch():
    with pytest.raises(Exception):
        hb.HydroUnits(surface_types=['ground', 'glacier', 'glacier'],
                      surface_names=['ground', 'glacier'])


@pytest.fixture
def hydro_units():
    hydro_units = hb.HydroUnits(
        surface_types=['ground', 'glacier', 'glacier'],
        surface_names=['ground', 'glacier-ice', 'glacier-debris'])
    return hydro_units


def test_load_from_csv(hydro_units):
    hydro_units.load_from_csv(
        'files/hydro_units_absolute_areas.csv', area_unit='km',
        column_elevation='Elevation Bands',
        columns_areas={'ground': 'Sum_Area Non Glacier Band',
                       'glacier-ice': 'Sum_Area ICE Band',
                       'glacier-debris': 'Sum_Area Debris Band'})
