import os.path
import tempfile
import uuid
from datetime import datetime
from io import BytesIO
from pathlib import Path

import imageio
import matplotlib.pyplot as plt
import numpy as np
from matplotlib.colors import ListedColormap

import hydrobricks as hb
from hydrobricks.constants import TO_RAD

# Paths
TEST_FILES_DIR = Path(
    os.path.dirname(os.path.realpath(__file__)),
    '..', '..', 'tests', 'files', 'catchments'
)
CATCHMENT_DEM = TEST_FILES_DIR / 'ch_sitter_stgallen' / 'dem.tif'

# Create temporary directory
working_dir = Path(tempfile.gettempdir()) / f"tmp_{uuid.uuid4().hex}"
working_dir.mkdir(parents=True, exist_ok=True)

# Options
date_string = '2009-08-06'
steps_per_hour = 2
resolution = None

# Prepare catchment data
catchment = hb.Catchment()
catchment.extract_dem(CATCHMENT_DEM)
hillshade = catchment.get_hillshade()

dem, masked_dem_data, slope, aspect = (
    catchment.resample_dem_and_calculate_slope_aspect(
        resolution,
        working_dir
    )
)

lat, _ = catchment.get_dem_mean_lat_lon()
lat_rad = lat * TO_RAD

# Date
target_date = datetime.strptime(date_string, '%Y-%m-%d')
day_of_year = target_date.timetuple().tm_yday

# Compute the solar declination
solar_declin = catchment.get_solar_declination_rad(day_of_year)

# Compute the zenith and azimuth
ha_limit = catchment.get_solar_hour_angle_limit(solar_declin, lat_rad)
time_interval = (15 / steps_per_hour) * TO_RAD
ha_list = np.arange(-ha_limit, ha_limit + time_interval, time_interval)
zenith_list = catchment.get_solar_zenith(ha_list, lat_rad, solar_declin)
azimuth_list = catchment.get_solar_azimuth_to_south(ha_list, lat_rad, solar_declin)

# Create a colormap for the shadows
cmap_shadow = ListedColormap([(0, 0, 0, 0), (0, 0, 0, 1)])
cmap_black = ListedColormap([(0, 0, 0, 1)])

# Initialize a list to store the frames
frames = []

# Loop over the time steps
for i, (zenith, azimuth) in enumerate(zip(zenith_list, azimuth_list)):
    shadows = catchment.calculate_cast_shadows(
        dem,
        masked_dem_data,
        zenith,
        azimuth
    )

    # Create a new figure and plot the data
    plt.figure()
    plt.imshow(hillshade, cmap='gray')
    if np.any(shadows == 0):
        plt.imshow(shadows, cmap=cmap_shadow, alpha=0.8)
    else:
        plt.imshow(shadows, cmap=cmap_black, alpha=0.8)
    plt.title(f'Cast shadow ({i + 1})')
    plt.tight_layout()

    # Instead of showing the figure, save it to a BytesIO object
    buf = BytesIO()
    plt.savefig(buf, format='png')
    buf.seek(0)
    frames.append(imageio.v3.imread(buf))
    plt.close()

# Save frames as an animated GIF
imageio.mimsave(
    'animated_shadows.gif',
    frames,
    'GIF',
    duration=500,
    loop=0
)
