# Changelog
All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [0.3.1] - 2022-12-16

### Added

- Addition of the check of the parameters constraints with SPOTPY.
- Users can now define the prior distribution for SPOTPY to use.

### Changed

- Removing the old Monte Carlo implementation.


## [0.3.0] - 2022-12-07

### Added

- Integration with SPOTPY.

### Changed

- The selection of the parameters to calibrate is now handled by the ParameterSet class.


## [0.2.0] - 2022-12-01

### Added

- Addition of a catchment preprocessing module to build the elevation bands.

### Fixed

- Fixed issues with the water balance computation.

### Changed

- Some Python packages are made optional (e.g., netCDF4) and hydrobricks can be used without these.
- The water balance of the Socont model is fully tested.


## [0.1.4] - 2022-11-28

### Added

- Addition of functions to extract the water balance components.
- Addition of static content change rate (vs dynamic; e.g. for instantaneous fluxes).

### Fixed

- Fixed issues with fluxes and the water balance.

### Changed

- The parameter names are now using snake_case.
- The fraction assignment has been moved to the model initialization (so it's not forgotten).


## [0.1.3] - 2022-11-18

### Added

- Added checks to ensure that each container receiving water has a process attached.
- Creation of specific water containers for snow and ice.

### Fixed

- Fixed issue related to processes resolved out of the solver.

### Changed

- New approach to surface bricks which are now separated into land use and surface components (e.g., snowpack).


## [0.1.2] - 2022-11-12

### Added

- Adding state variables initialization.

### Fixed

- Reinitialize the state of the storages after a model run.


## [0.1.1] - 2022-11-07

### Fixed

- Fixed issue in the function to change the range of the parameter values.


## [0.1.0] - 2022-10-31
First preliminary release.