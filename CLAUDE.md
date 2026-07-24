
# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## What is hydrobricks

Hydrobricks is a modular hydrological modelling framework. The core engine is written in C++20 and exposed to Python via pybind11 bindings. Users interact with the framework through the Python API.

## Commands

### Python (primary usage)

```bash
# Install in development mode (builds C++ bindings)
pip install -e ".[test]"

# Run all Python tests
pytest

# Run a single test file
pytest python/tests/test_binding.py

# Run a single test
pytest python/tests/test_binding.py::test_name

# Lint and format
pre-commit run --all-files
```

### C++ core (when modifying C++ code)

Requires `VCPKG_ROOT` environment variable pointing to a vcpkg installation.

```bash
mkdir bin && cd bin

# Configure (all targets)
cmake .. -DCMAKE_BUILD_TYPE=Release

# Configure (bindings only, skip CLI/tests)
cmake .. -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTS=OFF -DBUILD_CLI=OFF

# Build
cmake --build . --config Release

# Run C++ tests (copy test data first)
cp -R ../core/tests/files/ tests/
./tests/hydrobricks-tests        # Linux/macOS
tests/hydrobricks-tests.exe      # Windows
```

CMake options: `BUILD_TESTS` (ON), `BUILD_CLI` (ON), `BUILD_PYBINDINGS` (ON), `USE_SANITIZERS` (OFF).

## Architecture

### Two-layer design

1. **C++ core** (`core/src/`) — the computation engine. Organized into:
   - `base/` — `ModelHydro`, `Forcing`, `TimeSeries`, `Parameter`, `Solver*`, `SettingsModel`, `SettingsBasin`
   - `spatial/` — `SubBasin`, `HydroUnit` (spatial discretization)
   - `bricks/` — land cover units: `Glacier`, `Snowpack`, `LandCover`, `Vegetation`, `Urban`, `Storage`
   - `processes/` — hydrological processes: melt (`ProcessMelt*`), ET (`ProcessET*`), outflow (`ProcessOutflow*`), infiltration, runoff, lateral flow
   - `fluxes/` — water flux connections between bricks (`FluxToBrick`, `FluxToOutlet`, `Splitter*`)
   - `containers/` — `WaterContainer`, `IceContainer`, `SnowContainer`
   - `actions/` — dynamic landscape changes (glacier evolution, land cover change)
   - `app/` — CLI entry point

2. **Python layer** (`python/src/hydrobricks/`) — orchestration and user API:
   - `Catchment` — basin/catchment definition
   - `HydroUnits` — spatial discretization (reads shapefiles/rasters)
   - `Forcing` — meteorological input data
   - `ParameterSet` — parameter management with constraints/ranges
   - `Observations` — observed discharge for calibration
   - `Results` — simulation output
   - `TimeSeries` / `time_series.py` — time series handling
   - `Trainer` (in `trainer.py`) — calibration/optimization
   - `models/` — model implementations (e.g., `Socont`)
   - `actions/` — Python wrappers for dynamic actions
   - `preprocessing/` — spatial preprocessing utilities (DEM, glacier, catchment delineation)
   - `plotting/` — visualization

3. **Bindings** (`core/bindings/`) — pybind11 code that exposes `ModelHydro` and related C++ classes to Python as `_hydrobricks` (the compiled extension module).

### How a simulation works

The Python API builds `SettingsModel` and `SettingsBasin` YAML/object configurations, passes them to `ModelHydro` (C++) via the bindings, and calls `Run()`. Results are retrieved via `GetOutletDischarge()` and similar methods.

### Model structure concept

A model is a graph of **Bricks** connected by **Fluxes**. Each Brick represents a storage or land cover type. Each Brick has **Processes** that compute outflows. Fluxes route water between Bricks or to the basin outlet. **Splitters** partition precipitation (e.g., rain/snow split). The `SubBasin` owns `HydroUnit` objects, each of which holds a set of Bricks.

## Code style

- **Python**: Black (88 chars), isort (black profile), flake8
- **C++**: clang-format Google style, 120-column limit (see `.clang-format`)
- **CMake**: cmake-format, tab size 4, line width 120 (see `.cmake-format.yaml`)

Dependencies managed via vcpkg (`vcpkg.json`); fetched at CMake configure time via `FetchContent`: yaml-cpp, pybind11. NetCDF, Eigen3, GTest fetched via vcpkg.

### Logging

The C++ core uses a custom lightweight logger (`core/src/base/Log.h`/`Log.cpp`). Use `LogError(...)`, `LogWarning(...)`, `LogMessage(...)`, `LogDebug(...)` — all accept `std::format`-style format strings. Log level is controlled via `LogSetLevel(LogLevel::Debug/Message/Warning/Error)`. Python bindings expose `init_log(path)`, `close_log()`, `set_debug_log_level()`, `set_message_log_level()`.
