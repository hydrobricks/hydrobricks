"""
Compute glacier evolution (delta h) from ice data.
"""

import tempfile
import uuid
from pathlib import Path

import matplotlib.pyplot as plt
import numpy as np
import pandas as pd

import hydrobricks as hb
from hydrobricks.constants import ICE_WE

DISCRETIZE_BY_RADIATION = True  # Set to True to use radiation discretization
COMPUTE_RADIATION = False  # True: compute radiation. False: use the precomputed data.
GLACIER_EVOLUTION_FROM_TOPO = True  # Use topography to compute glacier area evolution
UPDATE_WIDTH = False  # Update glacier width during iterations

# Paths
TEST_FILES_DIR = (
    Path(__file__).resolve().parent.parent.parent.parent /
    'tests' / 'files' / 'catchments' / 'ch_rhone_gletsch'
)
CATCHMENT_OUTLINE = TEST_FILES_DIR / 'outline.shp'
CATCHMENT_DEM = TEST_FILES_DIR / 'dem.tif'
GLACIER_ICE_THICKNESS = TEST_FILES_DIR / 'glaciers' / 'ice_thickness.tif'
CATCHMENT_BANDS = TEST_FILES_DIR / 'hydro_units_elevation_radiation.csv'
HYDRO_UNITS_RADIATION = TEST_FILES_DIR / 'unit_ids_radiation.tif'

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
if DISCRETIZE_BY_RADIATION:
    if COMPUTE_RADIATION:
        catchment.calculate_daily_potential_radiation(TEST_FILES_DIR, resolution=None)
        catchment.discretize_by(
            ['elevation', 'radiation'],
            elevation_method='equal_intervals',
            elevation_distance=100,
            min_elevation=1600,
            max_elevation=3620,
            radiation_method='equal_intervals',
            radiation_distance=65,
            min_radiation=0,
            max_radiation=260
        )
    else:
        catchment.hydro_units.load_from_csv(
            CATCHMENT_BANDS,
            column_elevation='elevation',
            column_area='area'
        )
        catchment.load_unit_ids_from_raster(HYDRO_UNITS_RADIATION)
else:
    catchment.create_elevation_bands(method='equal_intervals', distance=100)

# Compute initial ice thickness and save as CSV
glacier_evolution = hb.preprocessing.GlacierEvolutionDeltaH()
glacier_df = glacier_evolution.compute_initial_ice_thickness(
    catchment,
    ice_thickness=GLACIER_ICE_THICKNESS,
    glacier_area_evolution_from_topo=GLACIER_EVOLUTION_FROM_TOPO
)

# Compute and save lookup table as CSV
glacier_evolution.compute_lookup_table(
    catchment,
    update_width=UPDATE_WIDTH
)
glacier_evolution.save_as_csv(working_dir)

print(f"Files saved to: {working_dir}")

# Load the results from the CSV files
areas_evol = pd.read_csv(
    working_dir / "details_glacier_areas_evolution.csv",
    header=[0, 1, 2]
)
we_raw = pd.read_csv(
    working_dir / "details_glacier_we_evolution.csv",
    header=[0, 1, 2]
)

# Group by elevation in case we have radiation / aspect discretization
glacier_df = glacier_df.groupby(('elevation', 'm')).sum().reset_index()
# Sum of weights (total area per group)
total_area = areas_evol.T.groupby(level=2).sum().T
weighted_sum = (we_raw * areas_evol).T.groupby(level=2).sum().T
we_evol = (weighted_sum / total_area).fillna(0)
areas_evol = areas_evol.T.groupby(level=2).sum().T

elevation_bands = np.unique(glacier_df[('elevation', 'm')])

# --- Plotting Section ---
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

# Figure 2b - Absolute glacier thickness per elevation band
plt.figure()
for i in range(0, len(areas_evol), 5):  # Grey lines
    thickness = we_evol.iloc[i, :].values / (1000 * ICE_WE)
    plt.plot(thickness, elevation_bands, drawstyle="steps-post", color="lightgrey")
for i in range(0, len(areas_evol), 20):  # Black lines
    thickness = we_evol.iloc[i, :].values / (1000 * ICE_WE)
    plt.plot(thickness, elevation_bands, drawstyle="steps-post", color="black")
thickness = we_evol.iloc[0, :].values / (1000 * ICE_WE)
plt.plot(thickness, elevation_bands, drawstyle="steps-post", color='red')
plt.xlabel('Glacier thickness (m)')
plt.ylabel('Elevation (m a.s.l.)')
plt.xlim(0, )
plt.tight_layout()
plt.show()

# Glacier area per elevation band
plt.figure()
for i in range(0, len(areas_evol), 5):  # Grey lines
    area = areas_evol.iloc[i, :].values
    plt.plot(area, elevation_bands, drawstyle="steps-post", color="lightgrey")
for i in range(0, len(areas_evol), 20):  # Black lines
    area = areas_evol.iloc[i, :].values
    plt.plot(area, elevation_bands, drawstyle="steps-post", color="black")
plt.plot(glacier_df[('glacier_area', 'm2')], elevation_bands,
         drawstyle="steps-post", color='red')
plt.xlabel('Glacier area (scaled) (m²)')
plt.ylabel('Elevation (m a.s.l.)')
plt.xlim(0, )
plt.tight_layout()
plt.show()

# Figure 2c - Relative glacier area per elevation band as fraction of initial area
plt.figure()
for i in range(0, len(areas_evol), 5):  # Grey lines
    area = areas_evol.iloc[i, :].values
    ratio = area / glacier_df[('glacier_area', 'm2')]
    plt.plot(ratio, elevation_bands, drawstyle="steps-post", color="lightgrey")
for i in range(0, len(areas_evol), 20):  # Black lines
    area = areas_evol.iloc[i, :].values
    ratio = area / glacier_df[('glacier_area', 'm2')]
    ratio[np.isnan(ratio) | np.isinf(ratio)] = 0
    plt.plot(ratio, elevation_bands, drawstyle="steps-post", color="black")
plt.xlabel('Glacier area (scaled) / Glacier initial area (-)')
plt.ylabel('Elevation (m a.s.l.)')
plt.xlim(0, )
plt.tight_layout()
plt.show()

# Glacier volume evolution over increments
plt.figure()
for i in range(0, len(areas_evol), 1):
    volume = np.sum(areas_evol.iloc[i, :].values * we_evol.iloc[i, :].values /
                    (1000 * ICE_WE))
    plt.plot(i, volume, color="black", marker='o')
plt.xlabel('Increment')
plt.ylabel('Glacier volume (m³)')

# Glacier thickness evolution over increments
plt.figure()
for i in range(0, len(areas_evol), 1):
    thickness = np.mean(we_evol.iloc[i, :].values)
    plt.plot(i, thickness, color="lightgrey", marker='o')
plt.xlabel('Increment')
plt.ylabel('Glacier thickness (m)')
plt.figure()

# Glacier area evolution over increments
for i in range(0, len(areas_evol), 1):
    areas = np.sum(areas_evol.iloc[i, :].values)
    plt.plot(i, areas, color="lightgrey", marker='o')
plt.xlabel('Increment')
plt.ylabel('Glacier area (scaled) (m²)')
plt.tight_layout()
plt.show()

