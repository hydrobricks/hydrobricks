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

# Create temporary directory
with tempfile.TemporaryDirectory() as tmp_dir_name:
    tmp_dir = tmp_dir_name

os.mkdir(tmp_dir)
working_dir = Path(tmp_dir)

# Prepare catchment data
catchment = hb.Catchment(CATCHMENT_OUTLINE)
catchment.extract_dem(CATCHMENT_DEM)

# Create elevation bands
catchment.create_elevation_bands(method='isohypse', distance=50)






glacier_path = f'D:/Projects/Hydrobricks/Swiss glaciers extents/'

whole_glaciers = [f'{glacier_path}inventory_sgi1850_r1992/SGI_1850.shp',
                  f'{glacier_path}inventory_sgi1931_r2022/SGI_1931.shp',
                  f'{glacier_path}inventory_sgi1973_r1976/SGI_1973.shp',
                  f'{glacier_path}inventory_sgi2010_r2010/SGI_2010.shp',
                  f'{glacier_path}inventory_sgi2016_r2020/SGI_2016_glaciers.shp']

debris_glaciers = [None, None, None, None,
                   f'{glacier_path}inventory_sgi2016_r2020/SGI_2016_debriscover.shp']

times = ['01/01/1850', '01/01/1931', '01/01/1973', '01/01/2010', '01/01/2016']

with_debris = True




whole_glaciers = [f'{glacier_path}inventory_sgi2016_r2020/SGI_2016_glaciers.shp']

debris_glaciers = [f'{glacier_path}inventory_sgi2016_r2020/SGI_2016_debriscover.shp']

times = ['01/01/2016']


changes = catchment.create_behaviour_land_cover_change_for_glaciers(
    whole_glaciers, debris_glaciers,
    times, with_debris)





# Create elevation bands
catchment.discretize_by(criteria=['elevation', 'aspect'], elevation_method='isohypse',
                        elevation_distance=100)

# Save elevation bands to a raster
catchment.save_unit_ids_raster(working_dir / 'unit_ids.tif')

# Save the elevation band properties to a csv file
catchment.save_hydro_units_to_csv(working_dir / 'bands.csv')

print(f"Files saved to: {working_dir}")
