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

init()
__all__ = ('ParameterSet', 'HydroUnits', 'Forcing', 'Observations', 'TimeSeries',
           'init', 'init_log', 'set_debug_log_level', 'set_max_log_level',
           'set_message_log_level')
