# configs.py
from pathlib import Path

"""
@file configs.py
@brief Configuration module for hydrological modeling setup.

This module centralizes all static, non-code configuration used across the modeling workflow. 
It defines spatial-temporal settings, meteorological data metadata, glacier evolution inputs, 
and file structure layout per study area.

It supports "Swiss" (e.g., Arolla, Ferpecle) and "South Tyrol" (e.g., Solda) case studies, 
and defines which model inputs are valid, where datasets are located, and how to configure 
simulation parameters automatically per area.

@details

Configuration keys:
====================
- `TEMPORAL_CONFIGS`: Dict[area][catchment] → (start_date, end_date)
  Sets historical simulation periods based on data availability.

- `FUTURE_TEMPORAL_CONFIGS`: Dict[area][catchment] → (start_date, end_date)
  Defines a default simulation window extending to 2099 for future climate scenarios.

- `RADIATION_CONFIGS`: Dict[area or None] → radiation discretization settings.
  Configures radiation binning methods and ranges (default is a global config under `None`).

- `DEM_CONFIGS`: Dict[area] → DEM path, elevation zoning, latitude, EPSG codes, outlines.
  Provides geospatial metadata and elevation discretization parameters for each study area.

- `METEO_CONFIGS`: Dict[dataset_name] → gridded input file structure and variable names.
  Describes precipitation/temperature NetCDF files, CRS, dimensions, and file matching patterns.

- `AREA_METEO_COMPATIBILITY`: Dict[area] → list of supported meteorological datasets.
  Maps which climate input datasets are allowed per area (e.g., only 'Crespi' for Solda).

- `GLACIER_CONFIGS`: Dict[area] → glacier shapefile inputs and change method configs.
  Stores time-dependent shapefiles, either for glacier evolution via delta-h or land cover change.

- `FILEPATH_CONFIGS`: Dict[area] → folder structure for simulation results.
  Maps the root output directory structure by area, catchment, and optional debris subfolder.

Special values:
---------------
- `"same_as_*"`: Reuses another area's config to avoid duplication.
- `None` keys in nested dicts (e.g., TEMPORAL_CONFIGS) indicate defaults for areas or catchments.

@note
This config structure is designed for static usage. Any dynamic runtime overrides (e.g., CLI args,
GUI selections) should be handled by reading and adapting these structures at runtime.

@see
- used in: `setting_model()`, `setting_forcing()`, `setting_hydro_units_and_changes()`, etc.
- related modules: `area_config.py`, `meteo_config.py`, `file_paths.py`
"""

TEMPORAL_CONFIGS = {
    'Arolla': {
        None: ('2009-01-01', '2014-12-31'),  # no catchment specified
    },
    'Ferpecle': {
        None: ('2009-01-01', '2014-12-31'),
    },
    'Mattertal': {
        None: ('2009-01-01', '2014-12-31'),
    },
    'Solda': {
        'PS': ('2014-01-01', '2021-12-31'),
        'SGF': ('2017-01-01', '2020-12-31'),
        'TF': ('2017-01-01', '2019-12-31'),
        'ZA': ('2017-01-01', '2018-12-31'),
    },
    'Solda_D_CASCADE': {
        None: ('2009-01-01', '2014-12-31'),
    }
}

FUTURE_TEMPORAL_CONFIGS = {
    None: {
        None: ('2009-01-01', '2099-12-31'),  # for all areas, all catchments
    },
}

RADIATION_CONFIGS = {
    None: {
        'method': 'equal_intervals',
        'min_val': 0,
        'max_val': 260,
        'distance': 65,
    }
}

DEM_CONFIGS = {
    'Arolla': {
        'dem_path': Path("Swiss_Study_area/StudyAreas_EPSG21781_clip.tif"),
        'outline': Path("Swiss_discharge/Arolla_discharge/Watersheds_on_dhm25/{catchment}_UpslopeArea_EPSG21781/out.shp"),
        'outline_epsg': 21781,
        'elev_method': 'equal_intervals',
        'elev_min_val': 1900,
        'elev_max_val': 3900,
        'elev_distance': 40,
        'latitude': 46.0,
    },
    'Ferpecle': {
        'dem_path': Path("Swiss_Study_area/StudyAreas_EPSG21781_Ferpecle.tif"),
        'outline': Path("Swiss_discharge/Ferpecle_discharge/Watersheds_on_dhm25/{catchment}_UpslopeArea_EPSG21781/out.shp"),
        'outline_epsg': 21781,
        'elev_method': 'equal_intervals',
        'elev_min_val': 1900,
        'elev_max_val': 4380,
        'elev_distance': 40,
        'latitude': 46.0,
    },
    'Mattertal': {
        'dem_path': Path("Swiss_Study_area/StudyAreas_EPSG21781_Mattertal.tif"),
        'outline': Path("Swiss_discharge/Mattertal_discharge/Watersheds_on_dhm25/{catchment}_UpslopeArea_EPSG21781/out.shp"),
        'outline_epsg': 21781,
        'elev_method': 'equal_intervals',
        'elev_min_val': 1900,
        'elev_max_val': 3900,
        'elev_distance': 40,
        'latitude': 46.0,
    },
    'Solda': {
        'dem_path': Path("Italy_Study_area/dtm_25m_utm_st_whole_StudyArea_OriginalCRS_filled.tif"),
        'outline': Path("Italy_discharge/Watersheds/{catchment}_UpslopeArea_EPSG25832/out.shp"),
        'outline_epsg': 25832,
        'elev_method': 'equal_intervals',
        'elev_min_val': 1100,
        'elev_max_val': 3900,
        'elev_distance': 70,
        'latitude': 46.5,
    },
    'Solda_D_CASCADE': {
        'dem_path': Path("Italy_Study_area/dtm_25m_utm_st_whole_StudyArea_OriginalCRS_filled.tif"),
        'outline': Path("Italy_discharge/DCASCADE_nodes/upslope_area_{catchment}.shp"),
        'outline_epsg': 25832,
        'elev_method': 'equal_intervals',
        'elev_min_val': 1100,
        'elev_max_val': 3900,
        'elev_distance': 70,
        'latitude': 46.5,
    }
}

METEO_CONFIGS = {
    "MeteoSwiss": {
        "forcing_dir": ["Swiss_Past_MeteoSwiss", "OldVersion"],
        "pr_path": "RhiresDSmall",
        "tas_path": "TabsDSmall",
        "pr_file_pattern": "RhiresD_ch01r.swisscors_*.nc",
        "tas_file_pattern": "TabsD_ch01r.swisscors_*.nc",
        "pr_varname": "RhiresD",
        "tas_varname": "TabsD",
        "dim_x": "chx",
        "dim_y": "chy",
        "dim_time": "time",
        "nc_crs": 21781
    },
    "CH2018_RCP26": {
        "forcing_dir": ["Swiss_Future_CH2018", "server_data", "QMgrid"],
        "pr_path": None,
        "tas_path": None,
        "pr_file_pattern": "CH2018_pr_SMHI-RCA_ECEARTH_EUR11_RCP26_QMgrid_1981-2099.nc",
        "tas_file_pattern": "CH2018_tas_SMHI-RCA_ECEARTH_EUR11_RCP26_QMgrid_1981-2099.nc",
        "pr_varname": "pr",
        "tas_varname": "tas",
        "dim_x": "lon",
        "dim_y": "lat",
        "dim_time": "time",
        "nc_crs": 4150
    },
    "CH2018_RCP45": {
        "forcing_dir": ["Swiss_Future_CH2018", "server_data", "QMgrid"],
        "pr_path": None,
        "tas_path": None,
        "pr_file_pattern": "CH2018_pr_SMHI-RCA_ECEARTH_EUR11_RCP45_QMgrid_1981-2099.nc",
        "tas_file_pattern": "CH2018_tas_SMHI-RCA_ECEARTH_EUR11_RCP45_QMgrid_1981-2099.nc",
        "pr_varname": "pr",
        "tas_varname": "tas",
        "dim_x": "lon",
        "dim_y": "lat",
        "dim_time": "time",
        "nc_crs": 4150
    },
    "CH2018_RCP85": {
        "forcing_dir": ["Swiss_Future_CH2018", "server_data", "QMgrid"],
        "pr_path": None,
        "tas_path": None,
        "pr_file_pattern": "CH2018_pr_SMHI-RCA_ECEARTH_EUR11_RCP85_QMgrid_1981-2099.nc",
        "tas_file_pattern": "CH2018_tas_SMHI-RCA_ECEARTH_EUR11_RCP85_QMgrid_1981-2099.nc",
        "pr_varname": "pr",
        "tas_varname": "tas",
        "dim_x": "lon",
        "dim_y": "lat",
        "dim_time": "time",
        "nc_crs": 4150
    },
    "Crespi": {
        "forcing_dir": ["Italy_Past_Crespi-etal_2020_allfiles"],
        "pr_path": "Subset_files_split_by_year",
        "tas_path": "Subset_files_split_by_year",
        "pr_file_pattern": "DailyPrec_*.nc",
        "tas_file_pattern": "DailyMeanTemp_*.nc",
        "pr_varname": "precipitation",
        "tas_varname": "temperature",
        "dim_x": "x",
        "dim_y": "y",
        "dim_time": "DATE",
        "nc_crs": 32632
    }
}

#        Supported combinations:
#        - Swiss regions (`Arolla`, `Ferpecle`, `Mattertal`) support:
#            * "MeteoSwiss"
#            * "CH2018_RCP26"
#            * "CH2018_RCP45"
#            * "CH2018_RCP85"
#        - South Tyrol regions (`Solda`, `Solda_D_CASCADE`) support:
#            * "Crespi"
AREA_METEO_COMPATIBILITY = {
    "Arolla": ["MeteoSwiss", "CH2018_RCP26", "CH2018_RCP45", "CH2018_RCP85"],
    "Ferpecle": "same_as_Arolla",
    "Mattertal": "same_as_Arolla",
    "Solda": ["Crespi"],
    "Solda_D_CASCADE": "same_as_Solda"
}

GLACIER_CONFIGS = {
    "Arolla": {
        "glacier_path": "Swiss_GlaciersExtents",
        "with_debris": {
            "clean_glaciers": [
                "Ice_Ind_Thr_2.00_DEM_2000__made_sure_no_shift_in_pixels__manually_corrected/Clean_Ice_Fin.shp",
                "Ice_Ind_Thr_2.00_DEM_2000__made_sure_no_shift_in_pixels__manually_corrected/Clean_Ice_Fin.shp",
                "inventory_sgi2016_r2020/Clean_ice.shp"
            ],
            "debris_glaciers": [
                "Ice_Ind_Thr_2.00_DEM_2000__made_sure_no_shift_in_pixels__manually_corrected/Debris_C_Final.shp",
                "Ice_Ind_Thr_2.00_DEM_2000__made_sure_no_shift_in_pixels__manually_corrected/Debris_C_Final.shp",
                "inventory_sgi2016_r2020/SGI_2016_debriscover.shp"
            ],
            "times": ['2009-01-01', '2010-01-01', '2016-01-01']
            # We duplicate the data from 2010 to use it in 2009 (approximation since the beginning of our simulation is in 2009).
        },
        "without_debris": {
            "whole_glaciers": [
                "inventory_sgi1850_r1992/SGI_1850.shp",
                "inventory_sgi1931_r2022/SGI_1931.shp",
                "inventory_sgi1973_r1976/SGI_1973.shp",
                "inventory_sgi2010_r2010/SGI_2010.shp",
                "inventory_sgi2010_r2010/SGI_2010.shp",
                "inventory_sgi2016_r2020/SGI_2016_glaciers.shp"
            ],
            "times": ['1850-01-01', '1931-01-01', '1973-01-01', '2009-01-01', '2010-01-01', '2016-01-01'] # BUGS if EPSG:4326... (still?)
        },
        "glacier_shp": None,
        "glacier_thickness": "Ice_thicknesses/04_IceThickness_SwissAlps/IceThickness_EPSG21781.tif"
    },
    "Ferpecle": {
        "glacier_path": "Swiss_GlaciersExtents",
        "without_debris": {
            "whole_glaciers": [
                "inventory_sgi1850_r1992/SGI_1850.shp",
                "inventory_sgi1931_r2022/SGI_1931.shp",
                "inventory_sgi1973_r1976/SGI_1973.shp",
                "inventory_sgi2010_r2010/SGI_2010.shp",
                "inventory_sgi2010_r2010/SGI_2010.shp",
                "inventory_sgi2016_r2020/SGI_2016_glaciers.shp"
            ],
            "times": ['1850-01-01', '1931-01-01', '1973-01-01', '2009-01-01', '2010-01-01', '2016-01-01']
        },
        "glacier_shp": None,
        "glacier_thickness": "Ice_thicknesses/04_IceThickness_SwissAlps/IceThickness_EPSG21781.tif"
    },
    "Mattertal": "same_as_Ferpecle",
    "Solda": {
        "glacier_path": "Italy_GlaciersExtents",
        "without_debris": {
            "whole_glaciers": [
                "SaraSavi/SuldenGlacier1985_fromOrthophoto_EPSG25832.shp",
                "GlaciersExtents/GlaciersExtents1997/glaciers_1997_EPSG25832.shp",
                "GlaciersExtents/GlaciersExtents2005/glaciers_2005_EPSG25832.shp",
                "GlaciersExtents/GlaciersExtents2016_2017/glaciers_2016_2017_EPSG25832.shp",
                "SaraSavi/SuldenGlacier2018_fromOrthophoto_EPSG25832.shp",
                "SaraSavi/SuldenGlacier2018_fromOrthophoto_EPSG25832.shp"
            ],
            "debris_glaciers": [None] * 6,
            "times": ['1985-01-01', '1997-01-01', '2005-01-01', '2016-01-01', '2018-01-01', '2024-01-01']
        },
        "glacier_shp": "SaraSavi/SuldenGlacier2018_fromOrthophoto_EPSG25832.shp",
        "glacier_thickness": None
    },
    "Solda_D_CASCADE": "same_as_Solda"
}

FILEPATH_CONFIGS = {
    "Arolla": {
        "area": "Arolla",
        "catchment": "BI",
        "with_debris_subfolder": "with_debris"
    },
    "Ferpecle": "same_as_Arolla",
    "Solda": {
        "area": "Solda",
        "catchment": "PS",
        "with_debris_subfolder": None
    },
    "Solda_D_CASCADE": "same_as_Solda"
}

