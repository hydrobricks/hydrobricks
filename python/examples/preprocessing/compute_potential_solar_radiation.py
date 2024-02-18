import os.path
import tempfile
from pathlib import Path

import hydrobricks as hb

# Paths
TEST_FILES_DIR = Path(
    os.path.dirname(os.path.realpath(__file__)),
    '..', '..', '..', 'tests', 'files', 'catchments'
)
CATCHMENT_OUTLINE = TEST_FILES_DIR / 'ch_rhone_gletsch' / 'outline.shp'
CATCHMENT_DEM = TEST_FILES_DIR / 'ch_rhone_gletsch' / 'dem.tif'
CATCHMENT_METEO = TEST_FILES_DIR / 'ch_rhone_gletsch' / 'meteo.csv'

# Create temporary directory
with tempfile.TemporaryDirectory() as tmp_dir_name:
    tmp_dir = tmp_dir_name

os.mkdir(tmp_dir)
working_dir = Path(tmp_dir)

# Prepare catchment data
catchment = hb.Catchment(CATCHMENT_OUTLINE)
catchment.extract_dem(CATCHMENT_DEM)
print("Results were saved in: ", working_dir)

# The annual radiation can then be loaded from the generated file
# catchment.load_mean_annual_radiation_raster(working_dir)

catchment.discretize_by(['elevation', 'radiation'])
catchment.save_unit_ids_raster(working_dir)

forcing = hb.Forcing(catchment.hydro_units)
forcing.load_station_data_from_csv(
    CATCHMENT_METEO, column_time='date', time_format='%d/%m/%Y',
    content={'precipitation': 'precip(mm/day)', 'temperature': 'temp(C)'})
forcing.spatialize_from_station_data(
    variable='precipitation', method='constant')
forcing.spatialize_from_station_data(
    variable='temperature', ref_elevation=2615, gradient=-0.6)
forcing.spatialize_from_gridded_data(
    variable='solar_radiation', var_name='radiation', path=working_dir,
    file_pattern='daily_potential_radiation.nc', data_crs=2056,
    dim_x='x', dim_y='y', raster_hydro_units=working_dir / 'unit_ids.tif')

# Save forcing to a netcdf file
forcing.save_as(working_dir / 'forcing.nc')
print(f"File saved to: {working_dir / 'forcing.nc'}")
