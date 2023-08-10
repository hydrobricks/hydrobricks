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

with tempfile.TemporaryDirectory() as tmp_dir_name:
    tmp_dir = tmp_dir_name

os.mkdir(tmp_dir)
working_dir = Path(tmp_dir)

# Model structure
socont = models.Socont(soil_storage_nb=2, surface_runoff="linear_storage",
                       record_all=False)

# Parameters
parameters = socont.generate_parameters()
parameters.set_values({'A': 458, 'a_snow': 1.8, 'k_slow_1': 0.9, 'k_slow_2': 0.8,
                       'k_quick': 1, 'percol': 9.8})

# Hydro units
hydro_units = hb.HydroUnits()
hydro_units.load_from_csv(
    CATCHMENT_BANDS, area_unit='m2', column_elevation='elevation',
    column_area='area')

# Meteo data
ref_elevation = 1250  # Reference altitude for the meteo data
forcing = hb.Forcing(hydro_units)
forcing.load_from_csv(
    CATCHMENT_METEO, column_time='Date', time_format='%d/%m/%Y',
    content={'precipitation': 'precip(mm/day)', 'temperature': 'temp(C)',
             'pet': 'pet_sim(mm/day)'})
forcing.spatialize_temperature(ref_elevation, -0.6)
forcing.spatialize_pet()
forcing.spatialize_precipitation(ref_elevation=ref_elevation, gradient=0.05,
                                 correction_factor=0.75)

# Obs data
obs = hb.Observations()
obs.load_from_csv(CATCHMENT_DISCHARGE, column_time='Date', time_format='%d/%m/%Y',
                  content={'discharge': 'Discharge (mm/d)'})

# Model setup
socont.setup(spatial_structure=hydro_units, output_path=str(working_dir),
             start_date='1981-01-01', end_date='2020-12-31')

# Initialize and run the model
socont.initialize_state_variables(parameters=parameters, forcing=forcing)
socont.run(parameters=parameters, forcing=forcing)

# Get outlet discharge time series
sim_ts = socont.get_outlet_discharge()

# Evaluate
obs_ts = obs.data_raw[0]
nse = socont.eval('nse', obs_ts)
kge_2012 = socont.eval('kge_2012', obs_ts)

print(f"nse = {nse:.3f}, kge_2012 = {kge_2012:.3f}")
