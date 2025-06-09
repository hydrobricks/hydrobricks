import os
from pathlib import Path

import numpy as np
import pytest

import hydrobricks as hb
import hydrobricks.actions as actions
import hydrobricks.models as models

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
def hydro_units_csv(hydro_units: hb.HydroUnits):
    hydro_units.load_from_csv(
        TEST_FILES_DIR / 'parsing' / 'hydro_units_absolute_areas.csv',
        column_elevation='Elevation Bands',
        columns_areas={'ground': 'Sum_Area Non Glacier Band',
                       'glacier_ice': 'Sum_Area ICE Band',
                       'glacier_debris': 'Sum_Area Debris Band'})
    return hydro_units


@pytest.fixture
def catchment_gletsch():
    catchment = hb.Catchment(
        TEST_FILES_DIR / 'catchments' / 'ch_rhone_gletsch' / 'outline.shp',
        land_cover_types=['ground', 'glacier'],
        land_cover_names=['ground', 'glacier'])
    catchment.extract_dem(
        TEST_FILES_DIR / 'catchments' / 'ch_rhone_gletsch' / 'dem.tif')
    catchment.create_elevation_bands(method='equal_intervals', distance=50)

    return catchment


@pytest.fixture
def changes_data():
    glacier_dir = TEST_FILES_DIR / 'catchments' / 'ch_rhone_gletsch' / 'glaciers'
    glaciers = [f'{glacier_dir}/sgi_1850.shp',
                f'{glacier_dir}/sgi_1931.shp',
                f'{glacier_dir}/sgi_1973.shp',
                f'{glacier_dir}/sgi_2010.shp',
                f'{glacier_dir}/sgi_2016.shp']
    times = ['1850-01-01', '1931-01-01', '1973-01-01', '2010-01-01', '2016-01-01']

    return [glaciers, times]


def test_load_from_csv(hydro_units_csv: hb.HydroUnits):
    changes = actions.ActionLandCoverChange()
    changes.load_from_csv(
        TEST_FILES_DIR / 'parsing' / 'surface_changes_glacier_ice.csv',
        hydro_units_csv, land_cover='glacier_ice', area_unit='km2',
        match_with='elevation'
    )

    assert changes.get_land_covers_nb() == 1
    assert changes.get_changes_nb() == 232


def test_load_from_csv_by_id(hydro_units_csv: hb.HydroUnits):
    changes = actions.ActionLandCoverChange()
    changes.load_from_csv(
        TEST_FILES_DIR / 'parsing' / 'surface_changes_glacier_ice_with_ids.csv',
        hydro_units_csv, land_cover='glacier_ice', area_unit='km2',
        match_with='id'
    )

    assert changes.get_land_covers_nb() == 1
    assert changes.get_changes_nb() == 232


def test_load_from_two_files(hydro_units_csv: hb.HydroUnits):
    changes = actions.ActionLandCoverChange()
    changes.load_from_csv(
        TEST_FILES_DIR / 'parsing' / 'surface_changes_glacier_ice.csv',
        hydro_units_csv, land_cover='glacier_ice', area_unit='km2',
        match_with='elevation'
    )
    changes.load_from_csv(
        TEST_FILES_DIR / 'parsing' / 'surface_changes_glacier_debris.csv',
        hydro_units_csv, land_cover='glacier_debris', area_unit='km2',
        match_with='elevation'
    )

    assert changes.get_land_covers_nb() == 2
    assert changes.get_changes_nb() == 444


def test_add_action_to_model(hydro_units_csv: hb.HydroUnits):
    changes = actions.ActionLandCoverChange()
    changes.load_from_csv(
        TEST_FILES_DIR / 'parsing' / 'surface_changes_glacier_ice.csv',
        hydro_units_csv, land_cover='glacier_ice', area_unit='km2',
        match_with='elevation'
    )

    model = models.Socont()
    assert model.add_action(changes)


def test_action_correctly_set_in_model(hydro_units_csv: hb.HydroUnits):
    changes = actions.ActionLandCoverChange()
    changes.load_from_csv(
        TEST_FILES_DIR / 'parsing' / 'surface_changes_glacier_ice.csv',
        hydro_units_csv, land_cover='glacier_ice', area_unit='km2',
        match_with='elevation'
    )

    cover_names = ['ground', 'glacier_ice']
    cover_types = ['ground', 'glacier']
    model = models.Socont(land_cover_names=cover_names, land_cover_types=cover_types)

    assert model.add_action(changes)
    assert model.get_actions_nb() == 1
    assert model.get_sporadic_action_items_nb() == 232


def test_action_2_files_correctly_set_in_model(hydro_units_csv: hb.HydroUnits):
    changes = actions.ActionLandCoverChange()
    changes.load_from_csv(
        TEST_FILES_DIR / 'parsing' / 'surface_changes_glacier_ice.csv',
        hydro_units_csv, land_cover='glacier_ice', area_unit='km2',
        match_with='elevation'
    )
    changes.load_from_csv(
        TEST_FILES_DIR / 'parsing' / 'surface_changes_glacier_debris.csv',
        hydro_units_csv, land_cover='glacier_debris', area_unit='km2',
        match_with='elevation'
    )

    cover_names = ['ground', 'glacier_ice', 'glacier_debris']
    cover_types = ['ground', 'glacier', 'glacier']
    model = models.Socont(land_cover_names=cover_names, land_cover_types=cover_types)

    assert model.add_action(changes)
    assert model.get_actions_nb() == 1
    assert model.get_sporadic_action_items_nb() == 444


def test_extract_glacier_cover_evolution_raster(
        catchment_gletsch: hb.Catchment, changes_data: list[str]):
    files = changes_data[0]
    times = changes_data[1]

    # Create the action land cover change object and the corresponding dataframe
    changes, df = actions.ActionLandCoverChange.create_action_for_glaciers(
        catchment_gletsch, times, files, with_debris=False, method='raster',
        interpolate_yearly=False)

    assert changes.get_land_covers_nb() == 1
    df_values = df[0].to_numpy()[:, 1:].astype(np.float32)
    assert changes.get_changes_nb() == np.count_nonzero(~np.isnan(df_values))

    assert len(df) == 2
    assert df[0].shape == (38, 6)
    assert df[1].shape == (38, 6)
    assert df[0].columns[0] == 'hydro_unit'
    assert df[0].columns[1] == '1850-01-01'
    assert df[0].columns[2] == '1931-01-01'
    assert df[0].columns[3] == '1973-01-01'
    assert df[0].columns[4] == '2010-01-01'
    assert df[0].columns[5] == '2016-01-01'

    # The sum by column should be equal to the total area of the catchment
    changes_sum = df[0].sum(axis=0) + df[1].sum(axis=0)
    assert changes_sum.iloc[1] == pytest.approx(catchment_gletsch.area, rel=0.001)
    assert changes_sum.iloc[5] == pytest.approx(catchment_gletsch.area, rel=0.001)


def test_extract_glacier_cover_evolution_vector(
        catchment_gletsch: hb.Catchment, changes_data: list[str]):
    files = changes_data[0]
    times = changes_data[1]

    # Create the action land cover change object and the corresponding dataframe
    changes, df = actions.ActionLandCoverChange.create_action_for_glaciers(
        catchment_gletsch, times, files, with_debris=False, method='vector',
        interpolate_yearly=False)

    assert changes.get_land_covers_nb() == 1
    df_values = df[0].to_numpy()[:, 1:].astype(np.float32)
    assert changes.get_changes_nb() == np.count_nonzero(~np.isnan(df_values))

    assert len(df) == 2
    assert df[0].shape == (38, 6)
    assert df[1].shape == (38, 6)
    assert df[0].columns[0] == 'hydro_unit'
    assert df[0].columns[1] == '1850-01-01'
    assert df[0].columns[2] == '1931-01-01'
    assert df[0].columns[3] == '1973-01-01'
    assert df[0].columns[4] == '2010-01-01'
    assert df[0].columns[5] == '2016-01-01'

    # The sum by column should be equal to the total area of the catchment
    changes_sum = df[0].sum(axis=0) + df[1].sum(axis=0)
    assert changes_sum.iloc[1] == pytest.approx(catchment_gletsch.area, rel=0.001)
    assert changes_sum.iloc[5] == pytest.approx(catchment_gletsch.area, rel=0.001)


def test_extract_glacier_cover_evolution_interpolate(
        catchment_gletsch: hb.Catchment, changes_data: list[str]):
    files = changes_data[0]
    times = changes_data[1]

    # Create the action land cover change object and the corresponding dataframe
    changes, df = actions.ActionLandCoverChange.create_action_for_glaciers(
        catchment_gletsch, times, files, with_debris=False, method='raster',
        interpolate_yearly=True)

    assert changes.get_land_covers_nb() == 1
    df_values = df[0].to_numpy()[:, 1:].astype(np.float32)
    assert changes.get_changes_nb() == np.count_nonzero(~np.isnan(df_values))

    assert len(df) == 2
    assert df[0].shape == (38, 168)
    assert df[1].shape == (38, 168)
    assert df[0].columns[0] == 'hydro_unit'
    assert df[0].columns[1] == '1850-01-01'
    assert df[0].columns[2] == '1851-01-01'
    assert df[0].columns[3] == '1852-01-01'
    assert df[0].columns[4] == '1853-01-01'
    assert df[0].columns[5] == '1854-01-01'

    # The sum by column should be equal to the total area of the catchment
    changes_sum = df[0].sum(axis=0) + df[1].sum(axis=0)
    assert changes_sum.iloc[1] == pytest.approx(catchment_gletsch.area, rel=0.001)
    assert changes_sum.iloc[5] == pytest.approx(catchment_gletsch.area, rel=0.001)
