from . import _hydrobricks
from ._hydrobricks import (
    close_log,
    init,
    init_log,
    set_debug_log_level,
    set_max_log_level,
    set_message_log_level,
)

# Import optional dependency management
from hydrobricks._optional import (
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

from hydrobricks.catchment import Catchment
from hydrobricks.forcing import Forcing
from hydrobricks.hydro_units import HydroUnits
from hydrobricks.models.model import Model
from hydrobricks.observations import Observations
from hydrobricks.parameters import ParameterSet
from hydrobricks.results import Results
from hydrobricks.time_series import TimeSeries
from hydrobricks.trainer import evaluate

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
