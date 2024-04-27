# Changelog
All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog(https://keepachangelog.com/en/1.0.0/),
and this project adheres to [Semantic Versioning(https://semver.org/spec/v2.0.0.html).


## 0.7.1 - 2024-04-27

### Added

-   Adding a function to compute the units connectivity.
-   Adding the cast shadow computation for the potential radiation.


## 0.7.0 - 2024-02-18

### Added

-   Adding a new functionality to create a BehaviourLandCoverChange object from shapefiles of glacier extents.
-   Adding a function to compute potential clear-sky direct solar radiation as defined by Hock (1999).
-   Adding a temperature index melt model based on Hock (1999).
-   Adding the possibility to discretize the hydrological units based on the mean annual potential radiation used in Hock (1999).
-   Adding an aspect-related degree day factor.
-   Adding a reference prediction based on a bootstrapping approach of the observed discharge.
-   Adding the option to define a parameter shared by different processes (e.g., snow and ice melt).
-   Allow calibrating a single parameter set for multiple catchments.
-   Adding a Results class to simplify outputs analysis.
-   Adding maps plotting and spatio-temporal animations of the model outputs.
-   Handling yearly recurrent forcing with a 'day_of_year' approach.
-   Moving the model structure definition from C++ to Python.

### Changed

-   Transitioning to a dictionary-based structure definition.
-   Moving the parameters generation out of the Socont class.
-   Moving the process parameters and forcing definitions to respective classes.
-   Changing workflow in load_from_csv.
-   Restructuring examples.
-   An error is raised if some parameter values are missing.
-   The parameter estimation and evaluation functions are restructured to a new trainer.py file.
-   Adding the date/time to the log file name.

### Fixed

-   Socont quick discharge has been fixed... For real, this time.
-   Fixing the SpotpySetup initialization.
-   Fixing issue with hydro unit initialization in catchment discretization.
-   Fixing a bug that occurred when the study area outline comprised multiple polygons.
-   Adapting the FixLandCoverFractionsTotal() function to also work with cases where the hydro unit is fully glaciated at the beginning of the Behaviour time series.


## 0.6.2 - 2023-09-15

### Breaking changes

-   The csv files defining the hydro units (e.g., elevation bands) properties have now to include the units of the values (e.g., m for elevation) below the column title.

### Added

-   Adding the computation of the slope and aspect from the DEM.
-   Transferring slope and aspect values to the hydrological model (e.g., for Socont quick discharge using the unit slope).
-   Any hydro unit property can now be extracted from data and provided to the hydrological model (flexible generic approach).
-   Adding the computation of the mean latitude and longitude for each hydro unit and allow using it for the PET computation.
-   Adding an extraction of the forcing using a faster weighted approach.
-   Addition of an example script for the catchment data preparation.
-   Allow loading unit ids from raster.
-   Adding a hydro unit discretization by elevation and aspect (separately or combined; with an example script).

### Changed

-   Refactoring catchment attributes parsing.
-   The csv files defining the hydro units (e.g., elevation bands) properties have now to include the units of the values (e.g., m for elevation) below the column title.

### Fixed

-   Socont quick discharge has been fixed.


## 0.6.1 - 2023-08-23

### Fixed

-   Fixing an issue with the data container shape when using the spatialization from gridded data.


## 0.6.0 - 2023-08-22

### Breaking changes

-   Many changes in the Forcing class:
    -   load_from_csv() was renamed to load_station_data_from_csv().
    -   define_spatialization() was renamed to spatialize_from_station_data() and is only meant for spatialization from station data.
    -   correct_station_data() was added and is to be used for applying a correction factor, for example.
    -   spatialize_from_gridded_data() was added to load data from gridded netCDF files.
    -   compute_pet() was added and uses the pyet package.
    -   The operations are not performed immediately, but only applied when apply_operations() is called, which is done automatically before the model run or before saving the forcing data to a netcdf file.
-   The Catchment class is now part of the main module.

### Added

-   Adding the spatialization from gridded data with spatialize_from_gridded_data().
-   Adding PET computation using the pyet library. New function: compute_pet().
-   The forcing data can now be loaded from the dumped netCDF file with load_from().

### Changed

-   Refactoring of the Forcing and TimeSeries classes to separate 1D and 2D data.
-   Enums are used to specify the Forcing variables.
-   The Catchment class is now part of the main module.
-   The Catchment class can generate a geotiff of the hydro unit ids with save_unit_ids_raster()

### Fixed

-   Fixing an issue with the elevation range condition in the elevation bands creation.


## 0.5.0 - 2023-07-06

### Breaking changes

-   Removing hyphens for underscores. Any component (including land cover elements) have to use underscores and not hyphens (e.g., glacier_ice instead of glacier-ice, slow_reservoir instead of slow-reservoir)

### Added

-   Adding aliases for all parameters.
-   Adding examples.
-   Adding Socont constraints.

### Changed

-   Changing Socont default values.
-   Upgrading conan builds.
-   Upgrading C++ minimal version.
-   Upgrading to Pandas 2.0.

### Fixed

-   Code improvements.
-   There were issues to build the wheel locally (in editable mode).


## 0.4.11 - 2023-02-17

### Added
-   Resetting the behaviour counters when resetting the model.
-   Adding land cover fractions to the logger output.

### Fixed
-   Fixing issue of behaviour dates not being correctly sorted.
-   Fixing issue with dates in land cover behaviours.


## 0.4.10 - 2023-02-17

### Fixed

-   Fixing ground area compensation in land cover changes.
-   Fixing flux weighting when applying land cover changes.


## 0.4.9 - 2023-02-15

### Added

-   Addition of functionalities for behaviours.
-   Remove empty changes in land cover changes behaviour.

### Fixed

-   Fixing issue with behaviours not triggering.


## 0.4.8 - 2023-01-18

### Fixed

-   Fixing precision issue with infinitesimal snow blocking ice melt.
-   Fixing issue with precipitation gradient approach with threshold.


## 0.4.7 - 2023-01-18

### Added

-   Addition of forcing dumping from SPOTPY.


## 0.4.6 - 2023-01-17

### Added

-   Addition of some parameter constraints to Socont.
-   Addition of a function to remove a constraint.
-   Addition of the constraints listing.

### Fixed

-   Fixing dump path creation in SPOTPY.


## 0.4.5 - 2023-01-16

### Changed

-   Check parameter constraints in SPOTPY and handle non-conforming cases.


## 0.4.4 - 2023-01-13

### Changed

-   Improvement of the parameter constraints handling in SPOTPY.


## 0.4.3 - 2023-01-10

### Changed

-   Allow using HydroErr metrics in SPOTPY.


## 0.4.2 - 2023-01-09

### Changed

-   Adding an option to dump outputs from SPOTPY.
-   Homogenisation of parameter naming in the spatialization of forcings.


## 0.4.1 - 2022-12-23

### Fixed

-   Fixing the warmup split when using SPOTPY.


## 0.4.0 - 2022-12-23

### Added

-   Addition of a land cover evolution in time.


## 0.3.1 - 2022-12-16

### Added

-   Addition of the check of the parameters constraints with SPOTPY.
-   Users can now define the prior distribution for SPOTPY to use.

### Changed

-   Removing the old Monte Carlo implementation.


## 0.3.0 - 2022-12-07

### Added

-   Integration with SPOTPY.

### Changed

-   The selection of the parameters to calibrate is now handled by the ParameterSet class.


## 0.2.0 - 2022-12-01

### Added

-   Addition of a catchment preprocessing module to build the elevation bands.

### Changed

-   Some Python packages are made optional (e.g., netCDF4) and hydrobricks can be used without these.
-   The water balance of the Socont model is fully tested.

### Fixed

-   Fixed issues with the water balance computation.


## 0.1.4 - 2022-11-28

### Added

-   Addition of functions to extract the water balance components.
-   Addition of static content change rate (vs dynamic; e.g. for instantaneous fluxes).

### Changed

-   The parameter names are now using snake_case.
-   The fraction assignment has been moved to the model initialization (so it's not forgotten).

### Fixed

-   Fixed issues with fluxes and the water balance.


## 0.1.3 - 2022-11-18

### Added

-   Added checks to ensure that each container receiving water has a process attached.
-   Creation of specific water containers for snow and ice.

### Changed

-   New approach to surface bricks which are now separated into land use and surface components (e.g., snowpack).

### Fixed

-   Fixed issue related to processes resolved out of the solver.


## 0.1.2 - 2022-11-12

### Added

-   Adding state variables initialization.

### Fixed

-   Reinitialize the state of the storages after a model run.


## 0.1.1 - 2022-11-07

### Fixed

-   Fixed issue in the function to change the range of the parameter values.


## 0.1.0 - 2022-10-31
First preliminary release.