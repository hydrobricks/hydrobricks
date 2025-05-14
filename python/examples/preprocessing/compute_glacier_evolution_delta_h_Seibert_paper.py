import os.path
import tempfile
from pathlib import Path

import matplotlib.pyplot as plt
import numpy as np
import pandas as pd

import hydrobricks as hb
from hydrobricks.constants import ICE_WE

# Paths
TEST_FILES_DIR = Path(
    os.path.dirname(os.path.realpath(__file__)),
    '..', '..', '..', 'tests', 'files', 'catchments'
)
HYDRO_UNITS = TEST_FILES_DIR / 'synthetic_delta_h' / 'hydro_units_50m.csv'
GLACIER_PROFILE = TEST_FILES_DIR / 'synthetic_delta_h' / 'glacier_profile_id_50m.csv'

# Create temporary directory
with tempfile.TemporaryDirectory() as tmp_dir_name:
    tmp_dir = tmp_dir_name

os.mkdir(tmp_dir)
working_dir = Path(tmp_dir)

# Hydro units
hydro_units = hb.HydroUnits()
hydro_units.load_from_csv(
    HYDRO_UNITS, column_elevation='elevation', column_area='area')

# Glacier evolution
glacier_evolution = hb.preprocessing.GlacierEvolutionDeltaH(hydro_units)
# Compute the lookup table. In Seibert et al. (2018), the glacier width is not updated
# during the iterations (update_width=False), but we would recommend to do so.
glacier_evolution.compute_lookup_table(GLACIER_PROFILE, update_width=False)
glacier_evolution.save_as_csv(working_dir)

print(f"Files saved to: {working_dir}")

# Load the results from the CSV files
areas_evol = pd.read_csv(
    working_dir / "details_glacier_areas_evolution.csv", index_col=0, header=[0, 1, 2])
we_evol = pd.read_csv(
    working_dir / "details_glacier_we_evolution.csv", index_col=0, header=[0, 1, 2])
init_glacier_df = pd.read_csv(GLACIER_PROFILE, skiprows=[1])
init_glacier_df = init_glacier_df.drop(
    init_glacier_df[init_glacier_df.glacier_area == 0].index)

# Reproduce the plots from Seibert et al. (2018)
elevation_bands = init_glacier_df.elevation.values

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
plt.plot(init_glacier_df.glacier_area, init_glacier_df.elevation,
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
    ratio = area / init_glacier_df.glacier_area.values
    plt.plot(ratio, elevation_bands,
             drawstyle="steps-post", color="lightgrey")
for i in range(0, len(areas_evol), 20):  # Black lines
    area = areas_evol.iloc[i, :].values
    ratio = area / init_glacier_df.glacier_area.values
    ratio[np.isnan(ratio) | np.isinf(ratio)] = 0
    plt.plot(ratio, elevation_bands,
             drawstyle="steps-post", color="black")
plt.xlabel('Glacier area (scaled) / Glacier initial area (-)')
plt.ylabel('Elevation (m a.s.l.)')
plt.xlim(0, )
plt.tight_layout()
plt.show()