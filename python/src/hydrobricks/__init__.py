from _hydrobricks import (
    close_log,
    init,
    init_log,
    set_debug_log_level,
    set_max_log_level,
    set_message_log_level,
)

from .forcing import Forcing
from .hydro_units import HydroUnits
from .observations import Observations
from .parameters import ParameterSet
from .time_series import TimeSeries

try:
    from netCDF4 import Dataset
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

init()
__all__ = ('ParameterSet', 'HydroUnits', 'Forcing', 'Observations', 'TimeSeries',
           'init', 'init_log', 'close_log', 'set_debug_log_level', 'set_max_log_level',
           'set_message_log_level', 'Dataset', 'rasterio', 'gpd', 'mapping', 'mask',
           'SpotpySetup', 'spotpy')
