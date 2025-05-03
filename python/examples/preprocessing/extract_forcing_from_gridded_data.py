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
CATCHMENT_OUTLINE = TEST_FILES_DIR / 'ch_sitter_appenzell' / 'outline.shp'
CATCHMENT_DEM = TEST_FILES_DIR / 'ch_sitter_appenzell' / 'dem.tif'
FORCING_DIR = 'path/to/MeteoSwiss_gridded_products'

# Create temporary directory
with tempfile.TemporaryDirectory() as tmp_dir_name:
    tmp_dir = tmp_dir_name

os.mkdir(tmp_dir)
working_dir = Path(tmp_dir)

# Prepare catchment data
catchment = hb.Catchment(CATCHMENT_OUTLINE)
catchment.extract_raster(CATCHMENT_DEM)

# Create elevation bands
catchment.create_elevation_bands(method='equal_intervals', distance=50)

# Save elevation bands to a raster
catchment.save_unit_ids_raster(working_dir)

# Create forcing object and spatialize from gridded data
forcing = hb.Forcing(catchment.hydro_units)
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

# The forcing object can also be directly used to run a model:
socont = models.Socont(soil_storage_nb=2, surface_runoff="linear_storage",
                       record_all=False)
parameters = socont.generate_parameters()
parameters.set_values({'A': 458, 'a_snow': 2, 'k_slow_1': 0.9, 'k_slow_2': 0.8,
                       'k_quick': 1, 'percol': 9.8})
socont.setup(spatial_structure=catchment.hydro_units, output_path=str(working_dir),
             start_date='2015-01-01', end_date='2017-12-31')
socont.run(parameters=parameters, forcing=forcing)
