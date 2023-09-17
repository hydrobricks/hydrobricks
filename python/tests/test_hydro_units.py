import os
import tempfile
from pathlib import Path

import pytest

import hydrobricks as hb

TEST_FILES_DIR = Path(
    os.path.dirname(os.path.realpath(__file__)),
    '..', '..', 'tests', 'files',
)


def test_hydro_units_creation():
    hb.HydroUnits()


def test_hydro_units_creation_with_land_covers():
    hb.HydroUnits(land_cover_types=['ground', 'glacier'],
                  land_cover_names=['ground', 'glacier'])


def test_hydro_units_creation_with_land_covers_mismatch():
    with pytest.raises(ValueError):
        hb.HydroUnits(land_cover_types=['ground', 'glacier', 'glacier'],
                      land_cover_names=None)


def test_hydro_units_creation_with_land_covers_size_mismatch():
    with pytest.raises(ValueError):
        hb.HydroUnits(land_cover_types=['ground', 'glacier', 'glacier'],
                      land_cover_names=['ground', 'glacier'])


@pytest.fixture
def hydro_units():
    hydro_units = hb.HydroUnits(
        land_cover_types=['ground', 'glacier', 'glacier'],
        land_cover_names=['ground', 'glacier_ice', 'glacier_debris'])
    return hydro_units


@pytest.fixture
def hydro_units_csv(hydro_units):
    hydro_units.load_from_csv(
        TEST_FILES_DIR / 'parsing' / 'hydro_units_absolute_areas.csv',
        column_elevation='Elevation Bands',
        columns_areas={'ground': 'Sum_Area Non Glacier Band',
                       'glacier_ice': 'Sum_Area ICE Band',
                       'glacier_debris': 'Sum_Area Debris Band'})
    return hydro_units


def test_load_from_csv(hydro_units_csv):
    hu = hydro_units_csv.hydro_units
    assert hu.loc[0].at['id'].values == 1
    assert hu.loc[10].at['id'].values == 11
    assert hu.loc[20].at['id'].values == 21
    assert hu.loc[0].at['area'].values == pytest.approx(2408000, abs=0.001)
    assert hu.loc[10].at['area'].values == pytest.approx(2806000, abs=0.001)
    assert hu.loc[20].at['area'].values == pytest.approx(1483000, abs=0.001)
    assert hu.loc[0].at['elevation'].values == 3986
    assert hu.loc[10].at['elevation'].values == 4346
    assert hu.loc[20].at['elevation'].values == 4706
    assert hu.loc[0].at['fraction-ground'].values == 1
    assert (hu.loc[10].at['fraction-ground'].values ==
            pytest.approx(0.918, abs=0.001))
    assert (hu.loc[20].at['fraction-ground'].values ==
            pytest.approx(0.770, abs=0.001))
    assert hu.loc[0].at['fraction-glacier_ice'].values == 0
    assert (hu.loc[10].at['fraction-glacier_ice'].values ==
            pytest.approx(0.018, abs=0.001))
    assert (hu.loc[20].at['fraction-glacier_ice'].values ==
            pytest.approx(0.206, abs=0.001))
    assert hu.loc[0].at['fraction-glacier_debris'].values == 0
    assert (hu.loc[10].at['fraction-glacier_debris'].values ==
            pytest.approx(0.062, abs=0.001))
    assert (hu.loc[20].at['fraction-glacier_debris'].values ==
            pytest.approx(0.023, abs=0.001))


def test_create_file(hydro_units_csv):
    if not hb.has_netcdf:
        return

    with tempfile.TemporaryDirectory() as tmp_dir:
        hydro_units_csv.save_as(tmp_dir + '/test.nc')
