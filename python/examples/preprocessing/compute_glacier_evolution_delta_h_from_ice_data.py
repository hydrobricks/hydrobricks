import os.path
import tempfile
import numpy as np
import pandas as pd
from matplotlib import pyplot as plt
from pathlib import Path

import hydrobricks as hb
from hydrobricks.constants import ICE_WE

# Paths
TEST_FILES_DIR = Path(
    os.path.dirname(os.path.realpath(__file__)),
    '..', '..', '..', 'tests', 'files', 'catchments'
)
CATCHMENT_OUTLINE = TEST_FILES_DIR / 'ch_rhone_gletsch' / 'outline.shp'
CATCHMENT_DEM = TEST_FILES_DIR / 'ch_rhone_gletsch' / 'dem.tif'
GLACIER_ICE_THICKNESS = TEST_FILES_DIR / 'ch_rhone_gletsch' / 'glaciers' / 'ice_thickness.tif'

# Create temporary directory
with tempfile.TemporaryDirectory() as tmp_dir_name:
    tmp_dir = tmp_dir_name

os.mkdir(tmp_dir)
working_dir = Path(tmp_dir)

# Prepare catchment data
catchment = hb.Catchment(CATCHMENT_OUTLINE, land_cover_types=['ground', 'glacier'],
                         land_cover_names=['ground', 'glacier'])
catchment.extract_dem(CATCHMENT_DEM)

# Create elevation bands
if True:
    catchment.create_elevation_bands(method='equal_intervals', distance=100)
else:
    catchment.calculate_daily_potential_radiation(TEST_FILES_DIR, resolution=None)
    catchment.discretize_by(['elevation', 'radiation'],
                             elevation_method='equal_intervals', 
                             elevation_distance=100,
                             min_elevation=1600, max_elevation=3620, 
                             radiation_method='equal_intervals', radiation_distance=65, 
                             min_radiation=0, max_radiation=260)

# Glacier evolution
glacier_evolution = hb.preprocessing.GlacierEvolutionDeltaH()
glacier_df = glacier_evolution.compute_initial_ice_thickness(
    catchment, ice_thickness=GLACIER_ICE_THICKNESS)

# It can then optionally be saved as a csv file
glacier_df.to_csv(working_dir / 'glacier_profile.csv', index=False)

# The lookup table can be computed and saved as a csv file
glacier_evolution.compute_lookup_table(update_width=False)
glacier_evolution.save_as_csv(working_dir)

print(f"Files saved to: {working_dir}")

# Load the results from the CSV files
areas_evol = pd.read_csv(
    working_dir / "details_glacier_areas_evolution.csv", header=[0, 1, 2])
we_evol = pd.read_csv(
    working_dir / "details_glacier_we_evolution.csv", header=[0, 1, 2])
init_glacier_df = pd.read_csv(working_dir / "glacier_profile.csv")
init_glacier_df = init_glacier_df.drop(
    init_glacier_df[init_glacier_df["('glacier_area', 'm2')"] == 0].index)

# Group by elevation in case we have radiation / aspect discretization
init_glacier_df = init_glacier_df.groupby("('elevation', 'm')").sum().reset_index()
areas_evol = areas_evol.groupby(axis=1, level=2).sum()
we_evol = we_evol.groupby(axis=1, level=2).sum()

# Reproduce the plots from Seibert et al. (2018)
elevation_bands = np.unique(init_glacier_df["('elevation', 'm')"])

# Figure 2b - Absolute glacier volume per elevation band
plt.figure()
for i in range(0, len(areas_evol), 5):  # Grey lines
    volume = areas_evol.iloc[i, :].values * we_evol.iloc[i, :].values / (1000 * ICE_WE)
    plt.plot(volume, elevation_bands, drawstyle="steps-post", color="lightgrey")
for i in range(0, len(areas_evol), 20):  # Black lines
    volume = areas_evol.iloc[i, :].values * we_evol.iloc[i, :].values / (1000 * ICE_WE)
    plt.plot(volume, elevation_bands, drawstyle="steps-post", color="black")
volume = areas_evol.iloc[0, :].values * we_evol.iloc[0, :].values / (1000 * ICE_WE)
plt.plot(volume, elevation_bands, drawstyle="steps-post", color='red')
plt.xlabel('Glacier volume (m³)')
plt.ylabel('Elevation (m a.s.l.)')
plt.xlim(0, )
plt.tight_layout()
plt.show()

# Similar to Figure 2b but with glacier area
plt.figure()
for i in range(0, len(areas_evol), 5):  # Grey lines
    area = areas_evol.iloc[i, :].values
    plt.plot(area, elevation_bands, drawstyle="steps-post", color="lightgrey")
for i in range(0, len(areas_evol), 20):  # Black lines
    area = areas_evol.iloc[i, :].values
    plt.plot(area, elevation_bands, drawstyle="steps-post", color="black")
plt.plot(init_glacier_df["('glacier_area', 'm2')"], elevation_bands,
         drawstyle="steps-post", color='red')
plt.xlabel('Glacier area (scaled) (m²)')
plt.ylabel('Elevation (m a.s.l.)')
plt.xlim(0, )
plt.tight_layout()
plt.show()

# Figure 2c - Relative glacier area per elevation band as relative fraction of the
# initial glacier area of the elevation interval.
plt.figure()
for i in range(0, len(areas_evol), 5):  # Grey lines
    area = areas_evol.iloc[i, :].values
    ratio = area / init_glacier_df["('glacier_area', 'm2')"]
    plt.plot(ratio, elevation_bands,
             drawstyle="steps-post", color="lightgrey")
for i in range(0, len(areas_evol), 20):  # Black lines
    area = areas_evol.iloc[i, :].values
    ratio = area / init_glacier_df["('glacier_area', 'm2')"]
    ratio[np.isnan(ratio) | np.isinf(ratio)] = 0
    plt.plot(ratio, elevation_bands,
             drawstyle="steps-post", color="black")
plt.xlabel('Glacier area (scaled) / Glacier initial area (-)')
plt.ylabel('Elevation (m a.s.l.)')
plt.xlim(0, )
plt.tight_layout()
plt.show()
