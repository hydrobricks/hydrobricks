import warnings

from _hydrobricks import (
    close_log,
    init,
    init_log,
    set_debug_log_level,
    set_max_log_level,
    set_message_log_level,
)

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
    has_rasterio = True

try:
    import geopandas as gpd
except ImportError:
    has_geopandas = False
else:
    has_geopandas = True

try:
    import shapely
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
    from .trainer import SpotpySetup

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
    import pysheds
except ImportError:
    has_pysheds = False
else:
    has_pysheds = True
    from pysheds.grid import Grid as pyshedsGrid

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
    import pyarrow as pa
except ImportError:
    has_pyarrow = False
else:
    has_pyarrow = True

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

try:
    import matplotlib.pyplot as plt  # noqa
except ImportError:
    has_matplotlib = False
else:
    has_matplotlib = True
    from .plotting import *  # noqa

from . import utils
from .catchment import Catchment
from .forcing import Forcing
from .hydro_units import HydroUnits
from .observations import Observations
from .parameters import ParameterSet
from .results import Results
from .time_series import TimeSeries
from .trainer import evaluate

init()
__all__ = ('ParameterSet', 'HydroUnits', 'Forcing', 'Observations', 'TimeSeries',
           'Catchment', 'Results', 'utils', 'init', 'init_log', 'close_log',
           'set_debug_log_level', 'set_max_log_level', 'set_message_log_level',
           'Dataset', 'rasterio', 'gpd', 'shapely', 'SpotpySetup', 'spotpy', 'pyet',
           'pyproj', 'pysheds', 'pyshedsGrid', 'xr', 'rxr', 'xrs', 'evaluate')
