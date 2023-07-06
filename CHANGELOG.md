# Changelog
All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog(https://keepachangelog.com/en/1.0.0/),
and this project adheres to [Semantic Versioning(https://semver.org/spec/v2.0.0.html).


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