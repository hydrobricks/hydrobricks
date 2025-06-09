import importlib.util
import warnings


class LazyImport:
    def __init__(self, module_name):
        self.module_name = module_name
        self.module = None

    def __getattr__(self, item):
        if self.module is None:
            self.module = importlib.import_module(self.module_name)
        return getattr(self.module, item)


def is_module_available(module_name):
    """Check if a module is available for import."""
    return importlib.util.find_spec(module_name) is not None


from _hydrobricks import (
    close_log,
    init,
    init_log,
    set_debug_log_level,
    set_max_log_level,
    set_message_log_level,
)

has_netcdf = is_module_available("netCDF4")
if has_netcdf:
    warnings.filterwarnings("ignore", message="numpy.ndarray size changed")
    from netCDF4 import Dataset
    warnings.resetwarnings()

has_rasterio = is_module_available("rasterio")
if has_rasterio:
    rasterio = LazyImport("rasterio")

has_geopandas = is_module_available("geopandas")
if has_geopandas:
    gpd = LazyImport("geopandas")

has_shapely = is_module_available("shapely")
if has_shapely:
    shapely = LazyImport("shapely")

has_spotpy = is_module_available("spotpy")
if has_spotpy:
    spotpy = LazyImport("spotpy")

has_pyet = is_module_available("pyet")
if has_pyet:
    pyet = LazyImport("pyet")

has_pyproj = is_module_available("pyproj")
if has_pyproj:
    pyproj = LazyImport("pyproj")

has_pysheds = is_module_available("pysheds")
if has_pysheds:
    pysheds = LazyImport("pysheds")
    from pysheds.grid import Grid as pyshedsGrid

has_xarray = is_module_available("xarray")
if has_xarray:
    xr = LazyImport("xarray")

has_rioxarray = is_module_available("rioxarray")
if has_rioxarray:
    rxr = LazyImport("rioxarray")

has_pyarrow = is_module_available("pyarrow")
if has_pyarrow:
    pyarrow = LazyImport("pyarrow")

has_xrspatial = is_module_available("xrspatial")
if has_xrspatial:
    xrs = LazyImport("xrspatial")

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
__all__ = ('ParameterSet', 'HydroUnits', 'Forcing', 'Observations', 'TimeSeries',
           'Catchment', 'Results', 'utils', 'init', 'init_log', 'close_log',
           'set_debug_log_level', 'set_max_log_level', 'set_message_log_level',
           'Dataset', 'rasterio', 'gpd', 'shapely', 'spotpy', 'pyet', 'pyproj',
           'pysheds', 'xr', 'rxr', 'xrs', 'evaluate', 'pyshedsGrid')
