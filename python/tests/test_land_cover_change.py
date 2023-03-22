import os
from pathlib import Path

import hydrobricks as hb
import hydrobricks.behaviours as behaviours
import hydrobricks.models as models
import pytest

TEST_FILES_DIR = Path(
    os.path.dirname(os.path.realpath(__file__)),
    '..', '..', 'tests', 'files',
)


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
        area_unit='km2', column_elevation='Elevation Bands',
        columns_areas={'ground': 'Sum_Area Non Glacier Band',
                       'glacier_ice': 'Sum_Area ICE Band',
                       'glacier_debris': 'Sum_Area Debris Band'})
    return hydro_units


def test_load_from_csv(hydro_units_csv):
    changes = behaviours.BehaviourLandCoverChange()
    changes.load_from_csv(
        TEST_FILES_DIR / 'parsing' / 'surface_changes_glacier_ice.csv',
        hydro_units_csv, area_unit='km2', match_with='elevation'
    )

    assert changes.get_land_covers_nb() == 1
    assert changes.get_changes_nb() == 232


def test_load_from_two_files(hydro_units_csv):
    changes = behaviours.BehaviourLandCoverChange()
    changes.load_from_csv(
        TEST_FILES_DIR / 'parsing' / 'surface_changes_glacier_ice.csv',
        hydro_units_csv, area_unit='km2', match_with='elevation'
    )
    changes.load_from_csv(
        TEST_FILES_DIR / 'parsing' / 'surface_changes_glacier_debris.csv',
        hydro_units_csv, area_unit='km2', match_with='elevation'
    )

    assert changes.get_land_covers_nb() == 2
    assert changes.get_changes_nb() == 444


def test_add_behaviour_to_model(hydro_units_csv):
    changes = behaviours.BehaviourLandCoverChange()
    changes.load_from_csv(
        TEST_FILES_DIR / 'parsing' / 'surface_changes_glacier_ice.csv',
        hydro_units_csv, area_unit='km2', match_with='elevation'
    )

    model = models.Socont()
    assert model.add_behaviour(changes)


def test_behaviour_correctly_set_in_model(hydro_units_csv):
    changes = behaviours.BehaviourLandCoverChange()
    changes.load_from_csv(
        TEST_FILES_DIR / 'parsing' / 'surface_changes_glacier_ice.csv',
        hydro_units_csv, area_unit='km2', match_with='elevation'
    )

    cover_names = ['ground', 'glacier_ice']
    cover_types = ['ground', 'glacier']
    model = models.Socont(land_cover_names=cover_names, land_cover_types=cover_types)

    assert model.add_behaviour(changes)
    assert model.get_behaviours_nb() == 1
    assert model.get_behaviour_items_nb() == 232


def test_behaviour_2_files_correctly_set_in_model(hydro_units_csv):
    changes = behaviours.BehaviourLandCoverChange()
    changes.load_from_csv(
        TEST_FILES_DIR / 'parsing' / 'surface_changes_glacier_ice.csv',
        hydro_units_csv, area_unit='km2', match_with='elevation'
    )
    changes.load_from_csv(
        TEST_FILES_DIR / 'parsing' / 'surface_changes_glacier_debris.csv',
        hydro_units_csv, area_unit='km2', match_with='elevation'
    )

    cover_names = ['ground', 'glacier_ice']
    cover_types = ['ground', 'glacier']
    model = models.Socont(land_cover_names=cover_names, land_cover_types=cover_types)

    assert model.add_behaviour(changes)
    assert model.get_behaviours_nb() == 1
    assert model.get_behaviour_items_nb() == 444
