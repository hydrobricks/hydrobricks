
from pathlib import Path
from configs import GLACIER_CONFIGS, DEM_CONFIGS, RADIATION_CONFIGS, TEMPORAL_CONFIGS, FUTURE_TEMPORAL_CONFIGS

class AreaConfig:
    """
    @brief Defines spatial and temporal configurations for a catchment study area.

    This class sets up DEM, outline, elevation/radiation binning, glacier outlines, and glacier 
    thicknesses depending on the selected area and catchment. It supports configurations for both 
    historical and future glacier simulations, with or without debris-covered ice.

    It handles different file structures and coordinate systems for Swiss and South Tyrolean study areas.
    """
    
    def __init__(self, area: str, path: Path, catchment: str, future: bool, 
                 with_debris: bool):
        """
        @brief Constructor that initializes the area-specific configuration.

        @param area The glacier or region name (e.g., 'Arolla', 'Ferpecle', 'Solda', etc.)
        @param path Root path to the project or data directory (should be a pathlib.Path)
        @param catchment The catchment or subcatchment name (e.g., 'BI', 'HGDA', 'TN', 'PI', 'BS', 'VU', 'DB' for 'Arolla')
        @param future Boolean flag indicating if future simulation setup is required
        @param with_debris Boolean indicating if debris-covered glacier areas are treated differently
        """
        
        self.path = path
        self.area = area
        self.catchment = catchment
        self.whole_glaciers = None
        self.clean_glaciers = None
        self.debris_glaciers = None
        self.times = None
        self.glacier_shp = None
        self.glacier_thickness = None
        self.dem_path = None
        self.outline = None
        self.outline_epsg = None
        self.elev_method = None
        self.elev_min_val = None
        self.elev_max_val = None
        self.elev_distance = None
        self.rad_method = None
        self.rad_min_val = None
        self.rad_max_val = None
        self.rad_distance = None
        self.latitude = None
        self.start_date = None
        self.end_date = None

        self._setup_dem_config()
        self._setup_temporal_config(future)
        self._setup_glacier_config(with_debris)

    def _setup_dem_config(self):
        """
        @brief Sets up the DEM path, catchment outline, elevation binning, and latitude for the selected area.

        Depending on the area, the DEM file, outline shapefile, coordinate system (EPSG),
        and binning method parameters (for elevation and radiation) are configured accordingly.
        """
        config = DEM_CONFIGS.get(self.area)
        if config is None:
            raise ValueError(f"Unsupported area: {self.area}")
        
        self.dem_path = self.path / config["dem_path"]
        # Format outline path with catchment
        self.outline = self.path / Path(
            str(config["outline"]).format(catchment=self.catchment)
        )
        self.outline_epsg = config["outline_epsg"]
        self.elev_method = config["elev_method"]
        self.elev_min_val = config["elev_min_val"]
        self.elev_max_val = config["elev_max_val"]
        self.elev_distance = config["elev_distance"]
        self.latitude = config["latitude"]
        
        config = RADIATION_CONFIGS.get(None)
        self.rad_method = config['method']
        self.rad_min_val = config['min_val']
        self.rad_max_val = config['max_val']
        self.rad_distance = config['distance']
        
    def _setup_glacier_config(self, with_debris: bool):
        """
        @brief Configures glacier outlines and thickness data depending on the area and debris-cover option.

        Loads the appropriate glacier shapefiles for clean/debris/total glaciers for each time step. 
        Optionally includes glacier thickness raster files when available. Distinguishes future glacier 
        simulation shape input where needed.
        @param with_debris Boolean indicating if debris-covered glacier areas are treated differently
        """
        
        area = self.area
        # Resolve same_as links
        while isinstance(GLACIER_CONFIGS[area], str) and GLACIER_CONFIGS[area].startswith("same_as_"):
            area = GLACIER_CONFIGS[area].replace("same_as_", "")
        
        cfg = GLACIER_CONFIGS[area]
        glacier_path = self.path / cfg["glacier_path"]
    
        if with_debris and "with_debris" in cfg:
            wd = cfg["with_debris"]
            self.clean_glaciers = [glacier_path / p for p in wd["clean_glaciers"]]
            self.debris_glaciers = [glacier_path / p for p in wd["debris_glaciers"]]
            self.times = wd["times"]
        else:
            wod = cfg["without_debris"]
            self.whole_glaciers = [glacier_path / p for p in wod["whole_glaciers"]]
            self.debris_glaciers = wod.get("debris_glaciers", [None] * len(self.whole_glaciers))
            self.times = wod["times"]
    
        self.glacier_shp = glacier_path / cfg["glacier_shp"] if cfg["glacier_shp"] else None
        self.glacier_thickness = glacier_path / cfg["glacier_thickness"] if cfg["glacier_thickness"] else None
        
    def _setup_temporal_config(self, future: bool):
        """
        @brief Sets the simulation start and end dates for each area and catchment.

        Dates are based on data availability and runtime constraints.
        For future simulations, FUTURE_TEMPORAL_CONFIGS is used.
        @param future Boolean flag indicating if future simulation setup is required
        """
        
        config = FUTURE_TEMPORAL_CONFIGS if future else TEMPORAL_CONFIGS
    
        area_cfg = config.get(self.area, config.get(None, {}))
        start_end = area_cfg.get(self.catchment) or area_cfg.get(None)
    
        if start_end is None:
            raise ValueError(f"No temporal configuration for area '{self.area}' with catchment '{self.catchment}'")
    
        self.start_date, self.end_date = start_end
