from _hydrobricks import (
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
    _has_netcdf = False
else:
    _has_netcdf = True

try:
    import rasterio
except ImportError:
    _has_rasterio = False
else:
    from rasterio.mask import mask
    _has_rasterio = True

try:
    import geopandas as gpd
except ImportError:
    _has_geopandas = False
else:
    _has_geopandas = True

try:
    from shapely.geometry import mapping
except ImportError:
    _has_shapely = False
else:
    _has_shapely = True

init()
__all__ = ('ParameterSet', 'HydroUnits', 'Forcing', 'Observations', 'TimeSeries',
           'init', 'init_log', 'set_debug_log_level', 'set_max_log_level',
           'set_message_log_level')
