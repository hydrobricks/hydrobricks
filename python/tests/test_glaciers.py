import os.path
import tempfile
import pandas as pd
import numpy as np
from pathlib import Path


import hydrobricks as hb

TEST_FILES_DIR = Path(
    os.path.dirname(os.path.realpath(__file__)),
    '..', '..', 'tests', 'files', 'catchments'
)
HYDRO_UNITS_SYNTH = TEST_FILES_DIR / 'synthetic_delta_h' / 'hydro_units_100m.csv'
GLACIER_PROFILE_SYNTH = TEST_FILES_DIR / 'synthetic_delta_h' / 'glacier_profile_id_100m.csv'


def test_glacier_evolution_delta_h_lookup_table():
    with tempfile.TemporaryDirectory() as tmp_dir:
        working_dir = Path(tmp_dir)

        # Hydro units
        hydro_units = hb.HydroUnits()
        hydro_units.load_from_csv(
            HYDRO_UNITS_SYNTH, column_elevation='elevation', column_area='area')

        # Glacier evolution
        glacier_evolution = hb.preprocessing.GlacierEvolutionDeltaH(hydro_units)
        # Compute the lookup table. In Seibert et al. (2018), the glacier width is not updated
        # during the iterations (update_width=False), but we would recommend to do so.
        glacier_evolution.compute_lookup_table(GLACIER_PROFILE_SYNTH, update_width=False)
        glacier_evolution.save_as_csv(working_dir)
        lookup_table = glacier_evolution.lookup_table_area

        # Load reference results as numpy array
        ref_file = TEST_FILES_DIR / 'synthetic_delta_h' / 'lookup_table_100m.csv'
        ref_lookup_table = np.loadtxt(ref_file, delimiter=',')

        assert np.allclose(ref_lookup_table, lookup_table)
