from hydrobricks._exceptions import (
    ConfigurationError,
    DataError,
    DependencyError,
    ForcingError,
    HydroBricksError,
    ModelError,
)

# Import optional dependency management
from hydrobricks._optional import (  # Availability flags; Lazy-loaded modules
    HAS_GEOPANDAS,
    HAS_GRAPHVIZ,
    HAS_NETCDF,
    HAS_PYARROW,
    HAS_PYET,
    HAS_PYPROJ,
    HAS_PYSHEDS,
    HAS_RASTERIO,
    HAS_RIOXARRAY,
    HAS_SCIPY,
    HAS_SHAPELY,
    HAS_SPOTPY,
    HAS_XARRAY,
    HAS_XRSPATIAL,
    Dataset,
    gpd,
    graphviz,
    pyarrow,
    pyet,
    pyproj,
    pysheds,
    pyshedsGrid,
    rasterio,
    rxr,
    scipy,
    shapely,
    spotpy,
    xr,
    xrs,
)
from hydrobricks.catchment import Catchment
from hydrobricks.evaluation import (
    AuxiliaryObservation,
    DischargeObservations,
    DischargeTransform,
    GlacierMassBalanceObservations,
    SnowCoverObservations,
    evaluate,
)
from hydrobricks.forcing import Forcing
from hydrobricks.hydro_units import HydroUnits
from hydrobricks.models.model import Model
from hydrobricks.parameters import ParameterSet
from hydrobricks.periods import Period, Periods, evaluate_periods
from hydrobricks.plotter import Plotter
from hydrobricks.project import Project, load_project
from hydrobricks.results import Results
from hydrobricks.structure import StructureGraph
from hydrobricks.study import Study, load_study
from hydrobricks.time_series import TimeSeries

from ._hydrobricks import (
    close_log,
    init,
    init_log,
    set_debug_log_level,
    set_max_log_level,
    set_message_log_level,
)

init()
__all__ = (
    # Core classes
    "ParameterSet",
    "HydroUnits",
    "Forcing",
    "DischargeObservations",
    "AuxiliaryObservation",
    "GlacierMassBalanceObservations",
    "SnowCoverObservations",
    "TimeSeries",
    "Catchment",
    "Results",
    "Model",
    "Plotter",
    "StructureGraph",
    "Period",
    "Periods",
    "DischargeTransform",
    "evaluate",
    "evaluate_periods",
    # Project file loader
    "load_project",
    "Project",
    # Study file loader (multi-* comparison experiments)
    "load_study",
    "Study",
    # Logging functions
    "init",
    "init_log",
    "close_log",
    "set_debug_log_level",
    "set_max_log_level",
    "set_message_log_level",
    # Optional dependency flags
    "HAS_NETCDF",
    "HAS_RASTERIO",
    "HAS_GEOPANDAS",
    "HAS_SHAPELY",
    "HAS_SPOTPY",
    "HAS_PYET",
    "HAS_PYPROJ",
    "HAS_PYSHEDS",
    "HAS_SCIPY",
    "HAS_XARRAY",
    "HAS_RIOXARRAY",
    "HAS_PYARROW",
    "HAS_XRSPATIAL",
    "HAS_GRAPHVIZ",
    # Lazy-loaded optional modules
    "Dataset",
    "rasterio",
    "gpd",
    "graphviz",
    "shapely",
    "spotpy",
    "pyet",
    "pyproj",
    "pysheds",
    "pyshedsGrid",
    "scipy",
    "xr",
    "rxr",
    "pyarrow",
    "xrs",
    # Core custom exceptions
    "HydroBricksError",
    "DataError",
    "ConfigurationError",
    "ModelError",
    "ForcingError",
    "DependencyError",
)
