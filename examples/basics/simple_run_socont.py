import os.path
import tempfile
import uuid
from pathlib import Path

import hydrobricks as hb
import hydrobricks.models as models

# Paths
TEST_FILES_DIR = Path(
    os.path.dirname(os.path.realpath(__file__)),
    '..', '..', 'tests', 'files', 'catchments'
)
CATCHMENT_BANDS = TEST_FILES_DIR / 'ch_sitter_appenzell' / 'hydro_units_elevation.csv'
CATCHMENT_METEO = TEST_FILES_DIR / 'ch_sitter_appenzell' / 'meteo.csv'
CATCHMENT_DISCHARGE = TEST_FILES_DIR / 'ch_sitter_appenzell' / 'discharge.csv'
CATCHMENT_RASTER = TEST_FILES_DIR / 'ch_sitter_appenzell' / 'unit_ids.tif'
DEM_RASTER = TEST_FILES_DIR / 'ch_sitter_appenzell' / 'dem.tif'

working_dir = Path(tempfile.gettempdir()) / f"tmp_{uuid.uuid4().hex}"
working_dir.mkdir(parents=True, exist_ok=True)

# Model structure
socont = models.Socont(
    soil_storage_nb=2,
    surface_runoff="linear_storage",
    record_all=True
)

# Parameters
parameters = socont.generate_parameters()
parameters.set_values({
    'A': 458,
    'a_snow': 2,
    'k_slow_1': 0.9,
    'k_slow_2': 0.8,
    'k_quick': 1,
    'percol': 9.8
})

# Hydro units
hydro_units = hb.HydroUnits()
hydro_units.load_from_csv(
    CATCHMENT_BANDS,
    column_elevation='elevation',
    column_area='area'
)

# Meteo data
ref_elevation = 1253  # Reference altitude for the meteo data
forcing = hb.Forcing(hydro_units)
forcing.load_station_data_from_csv(
    CATCHMENT_METEO,
    column_time='date',
    time_format='%d/%m/%Y',
    content={'precipitation': 'precip(mm/day)', 'temperature': 'temp(C)'}
)

forcing.spatialize_from_station_data(
    variable='temperature',
    ref_elevation=ref_elevation,
    gradient=-0.6
)
forcing.correct_station_data(
    variable='precipitation',
    correction_factor=0.75
)
forcing.spatialize_from_station_data(
    variable='precipitation',
    ref_elevation=ref_elevation,
    gradient=0.05
)
forcing.compute_pet(
    method='Hamon',
    use=['t', 'lat'],
    lat=47.3
)

# Obs data
obs = hb.Observations()
obs.load_from_csv(
    CATCHMENT_DISCHARGE,
    column_time='Date',
    time_format='%d/%m/%Y',
    content={'discharge': 'Discharge (mm/d)'}
)

# Model setup
socont.setup(
    spatial_structure=hydro_units,
    output_path=str(working_dir),
    start_date='1981-01-01',
    end_date='2020-12-31'
)

# Initialize and run the model
socont.initialize_state_variables(
    parameters=parameters,
    forcing=forcing
)
socont.run(
    parameters=parameters,
    forcing=forcing
)

# Get outlet discharge time series
sim_ts = socont.get_outlet_discharge()

# Evaluate
obs_ts = obs.data[0]
nse = socont.eval('nse', obs_ts)
kge_2012 = socont.eval('kge_2012', obs_ts)

print(f"NSE = {nse:.3f}, KGE = {kge_2012:.3f}")

# Compute reference metric
ref_nse = obs.compute_reference_metric('nse')
ref_kge = obs.compute_reference_metric('kge_2012')

print(f"ref NSE = {ref_nse:.3f}, ref KGE = {ref_kge:.3f}")

if not hb.has_matplotlib:
    print("Matplotlib is not installed, the plotting functions will not work.")
    exit()

# Plot
hb.plotting.plot_hydrograph(obs_ts, sim_ts, obs.time, year=2020)

# Dump all outputs
socont.dump_outputs(str(working_dir))

# Load the netcdf file
results = hb.Results(str(working_dir) + '/results.nc')

# List the hydro units components available
results.list_hydro_units_components()

# Plot the snow water equivalent on a map
hb.plotting.plot_map_hydro_unit_value(
    results,
    CATCHMENT_RASTER,
    "ground_snowpack:snow",
    "1981-01-20",
    dem_path=DEM_RASTER,
    max_val=300
)

# Create an animated map of the snow water equivalent
hb.plotting.create_animated_map_hydro_unit_value(
    results, CATCHMENT_RASTER,
    "ground_snowpack:snow",
    "1990-01-01",
    "1990-03-20",
    save_path=str(working_dir),
    dem_path=DEM_RASTER,
    max_val=300
)

# Create an animated map of the slow reservoir 2 content
hb.plotting.create_animated_map_hydro_unit_value(
    results,
    CATCHMENT_RASTER,
    "slow_reservoir_2:content",
    "2020-01-20",
    "2020-03-20",
    save_path=str(working_dir),
    dem_path=DEM_RASTER
)
