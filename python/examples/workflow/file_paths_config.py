import os
from pathlib import Path
from configs import FILEPATH_CONFIGS

class FilePathsConfig:
    """
    @brief Handles and generates all necessary file paths for model outputs, figures, 
    discharge files, and forcing input data for a specific hydrological simulation setup.

    This class constructs directory structures and filenames based on modeling parameters such as
    area, catchment, algorithm, melt model, and objective function. It supports configuration for
    multi-calibration setups and simulations with or without debris-covered glaciers.
    """
    
    def __init__(self, path: Path, area: str, catchment: str, algorithm: str, 
                 melt_model: str, obj_func: str, multi_calibration: str, 
                 with_debris: bool, subdirectory: str | None):
        """
        @brief Constructor that initializes all file paths and directories for the simulation.
        
        @param path Base directory for the datasets.
        @param area The glacier or region name (e.g., 'Arolla', 'Solda')
        @param catchment The catchment or subcatchment name (e.g., 'BI', 'HGDA', 'TN', 'PI', 'BS', 'VU', 'DB' for 'Arolla')
        @param algorithm The optimization algorithm name for the calibration step (e.g., 'SCEUA' 'MC')
        @param melt_model The melt model used (e.g., 'degree_day', 'degree_day_aspect', 'temperature_index')
        @param obj_func The objective function used for calibration (e.g., 'kge_2012', 'nse', 'mse')
        @param multi_calibration Identifier for multi-calibration setup
        @param with_debris Boolean indicating if debris-covered glacier areas are treated differently
        @param subdirectory Optional subdirectory name appended to the results path
        """
        
        self.path = Path(path).expanduser().resolve()
        # Make sure the path exists or raise a helpful error
        if not self.path.exists():
            raise FileNotFoundError(f"Specified data path does not exist: {self.path}")
        
        self.id = f"{algorithm}_melt:{melt_model}_{obj_func}"
        if multi_calibration:
            self.id = f"{algorithm}_melt:{melt_model}_{obj_func}_multicalib:{multi_calibration}"

        self.results = None
        self.figures = None
        
        self.hydro_units = None
        self.calibration_stats = None
        self.calibration_output = None
        self.calibration_all = None
        self.bootstrapping_stats = None
        
        self.changes_glacier_ice = None
        self.changes_glacier_debris = None
        self.changes_ground = None
        self.changes_glacier = None
        
        self.param_interac = None
        self.comparison = None
        self.discharge = None
        
        self.discharge_file = None
        self.column_time = None
        self.time_format = None
        self.content = None
        
        self.forcing_file = None
        self.unit_ids = None
        self.best_forcing_input_file = None
        self.best_forcing_stats_and_output = None
        self.median_forcing_input_file = None
        self.median_forcing_stats_and_output = None
        self.top_forcing_input_file = None
        self.top_forcing_stats_and_output = None
        
        self._create_directories(area, catchment, subdirectory)
        self._set_output_file_paths()
        self._set_output_figure_paths()
        self._set_actions_file_paths()
        self._set_discharge_file_characteristics(area, catchment)
        self._set_forcing_file_paths(area, algorithm, melt_model, obj_func, with_debris)

    def _create_directories(self, area: str, catchment: str, 
                            subdirectory: str | None):
        """
        @brief Creates results and figures directories if they do not already exist.
        
        @param area The glacier or region name (e.g., 'Arolla', 'Solda')
        @param catchment The catchment or subcatchment name (e.g., 'BI', 'HGDA', 'TN', 'PI', 'BS', 'VU', 'DB' for 'Arolla')
        @param subdirectory Optional subdirectory name appended to the results path
        """
        
        self.results = self.path / "Outputs" / area / catchment
        if subdirectory:
            self.results = self.results / subdirectory
        if not os.path.exists(self.results):
            os.makedirs(self.results)
        self.figures = self.path / "OutputFigures" / area / catchment
        if not os.path.exists(self.figures):
            os.makedirs(self.figures)

    def _set_output_file_paths(self):
        """
        @brief Sets file paths for various output CSVs such as hydrological units,
        calibration statistics and discharge time series, and bootstrapping results.
        """
        
        self.hydro_units = self.results / "hydro_units.csv"
        self.calibration_stats = self.results / f"best_fit_simulation_stats_{self.id}.csv"
        self.calibration_output = self.results / f"best_fit_simulated_discharge_{self.id}.csv"
        self.calibration_all = self.results / f"socont_{self.id}"
        self.bootstrapping_stats = self.results / "bootstrapping_stats.csv"

    def _set_output_figure_paths(self):
        """
        @brief Sets file paths for various figures, including parameter interaction,
        comparison plots, and discharge plots.
        """
        
        self.param_interac = self.figures / f"parameter_interaction_{self.id}"
        self.comparison = self.figures / f"comparison_{self.id}"
        self.discharge = self.figures / f"discharge_{self.id}"

    def _set_actions_file_paths(self):
        """
        @brief Sets file paths related to changes in areas of glacier ice, debris,
        ground land covers, and total glacier changes.
        """
        
        self.changes_glacier_ice = self.results / "changes_glacier_ice.csv"
        self.changes_glacier_debris = self.results / "changes_glacier_debris.csv"
        self.changes_ground = self.results / "changes_ground.csv"
        self.changes_glacier = self.results / "changes_glacier.csv"

    def _set_discharge_file_characteristics(self, area: str, catchment: str):
        """
        @brief Defines properties of the observed discharge file, including its path,
        time column format, and discharge column content.
        
        @param area The glacier or region name (e.g., 'Arolla', 'Solda')
        @param catchment The catchment or subcatchment name (e.g., 'BI', 'HGDA', 'TN', 'PI', 'BS', 'VU', 'DB' for 'Arolla')
        """
        
        self.discharge_file = self.path / "Outputs" / "ObservedDischarges" / f"{area}_hydrobricks_discharge_{catchment}.csv"
        self.column_time = 'Date'
        self.time_format = '%d/%m/%Y'
        self.content = {'discharge': 'Discharge (mm/d)'}

    def _set_forcing_file_paths(self, area: str, algorithm: str, melt_model: str, 
                                obj_func: str, with_debris: bool):
        """
        @brief Sets file paths for various forcing input files, unit ID raster, and
        statistics based on best, median, and top simulations.
        
        @param area The glacier or region name (e.g., 'Arolla', 'Solda')
        @param algorithm The optimization algorithm name for the calibration step (e.g., 'SCEUA' 'MC')
        @param melt_model The melt model used (e.g., 'degree_day', 'degree_day_aspect', 'temperature_index')
        @param obj_func The objective function used for calibration (e.g., 'kge_2012', 'nse', 'mse')
        @param with_debris Boolean indicating if debris-covered glacier areas are treated differently
        """
        
        snow_melt_process = f"melt:{melt_model}"
        self.forcing_file = self.results / "forcing.nc"
        self.unit_ids = self.results / "unit_ids.tif"
        MELT_MODEL_ABBR = {
            "degree_day": "TI",
            "degree_day_aspect": "ATI",
            "temperature_index": "HTI"
        }
        abbr = MELT_MODEL_ABBR.get(melt_model)
        
        # Resolve same_as_* indirection
        while isinstance(FILEPATH_CONFIGS[area], str) and FILEPATH_CONFIGS[area].startswith("same_as_"):
            area = FILEPATH_CONFIGS[area].replace("same_as_", "")
    
        config = FILEPATH_CONFIGS[area]
        area = config["area"]
        catchment = config["catchment"]
        debris_subfolder = config.get("with_debris_subfolder")
        
        # Build base path to input CSVs
        base_path = self.path / "Outputs" / area / catchment
        if with_debris and debris_subfolder:
            base_path = base_path / debris_subfolder
    
        self.best_forcing_input_file = str(base_path / f"best_fit_simulation_stats_{algorithm}_{snow_melt_process}_{obj_func}.csv")
        self.best_forcing_stats_and_output = self.results / f"forced_simulation_params_and_results_{algorithm}_{snow_melt_process}_{obj_func}_bestfit_{catchment}.csv"
    
        self.median_forcing_input_file = self.path / "Outputs" / area / f"{abbr}_medians.csv"
        self.median_forcing_stats_and_output = self.results / f"median_forced_simulation_params_and_results_{algorithm}_{snow_melt_process}_{obj_func}_bestfit_{catchment}.csv"
    
        self.top_forcing_input_file = self.path / "Outputs" / area / f"top_{abbr}_merged_dfs.csv"
        self.top_forcing_stats_and_output = self.results / f"top_forced_simulation_params_and_results_{algorithm}_{snow_melt_process}_{obj_func}_bestfit_{catchment}.csv"

