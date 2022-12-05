import os.path
import tempfile
from pathlib import Path

import hydrobricks as hb
import hydrobricks.models as models
import matplotlib.pyplot as plt

# Paths
TEST_FILES_DIR = Path(
    os.path.dirname(os.path.realpath(__file__)),
    '..', '..', 'tests', 'files', 'catchments'
)
CATCHMENT_BANDS = TEST_FILES_DIR / 'ch_sitter_appenzell' / 'elevation_bands.csv'
CATCHMENT_METEO = TEST_FILES_DIR / 'ch_sitter_appenzell' / 'meteo.csv'
CATCHMENT_DISCHARGE = TEST_FILES_DIR / 'ch_sitter_appenzell' / 'discharge.csv'

tmp_dir = tempfile.TemporaryDirectory()
working_dir = Path(tmp_dir.name)

# Model structure
socont = models.Socont(soil_storage_nb=2, surface_runoff="linear_storage",
                       record_all=False)

# Parameters
parameters = socont.generate_parameters()
parameters.add_data_parameter('precip_corr_factor', 1, min_value=0.7, max_value=1.3)
parameters.add_data_parameter('precip_gradient', 0.05, min_value=0, max_value=0.2)
parameters.add_data_parameter('temp_gradients', -0.6, min_value=-1, max_value=0)

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

forcing.define_spatialization(
    variable='temperature', method='additive_elevation_gradient',
    ref_elevation=ref_elevation, gradient='param:temp_gradients')

forcing.define_spatialization(variable='pet', method='constant')

forcing.define_spatialization(
    variable='precipitation', method='multiplicative_elevation_gradient',
    ref_elevation=ref_elevation, gradient='param:precip_gradient',
    correction_factor='param:precip_corr_factor'
)

# Obs data
obs = hb.Observations()
obs.load_from_csv(CATCHMENT_DISCHARGE, column_time='Date', time_format='%d/%m/%Y',
                  content={'discharge': 'Discharge (mm/d)'})

socont.setup(spatial_structure=hydro_units, output_path=working_dir,
             start_date='1981-01-01', end_date='2020-12-31')

# Select the parameters to optimize/analyze
parameters_list = ['a_snow', 'k_quick', 'A', 'k_slow_1', 'percol', 'k_slow_2',
                   'precip_corr_factor']

# Proceed to the Monte Carlo analysis
mc_analysis = socont.analyze(
    method='monte_carlo', metrics=['kge_2012', 'nse', 'me'], forcing=forcing,
    parameters=parameters, nb_runs=200, observations=obs.data_raw[0],
    parameters_to_assess=parameters_list)

# Plot
for param in parameters_list:
    mc_analysis.plot.scatter(x=param, y='nse')
    plt.ylim(0, 1)
    plt.title(param)
    plt.tight_layout()
    plt.show()

# Cleanup
socont.cleanup()
try:
    tmp_dir.cleanup()
except Exception:
    print('Could not remove temporary directory.')
