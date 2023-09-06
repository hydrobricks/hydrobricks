from pathlib import Path

import pandas as pd
import xarray as xr

DATA_DIR = Path("path/to/data")
FILE_NAME = "data.nc"
START_DATE = "1962-01-01"
N_DAYS = 3

# Load the NetCDF file
nc_file_path = DATA_DIR / FILE_NAME
ds = xr.open_dataset(nc_file_path)

# Calculate the midpoint index for the "E" dimension
midpoint_idx = ds.dims['E'] // 2
midpoint_E = ds.E[midpoint_idx]

# Extract the time range for the first three days
start_date = pd.Timestamp(START_DATE)
end_date = start_date + pd.DateOffset(days=N_DAYS - 1)
subset_ds = ds.sel(time=slice(start_date, end_date), E=slice(midpoint_E, None))

# Add compression to the NetCDF file
encoding = {var: {"zlib": True, "complevel": 4} for var in subset_ds.data_vars}

# Save the resampled subset to a new NetCDF file
output_file_path = DATA_DIR / "resampled_subset.nc"
subset_ds.to_netcdf(output_file_path, encoding=encoding)

# Close the original and subset datasets
ds.close()
subset_ds.close()

print("Resampling and extraction complete.")
