import os.path
import tempfile
import pytest
from pathlib import Path

import numpy as np

import hydrobricks as hb
import hydrobricks.actions as actions

TEST_FILES_DIR = Path(
    os.path.dirname(os.path.realpath(__file__)),
    '..', '..', 'tests', 'files', 'catchments'
)
HYDRO_UNITS_SYNTH = TEST_FILES_DIR / 'synthetic_delta_h' / 'hydro_units_100m.csv'
GLACIER_PROFILE_SYNTH = TEST_FILES_DIR / 'synthetic_delta_h' / 'glacier_profile_id_100m.csv'

CATCHMENT_OUTLINE = TEST_FILES_DIR / 'ch_rhone_gletsch' / 'outline.shp'
CATCHMENT_DEM = TEST_FILES_DIR / 'ch_rhone_gletsch' / 'dem.tif'
GLACIER_OUTLINE = TEST_FILES_DIR / 'ch_rhone_gletsch' / 'glaciers' / 'sgi_2016.shp'
GLACIER_ICE_THICKNESS = TEST_FILES_DIR / 'ch_rhone_gletsch' / 'glaciers' / 'ice_thickness.tif'


def test_glacier_evolution_delta_h_lookup_table():
    with tempfile.TemporaryDirectory() as tmp_dir:
        working_dir = Path(tmp_dir)

        # Hydro units
        hydro_units = hb.HydroUnits()
        hydro_units.load_from_csv(
            HYDRO_UNITS_SYNTH, column_elevation='elevation', column_area='area')

        # Glacier evolution
        glacier_evolution = hb.preprocessing.GlacierEvolutionDeltaH(hydro_units)
        # In Seibert et al. (2018), the glacier width is not updated during the
        # iterations (update_width=False), but we would recommend to do so.
        glacier_evolution.compute_lookup_table(GLACIER_PROFILE_SYNTH,
                                               update_width=False)
        glacier_evolution.save_as_csv(working_dir)
        lookup_table = glacier_evolution.lookup_table_area

        # Load reference results as numpy array
        ref_file = TEST_FILES_DIR / 'synthetic_delta_h' / 'lookup_table_100m.csv'
        ref_lookup_table = np.loadtxt(ref_file, delimiter=',')

        assert np.allclose(ref_lookup_table, lookup_table)


def test_glacier_initial_ice_thickness_computation():
    if not hb.has_rasterio:
        return

    with tempfile.TemporaryDirectory() as tmp_dir:
        # Loading tiff file to numpy array
        with hb.rasterio.open(GLACIER_ICE_THICKNESS) as src:
            # Read the data into a NumPy array
            thickness_data = src.read(1)

        # Total volume of the glacier
        volume_tot = float(np.sum(thickness_data[thickness_data > 0])) * 100

        # Prepare catchment data
        catchment = hb.Catchment(CATCHMENT_OUTLINE,
                                 land_cover_types=['ground', 'glacier'],
                                 land_cover_names=['ground', 'glacier'])
        catchment.extract_dem(CATCHMENT_DEM)

        # Create elevation bands
        catchment.create_elevation_bands(method='equal_intervals', distance=100)

        # Glacier evolution
        glacier_evolution = hb.preprocessing.GlacierEvolutionDeltaH()
        glacier_df = glacier_evolution.compute_initial_ice_thickness(
            catchment, ice_thickness=GLACIER_ICE_THICKNESS)

        assert glacier_df is not None

        # Check if the initial ice thickness is correct
        volume_df = float((
            glacier_df[('glacier_thickness', 'm')] * glacier_df[('glacier_area', 'm2')]
        ).sum())

        assert volume_df == pytest.approx(volume_tot, rel=0.0005), \
            (f"Volume of the glacier is not correct. Expected: {volume_tot}, "
             f"got: {volume_df}")


def test_delta_h_action_lookup_table_binding():
    # Hydro units
    hydro_units = hb.HydroUnits()
    hydro_units.load_from_csv(
        HYDRO_UNITS_SYNTH, column_elevation='elevation', column_area='area')

    # Glacier evolution preprocessing
    glacier_evolution = hb.preprocessing.GlacierEvolutionDeltaH(hydro_units)
    glacier_evolution.compute_lookup_table(GLACIER_PROFILE_SYNTH,
                                           update_width=False)
    lookup_table_area = glacier_evolution.get_lookup_table_area()
    lookup_table_volume = glacier_evolution.get_lookup_table_volume()
    hydro_unit_ids = lookup_table_area.columns

    # Corresponding action
    action = actions.ActionGlacierEvolutionDeltaH()
    action.load_from(glacier_evolution)

    assert action.get_month() == 10
    assert action.get_land_cover_name() == 'glacier'
    assert np.array_equal(action.get_hydro_unit_ids(), hydro_unit_ids.values)
    assert np.array_equal(action.get_lookup_table_area(), lookup_table_area.values)
    assert np.array_equal(action.get_lookup_table_volume(), lookup_table_volume.values)


def test_delta_h_action_lookup_table_binding_from_file():
    with tempfile.TemporaryDirectory() as tmp_dir:
        working_dir = Path(tmp_dir)

        # Hydro units
        hydro_units = hb.HydroUnits()
        hydro_units.load_from_csv(
            HYDRO_UNITS_SYNTH, column_elevation='elevation', column_area='area')

        # Glacier evolution preprocessing
        glacier_evolution = hb.preprocessing.GlacierEvolutionDeltaH(hydro_units)
        glacier_evolution.compute_lookup_table(GLACIER_PROFILE_SYNTH,
                                               update_width=False)
        glacier_evolution.save_as_csv(working_dir)

        lookup_table_area = glacier_evolution.get_lookup_table_area()
        lookup_table_volume = glacier_evolution.get_lookup_table_volume()
        hydro_unit_ids = lookup_table_area.columns

        # Corresponding action
        action = actions.ActionGlacierEvolutionDeltaH()
        action.load_from_csv(working_dir)

        assert action.get_month() == 10
        assert action.get_land_cover_name() == 'glacier'
        assert action.get_hydro_unit_ids().shape == hydro_unit_ids.values.shape
        assert np.array_equal(action.get_hydro_unit_ids(), hydro_unit_ids.values)
        assert action.get_lookup_table_area().shape == lookup_table_area.values.shape
        assert np.allclose(action.get_lookup_table_area(), lookup_table_area.values)
        assert action.get_lookup_table_volume().shape == lookup_table_volume.values.shape
        assert np.allclose(action.get_lookup_table_volume(), lookup_table_volume.values)
