import warnings

from _hydrobricks import (
    close_log,
    init,
    init_log,
    set_debug_log_level,
    set_max_log_level,
    set_message_log_level,
)

from .catchment import Catchment
from .forcing import Forcing
from .hydro_units import HydroUnits
from .observations import Observations
from .parameters import ParameterSet
from .time_series import TimeSeries

try:
    warnings.filterwarnings("ignore", message="numpy.ndarray size changed")
    from netCDF4 import Dataset
    warnings.resetwarnings()
except ImportError:
    has_netcdf = False
else:
    has_netcdf = True

try:
    import rasterio
except ImportError:
    has_rasterio = False
else:
    from rasterio.mask import mask
    has_rasterio = True

try:
    import geopandas as gpd
except ImportError:
    has_geopandas = False
else:
    has_geopandas = True

try:
    from shapely.geometry import mapping
except ImportError:
    has_shapely = False
else:
    has_shapely = True

try:
    import spotpy
except ImportError:
    has_spotpy = False
else:
    has_spotpy = True
    from .spotpy_setup import SpotpySetup

try:
    import pyet
except ImportError:
    has_pyet = False
else:
    has_pyet = True

try:
    import pyproj
except ImportError:
    has_pyproj = False
else:
    has_pyproj = True

try:
    import xarray as xr
except ImportError:
    has_xarray = False
else:
    has_xarray = True

try:
    import rioxarray as rxr
except ImportError:
    has_rioxarray = False
else:
    has_rioxarray = True
    if not has_xarray:
        raise ImportError("xarray is required to use rioxarray.")

try:
    import xrspatial as xrs
except ImportError:
    has_xrspatial = False
else:
    has_xrspatial = True
    if not has_xarray:
        raise ImportError("xarray is required to use xrspatial.")
    if not has_rioxarray:
        raise ImportError("rioxarray is required to use xrspatial.")

init()
__all__ = ('ParameterSet', 'HydroUnits', 'Forcing', 'Observations', 'TimeSeries',
           'Catchment', 'init', 'init_log', 'close_log', 'set_debug_log_level',
           'set_max_log_level', 'set_message_log_level', 'Dataset', 'rasterio', 'gpd',
           'mapping', 'mask', 'SpotpySetup', 'spotpy', 'pyet', 'pyproj', 'xr', 'rxr',
           'xrs')
