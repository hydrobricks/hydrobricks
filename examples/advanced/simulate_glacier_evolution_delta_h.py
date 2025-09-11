import os.path
import tempfile
import uuid
from pathlib import Path

import matplotlib.pyplot as plt

import hydrobricks as hb
import hydrobricks.models as models

# Paths
TEST_FILES_DIR = Path(
    os.path.dirname(os.path.realpath(__file__)),
    '..', '..', 'tests', 'files', 'catchments'
)
CATCHMENT_PATH = TEST_FILES_DIR / 'ch_rhone_gletsch'
CATCHMENT_OUTLINE = CATCHMENT_PATH / 'outline.shp'
CATCHMENT_DEM = CATCHMENT_PATH / 'dem.tif'
GLACIER_ICE_THICKNESS = CATCHMENT_PATH / 'glaciers' / 'ice_thickness.tif'
CATCHMENT_METEO = CATCHMENT_PATH / 'meteo.csv'
CATCHMENT_DISCHARGE = CATCHMENT_PATH / 'discharge.csv'

# Create temporary directory
working_dir = Path(tempfile.gettempdir()) / f"tmp_{uuid.uuid4().hex}"
working_dir.mkdir(parents=True, exist_ok=True)

# Prepare catchment data
catchment = hb.Catchment(
    CATCHMENT_OUTLINE,
    land_cover_types=['ground', 'glacier'],
    land_cover_names=['ground', 'glacier']
)
catchment.extract_dem(CATCHMENT_DEM)

# Create elevation bands
catchment.create_elevation_bands(
    method='equal_intervals',
    distance=100
)

# Connectivity for snow redistribution
connectivity = catchment.calculate_connectivity(mode='multiple')
catchment.hydro_units.set_connectivity(connectivity)

# Glacier evolution. This is for demonstration purposes only as the glacier
# volume/extent from 2016 is used for the 1981 initial conditions!
glacier_evolution = hb.preprocessing.GlacierEvolutionDeltaH()
glacier_df = glacier_evolution.compute_initial_ice_thickness(
    catchment,
    ice_thickness=GLACIER_ICE_THICKNESS
)

# It can then optionally be saved as a csv file
glacier_df.to_csv(
    working_dir / 'glacier_profile.csv',
    index=False
)

# The lookup table can be computed and saved as a csv file
glacier_evolution.compute_lookup_table(catchment)
glacier_evolution.save_as_csv(working_dir)

# Create the action glacier evolution object
changes = hb.actions.ActionGlacierEvolutionDeltaH()
changes.load_from(
    glacier_evolution,
    land_cover='glacier',
    update_month='October'
)

# Create the snow to ice transformation action (transform all remaining snow to ice)
snow_to_ice = hb.actions.ActionGlacierSnowToIceTransformation('September', 30, 'glacier')

# Model structure with specific options
socont = models.Socont(
    soil_storage_nb=2,
    surface_runoff="linear_storage",
    record_all=True,
    glacier_infinite_storage=0,
    snow_ice_transformation="transform:snow_ice_swat",
    snow_redistribution='transport:snow_slide',
    land_cover_types=['ground', 'glacier'],
    land_cover_names=['ground', 'glacier']
)

# Parameters
parameters = socont.generate_parameters()
parameters.set_values({
    'A': 458,
    'a_snow': 4,
    'k_slow_1': 0.9,
    'k_slow_2': 0.8,
    'k_quick': 1,
    'percol': 9.8,
    'a_ice': 8,
    'k_ice': 0.5,
    'k_snow': 0.1,
    'snow_ice_basal_acc_coeff': 0.0014
})

# Meteo data
ref_elevation = 2702  # Reference altitude for the meteo data
forcing = hb.Forcing(catchment.hydro_units)
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

# Model setup
socont.setup(
    spatial_structure=catchment.hydro_units,
    output_path=str(working_dir),
    start_date='1981-01-01',
    end_date='2020-12-31'
)

# Add the actions to the model
socont.add_action(changes)
socont.add_action(snow_to_ice)

# Initialize and run the model
socont.run(
    parameters=parameters,
    forcing=forcing
)

# Dump all outputs
socont.dump_outputs(str(working_dir))

# Load the netcdf file
results = hb.Results(str(working_dir) + '/results.nc')

glacier_fractions = results.results.land_cover_fractions[1, :, :]
hydro_units_areas = results.results.hydro_units_areas
glacier_areas = hydro_units_areas * glacier_fractions
glacier_area = glacier_areas.sum(axis=0)

# Plot glacier area. This is for demonstration purposes only as the glacier
# volume/extent from 2016 is used for the 1981 initial conditions!
plt.plot(results.results.time, glacier_area)
plt.title('Glacier area evolution')
plt.ylim(bottom=0)
plt.tight_layout()
plt.show()

# Plot glacier ice content
ice_we = results.get_hydro_units_values(
    component='glacier:ice_content'
)
ice_volume = glacier_areas * ice_we / 1000
for i in range(ice_volume.shape[0]):
    plt.plot(results.results.time, ice_volume[i, :], alpha=0.6)
plt.title('Glacier ice volume evolution per hydro unit')
plt.tight_layout()
plt.show()

# Plot the SWE on the glacier parts
swe = results.get_hydro_units_values(
    component='glacier_snowpack:snow_content'
)
for i in range(swe.shape[0]):
    plt.plot(results.results.time, swe[i, :], alpha=0.6)
plt.title('Glacier SWE evolution per hydro unit')
plt.tight_layout()
plt.show()

# Plot the SWE on the non glacier parts
swe = results.get_hydro_units_values(
    component='ground_snowpack:snow_content'
)
for i in range(swe.shape[0]):
    plt.plot(results.results.time, swe[i, :], alpha=0.6)
plt.title('Non glacier SWE evolution per hydro unit')
plt.tight_layout()
plt.show()
