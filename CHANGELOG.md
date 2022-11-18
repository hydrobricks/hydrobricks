# Changelog
All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

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