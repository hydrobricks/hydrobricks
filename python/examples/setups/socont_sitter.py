import os.path
import tempfile
from pathlib import Path

import hydrobricks as hb
import hydrobricks.models as models

# Paths
TEST_FILES_DIR = Path(
    os.path.dirname(os.path.realpath(__file__)),
    '..', '..', '..', 'tests', 'files', 'catchments'
)
CATCHMENT_BANDS = TEST_FILES_DIR / 'ch_sitter_appenzell' / 'elevation_bands.csv'
CATCHMENT_METEO = TEST_FILES_DIR / 'ch_sitter_appenzell' / 'meteo.csv'
CATCHMENT_DISCHARGE = TEST_FILES_DIR / 'ch_sitter_appenzell' / 'discharge.csv'

use_precip_gradient = globals().get('USE_PRECIP_GRADIENT', False)
use_pyet = globals().get('USE_PYET', False)


with tempfile.TemporaryDirectory() as tmp_dir_name:
    tmp_dir = tmp_dir_name

os.mkdir(tmp_dir)
working_dir = Path(tmp_dir)

# Model structure
socont = models.Socont(soil_storage_nb=2, surface_runoff="linear_storage",
                       record_all=False)

# Parameters
parameters = socont.generate_parameters()
parameters.define_constraint('k_slow_2', '<', 'k_slow_1')
parameters.add_data_parameter('precip_corr_factor', 0.85, min_value=0.7, max_value=1.3)
parameters.add_data_parameter('temp_gradients', -0.6, min_value=-1, max_value=0)
if use_precip_gradient:
    parameters.add_data_parameter('precip_gradient', 0.05, min_value=0, max_value=0.2)

# Hydro units
hydro_units = hb.HydroUnits()
hydro_units.load_from_csv(
    CATCHMENT_BANDS, area_unit='m2', column_elevation='elevation',
    column_area='area')

# Meteo data
ref_elevation = 1250  # Reference altitude for the meteo data
forcing = hb.Forcing(hydro_units)
forcing.load_station_data_from_csv(
    CATCHMENT_METEO, column_time='Date', time_format='%d/%m/%Y',
    content={'precipitation': 'precip(mm/day)', 'temperature': 'temp(C)',
             'pet': 'pet_sim(mm/day)'})

forcing.spatialize_from_station_data(
    variable='temperature', ref_elevation=ref_elevation,
    gradient='param:temp_gradients')

if use_pyet:
    forcing.compute_pet(method='priestley_taylor')
else:
    forcing.spatialize_from_station_data(variable='pet', method='constant')

forcing.correct_station_data(
    variable='precipitation', correction_factor='param:precip_corr_factor'
)
if use_precip_gradient:
    forcing.spatialize_from_station_data(
        variable='precipitation', ref_elevation=ref_elevation,
        gradient='param:precip_gradient',
    )
else:
    forcing.spatialize_from_station_data(
        variable='precipitation', method='constant'
    )

# Obs data
obs = hb.Observations()
obs.load_from_csv(CATCHMENT_DISCHARGE, column_time='Date', time_format='%d/%m/%Y',
                  content={'discharge': 'Discharge (mm/d)'})

socont.setup(spatial_structure=hydro_units, output_path=str(working_dir),
             start_date='1981-01-01', end_date='2020-12-31')
