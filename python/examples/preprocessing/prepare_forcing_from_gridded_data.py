import os.path
import tempfile
from pathlib import Path

import hydrobricks as hb

# Paths
TEST_FILES_DIR = Path(
    os.path.dirname(os.path.realpath(__file__)),
    '..', '..', '..', 'tests', 'files', 'catchments'
)
CATCHMENT_OUTLINE = TEST_FILES_DIR / 'ch_sitter_appenzell' / 'outline.shp'
CATCHMENT_DEM = TEST_FILES_DIR / 'ch_sitter_appenzell' / 'dem.tif'
FORCING_DIR = R'D:\Data\Time_series\Switzerland\Meteorology\MeteoSwiss_gridded_products'

# Create temporary directory
with tempfile.TemporaryDirectory() as tmp_dir_name:
    tmp_dir = tmp_dir_name

os.mkdir(tmp_dir)
working_dir = Path(tmp_dir)

# Prepare catchment data
catchment = hb.Catchment(CATCHMENT_OUTLINE)
catchment.extract_dem(CATCHMENT_DEM)

# Create elevation bands
bands = catchment.create_elevation_bands(method='isohypse', distance=50)

# Save elevation bands to a raster
catchment.save_unit_ids_raster(working_dir / 'unit_ids.tif')

# Create the hydro units
hydro_units = hb.HydroUnits(data=bands)

# Create forcing object and spatialize from gridded data
forcing = hb.Forcing(hydro_units)
forcing.spatialize_from_gridded_data(
    variable='precipitation', path=(Path(FORCING_DIR) / 'RhiresD_v2.0_swiss.lv95'),
    file_pattern='RhiresD_ch01h.swiss.lv95_*.nc', data_crs=2056, var_name='RhiresD',
    dim_x='E', dim_y='N', raster_hydro_units=working_dir / 'unit_ids.tif')
forcing.spatialize_from_gridded_data(
    variable='temperature', path=(Path(FORCING_DIR) / 'TabsD_v2.0_swiss.lv95'),
    file_pattern='TabsD_ch01r.swiss.lv95_*.nc', data_crs=2056, var_name='TabsD',
    dim_x='E', dim_y='N', raster_hydro_units=working_dir / 'unit_ids.tif')
forcing.compute_pet(method='Hamon', use=['t', 'lat'])

# Save forcing to a netcdf file
forcing.save_as(working_dir / 'forcing.nc')

print(f"File saved to: {working_dir / 'forcing.nc'}")
