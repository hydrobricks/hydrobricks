from ._hydrobricks import (
    close_log,
    init,
    init_log,
    set_debug_log_level,
    set_max_log_level,
    set_message_log_level,
)

# Import optional dependency management
from ._optional import (
    # Availability flags
    HAS_NETCDF,
    HAS_RASTERIO,
    HAS_GEOPANDAS,
    HAS_SHAPELY,
    HAS_SPOTPY,
    HAS_PYET,
    HAS_PYPROJ,
    HAS_PYSHEDS,
    HAS_XARRAY,
    HAS_RIOXARRAY,
    HAS_PYARROW,
    HAS_XRSPATIAL,
    # Lazy-loaded modules
    Dataset,
    rasterio,
    gpd,
    shapely,
    spotpy,
    pyet,
    pyproj,
    pysheds,
    pyshedsGrid,
    xr,
    rxr,
    pyarrow,
    xrs,
)

from .catchment import Catchment
from .forcing import Forcing
from .hydro_units import HydroUnits
from .models.model import Model
from .observations import Observations
from .parameters import ParameterSet
from .results import Results
from .time_series import TimeSeries
from .trainer import evaluate

init()
__all__ = (
    # Core classes
    'ParameterSet', 'HydroUnits', 'Forcing', 'Observations', 'TimeSeries',
    'Catchment', 'Results', 'Model', 'evaluate',
    # Logging functions
    'init', 'init_log', 'close_log',
    'set_debug_log_level', 'set_max_log_level', 'set_message_log_level',
    # Optional dependency flags
    'HAS_NETCDF', 'HAS_RASTERIO', 'HAS_GEOPANDAS', 'HAS_SHAPELY',
    'HAS_SPOTPY', 'HAS_PYET', 'HAS_PYPROJ', 'HAS_PYSHEDS',
    'HAS_XARRAY', 'HAS_RIOXARRAY', 'HAS_PYARROW', 'HAS_XRSPATIAL',
    # Lazy-loaded optional modules
    'Dataset', 'rasterio', 'gpd', 'shapely', 'spotpy', 'pyet', 'pyproj',
    'pysheds', 'pyshedsGrid', 'xr', 'rxr', 'pyarrow', 'xrs',
)
