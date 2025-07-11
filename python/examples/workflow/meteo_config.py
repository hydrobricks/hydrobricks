
from pathlib import Path

from configs import AREA_METEO_COMPATIBILITY, METEO_CONFIGS


class MeteoConfig:
    """
    @brief Handles configuration for meteorological data inputs used in hydrological modeling.
    
    Depending on the selected dataset (`meteo_data`) and geographical area (`area`), this class sets
    the appropriate file paths, NetCDF variable names, and coordinate metadata necessary to access 
    precipitation and temperature data for hydrological simulations.

    Supported meteorological datasets:
      - MeteoSwiss (observational)
      - CH2018_RCP26 / RCP45 / RCP85 (bias-corrected regional climate model scenarios)
      - Crespi et al. (2020) (South Tyrol-specific reanalysis)

    @param meteo_data The source of meteorological data (e.g., "MeteoSwiss", "CH2018_RCP26", "Crespi").
    @param area The glacier or region name (e.g., "Arolla", "Solda").
    @param path Base path to meteorological datasets.
    """
    
    def __init__(self, meteo_data: str, area: str, path: Path):
        ## @name MeteoConfig Initialization
        #  Initializes and sets up meteorological forcing configuration.
        #  @{
        ## Root directory of the meteorological datasets
        self.path = path
        ## Name of the meteorological dataset (e.g., MeteoSwiss, CH2018_RCP45, Crespi)
        self.meteo_data = meteo_data
        ## Path to the folder containing the meteorological input files
        self.forcing_dir = None
        # @}

        ## @name Precipitation Configuration
        #  Group of attributes related to precipitation input
        #  @{
        ## Path to the precipitation NetCDF files
        self.pr_path = None
        ## Pattern to match precipitation files (for reading multiple files or one file)
        self.pr_file_pattern = None
        ## Name of the precipitation variable inside the NetCDF files
        self.pr_varname = None
        # @}

        ## @name Temperature Configuration
        #  Group of attributes related to temperature input
        #  @{
        ## Path to the temperature NetCDF files
        self.tas_path = None
        ## Pattern to match temperature files (for reading multiple files or one file)
        self.tas_file_pattern = None
        ## Name of the temperature variable inside the NetCDF files
        self.tas_varname = None
        # @}
        
        ## @name NetCDF Dimensions and CRS
        #  Group of attributes for NetCDF metadata interpretation
        #  @{
        ## X-dimension name in the NetCDF files
        self.dim_x = None
        ## Y-dimension name in the NetCDF files
        self.dim_y = None
        ## Time-dimension name in the NetCDF files
        self.dim_time = None
        ## EPSG code for the coordinate reference system used in the NetCDF files
        self.nc_crs = None
        # @}
        
        ## @name Radiation File Configuration
        #  Configuration for potential radiation data
        #  @{
        ## Filename for the radiation file
        self.rad_file_pattern = 'daily_potential_radiation.nc'
        ## Variable name for radiation in the NetCDF file
        self.rad_varname = 'radiation'
        ## X-dimension for radiation NetCDF file
        self.rad_dim_x = 'x'
        ## Y-dimension for radiation NetCDF file
        self.rad_dim_y = 'y'
        ## Time-dimension for radiation NetCDF file
        self.rad_dim_time = 'time'
        # @}

        self._validate_inputs(area)
        self._setup_config()
        
    def _validate_inputs(self, area: str):
        """
        @brief Validates compatibility between meteorological dataset and geographical area.

        This method ensures that the provided combination of `area` and `meteo_data` is valid.
        Each geographical region supports only specific meteorological datasets. If the combination
        is not supported, a ValueError is raised with an explanatory message.

        @param area The glacier or region name (e.g., "Arolla", "Solda").
        @raises ValueError If the area is unknown or if the `meteo_data` is not compatible with the area.
        """
        
        # Resolve same_as links
        while isinstance(AREA_METEO_COMPATIBILITY[area], str) and AREA_METEO_COMPATIBILITY[area].startswith("same_as_"):
            area = AREA_METEO_COMPATIBILITY[area].replace("same_as_", "")
        
        valid_meteos = AREA_METEO_COMPATIBILITY[area]
        if valid_meteos is None:
            raise ValueError(f"Unknown area: {area}")
        
        if self.meteo_data not in valid_meteos:
            raise ValueError(f"{self.meteo_data} is not valid for area {area}")
            
    def _setup_config(self):
        """
        @brief Initializes dataset-specific parameters based on the selected meteorological source.

        Sets file paths, variable names, dimension names, and CRS values for precipitation and 
        temperature datasets. Also configures file patterns for batch access.
        """
        
        config = METEO_CONFIGS[self.meteo_data]
    
        self.forcing_dir = self.path.joinpath(*config["forcing_dir"])
        self.pr_path = self.forcing_dir / config["pr_path"] if config["pr_path"] else self.forcing_dir
        self.tas_path = self.forcing_dir / config["tas_path"] if config["tas_path"] else self.forcing_dir
    
        self.pr_file_pattern = config["pr_file_pattern"]
        self.tas_file_pattern = config["tas_file_pattern"]
        self.pr_varname = config["pr_varname"]
        self.tas_varname = config["tas_varname"]
    
        self.dim_x = config["dim_x"]
        self.dim_y = config["dim_y"]
        self.dim_time = config["dim_time"]
        self.nc_crs = config["nc_crs"]
