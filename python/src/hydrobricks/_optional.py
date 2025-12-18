"""Optional dependency management for hydrobricks.

This module handles detection and lazy loading of optional dependencies.
It provides availability flags (HAS_*) and lazy-loaded module proxies for
optional dependencies that may not be installed in all environments.

Usage from other hydrobricks modules (recommended):
    from hydrobricks._optional import HAS_SHAPELY

    if HAS_SHAPELY:
        from shapely.geometry import mapping
        # Use shapely functionality

    # Or use the lazy-loaded proxy:
    from hydrobricks._optional import HAS_RASTERIO, rasterio

    if HAS_RASTERIO:
        with rasterio.open(path) as dataset:
            # Use rasterio functionality

Usage from external code:
    import hydrobricks as hb

    if hb.HAS_GEOPANDAS:
        # Safe to use geopandas features
        gdf = hb.gpd.read_file(path)

Available flags:
    - HAS_NETCDF: NetCDF4 library
    - HAS_RASTERIO: Rasterio library
    - HAS_GEOPANDAS: GeoPandas library
    - HAS_SHAPELY: Shapely library
    - HAS_SPOTPY: SPOTPY library
    - HAS_PYET: PyEt library
    - HAS_PYPROJ: Pyproj library
    - HAS_PYSHEDS: Pysheds library
    - HAS_XARRAY: xarray library
    - HAS_RIOXARRAY: rioxarray library
    - HAS_PYARROW: PyArrow library
    - HAS_XRSPATIAL: xrspatial library
"""

import importlib.util
import warnings


class LazyImport:
    """Lazy import wrapper that delays actual import until first use."""

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


# Provide default placeholders so attributes are always present
Dataset = None
rasterio = None
gpd = None
shapely = None
spotpy = None
pyet = None
pyproj = None
pysheds = None
pyshedsGrid = None
xr = None
rxr = None
pyarrow = None
xrs = None

# Check availability and setup lazy imports
HAS_NETCDF = is_module_available("netCDF4")
if HAS_NETCDF:
    warnings.filterwarnings("ignore", message="numpy.ndarray size changed")
    from netCDF4 import Dataset
    warnings.resetwarnings()

HAS_RASTERIO = is_module_available("rasterio")
if HAS_RASTERIO:
    rasterio = LazyImport("rasterio")

HAS_GEOPANDAS = is_module_available("geopandas")
if HAS_GEOPANDAS:
    gpd = LazyImport("geopandas")

HAS_SHAPELY = is_module_available("shapely")
if HAS_SHAPELY:
    shapely = LazyImport("shapely")

HAS_SPOTPY = is_module_available("spotpy")
if HAS_SPOTPY:
    spotpy = LazyImport("spotpy")

HAS_PYET = is_module_available("pyet")
if HAS_PYET:
    pyet = LazyImport("pyet")

HAS_PYPROJ = is_module_available("pyproj")
if HAS_PYPROJ:
    pyproj = LazyImport("pyproj")

HAS_PYSHEDS = is_module_available("pysheds")
if HAS_PYSHEDS:
    pysheds = LazyImport("pysheds")
    from pysheds.grid import Grid as pyshedsGrid

HAS_XARRAY = is_module_available("xarray")
if HAS_XARRAY:
    xr = LazyImport("xarray")

HAS_RIOXARRAY = is_module_available("rioxarray")
if HAS_RIOXARRAY:
    rxr = LazyImport("rioxarray")

HAS_PYARROW = is_module_available("pyarrow")
if HAS_PYARROW:
    pyarrow = LazyImport("pyarrow")

HAS_XRSPATIAL = is_module_available("xrspatial")
if HAS_XRSPATIAL:
    xrs = LazyImport("xrspatial")

__all__ = [
    # Utility functions
    'is_module_available',
    'LazyImport',
    # Availability flags
    'HAS_NETCDF',
    'HAS_RASTERIO',
    'HAS_GEOPANDAS',
    'HAS_SHAPELY',
    'HAS_SPOTPY',
    'HAS_PYET',
    'HAS_PYPROJ',
    'HAS_PYSHEDS',
    'HAS_XARRAY',
    'HAS_RIOXARRAY',
    'HAS_PYARROW',
    'HAS_XRSPATIAL',
    # Lazy-loaded modules
    'Dataset',
    'rasterio',
    'gpd',
    'shapely',
    'spotpy',
    'pyet',
    'pyproj',
    'pysheds',
    'pyshedsGrid',
    'xr',
    'rxr',
    'pyarrow',
    'xrs',
]

