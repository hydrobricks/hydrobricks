import datetime

import numpy as np
import rasterio
from insolation import insolf

# Options to change
height = 2614.75
latitude = 46.551196
longitude = 8.4035501
timezone = 1
time_step = 15  # minutes

# Default option values
visibility = 60
RH = 55
tempK = 285.0
O3 = 0.02
alphag = 0.2

# Load a DEM
demP = rasterio.open('dem_small_tile.tif')
dlxy = demP.res[0]
dem = demP.read(1)

# Create a time starting before the 1st of January
tt = datetime.datetime(2019, 12, 31)

# Compute insolation on a DEM over the whole year
I_tot_y = np.zeros((366, dem.shape[0], dem.shape[1]))
I_tot_dir_y = np.zeros((366, dem.shape[0], dem.shape[1]))
for d in np.arange(0, 366, 1):
    tt = tt + datetime.timedelta(days=1)
    print(tt)
    dayh = np.arange(0, 24, time_step / 60)

    # Results for the whole day
    jdrng = insolf.julian_day(tt.year, tt.month, tt.day, dayh)
    sunv = insolf.sunvector(jdrng, latitude, longitude, timezone)

    I_tot = np.zeros(dem.shape)
    I_tot_dir = np.zeros(dem.shape)
    for i in np.arange(len(dayh)):
        sunv = insolf.sunvector(jdrng[i], latitude, longitude, timezone)
        azimuth, zenith = insolf.sunpos(sunv)
        hsh = insolf.hillshading(dem, dlxy, sunv)
        Idir, Idiff = insolf.insolation(zenith, jdrng[i], height, visibility, RH, tempK,
                                        O3, alphag)
        # Global insolation in MJ/m^2
        # W2MJ = 3600e-6  # Watts/m^2 to MJ/m^2 hourly computation
        # I_tot = I_tot + W2MJ * Idir * hsh + W2MJ * Idiff
        I_tot = I_tot + Idir / 24.0 * hsh + Idiff / 24.0
        I_tot_dir = I_tot_dir + Idir / 24.0 * hsh

    I_tot = I_tot * (time_step / 60)
    I_tot_dir = I_tot_dir * (time_step / 60)

    I_tot_y[d, :, :] = I_tot
    I_tot_dir_y[d, :, :] = I_tot_dir

# Calculate the annual insolation
I_dir_res = np.mean(I_tot_dir_y, axis=0)

# Save the insolation map to a tiff file
profile = demP.profile
profile.update(dtype=rasterio.float32, count=1, compress='lzw')
with rasterio.open('radiation_annual_mean.tif', 'w', **profile) as dst:
    dst.write(I_dir_res.astype(rasterio.float32), 1)
