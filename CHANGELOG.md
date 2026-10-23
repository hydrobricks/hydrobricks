# Changelog
All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).


## Unreleased

### Breaking changes

- The "land use" vocabulary is dropped, with no deprecation shim. hydrobricks has land covers, which differ either by their *type* (what the model builds: `open`, `forest`, `glacier`, `water`, `wetland`, `urban`, `rock`) or only by their *parameters*; an entry of an external classification, such as PREVAH's 22 classes, is a *cover class*. What PREVAH calls a land-use table is a land cover classification — forests, meadows, crops, rock, water — and the values read from it (interception capacity, vegetation cover fraction, rooting depth, albedo, LAI) are physical properties of the canopy and the soil, not a socioeconomic use of the land. Renamed accordingly:
  - `hydrobricks.land_covers.PREVAH_LAND_USE_*` → `PREVAH_COVER_*`, and `PREVAH_LAND_USE_COVER_TYPES` → `PREVAH_COVER_TYPES`;
  - `PrevahUniBE.apply_land_use` → `apply_cover_parameters`, `apply_land_use_field_capacity` → `apply_cover_field_capacity`, `apply_land_use_percolation` → `apply_cover_percolation`, `land_use_field_capacity` → `cover_field_capacity`, `land_use_interception_capacity` → `cover_interception_capacity`;
  - the `land_use` argument of those methods → `cover_class`;
  - the `land_use` column of the Ticino hydro units CSV and of `prevah_ticino_project.yaml` → `cover_class`.

### Added

- Adding river networks of subbasins (multi-subbasin routing, see `docs/design/multi_sub_basin_routing.md`): `HydroUnits.set_subbasins()` takes a table of subbasins (`id`, `downstream`, optional `name` and properties such as the reach `length` and `slope`), and the hydro units carry a `subbasin` column (picked up from the CSV when present). The network is validated (unique IDs, one outlet, no cycle, every unit on a declared subbasin), saved in the hydro units NetCDF file (`subbasin` variable and `subbasins` dimension) and parsed back by the core, which builds a `RiverNetwork` of `Subbasin` objects with their reaches, processing order (upstream first) and drained areas. A model then runs the subbasins upstream first, each with its own solver; the volume leaving a subbasin enters the reach of its downstream subbasin and joins the local runoff at its outlet within the same time step (instantaneous by default). The `channel_routing` model option selects the channel routing scheme along the reaches (distinct from the in-catchment `routing:*` processes of a structure): `lag` (pure translation by `length / celerity`) or `muskingum` (`K = length / celerity`, weighting factor `x`, sub-stepped when the time step exceeds `2K(1-X)`); the reach `length` is a subbasin property and `celerity` / `x` are model parameters (component `channel`, aliases `channel_celerity` / `muskingum_x`, overridable per subbasin by the `celerity` / `muskingum_x` properties). With `record_all`, `reach:inflow`, `reach:outflow` and `reach:storage` are recorded per subbasin. Gauges: `DischargeObservations(subbasin=...)` observes the outlet of a subbasin (scored against it by `Model.eval`, `evaluate_periods` and `SpotpySetup`, as the primary signal or as an additional one through `extra_observations`, with its own `metric` and `weight`), and `units="m3/s"` with `drained_area` converts a record in m³/s to mm per step (`HydroUnits.get_subbasin_areas()` gives the local and drained areas). Project files gain a `subbasins` section (`file`, `hydro_units_column`) and accept a list of `observations` (the first is the primary gauge; `subbasin`, `units`, `metric`, `weight` per entry), exposed as `project.gauges` and passed to `project.calibrate()`. Preprocessing: `Catchment.delineate_subbasins()` delineates the subbasins from gauge locations on the DEM (outlets snapped to the streams, nested areas carved into a tree, a subbasin added for the catchment outlet when needed), measures the reach `length` (D8 flow path) and `slope` on the DEM, and sets the subbasin table on the hydro units; `save_subbasin_ids_raster()` writes the raster (`examples/preprocessing/delineate_subbasins.py`). Each subbasin builds its catchment-level components from the structure variant matching the land covers of its units (like the hydro units do), so a glacier-free subbasin of a glacierized catchment carries no glacier reservoir (the GSM reservoirs left the glacier-free base variant); `subbasin_structure_ids` is written to the results (`Results.get_subbasin_structure_ids()`). The reach values (`reach:inflow`, `reach:outflow`, `reach:storage`) are now recorded whenever the channel routing is on, not only with `record_all`. Adding the `muskingum_cunge` channel routing scheme: K and X are recomputed at every time step from the channel geometry (reach length and slope, `channel:width`, `channel:manning`) and the discharge through the Manning relation, so neither is calibrated. The `lag` and `muskingum` schemes accept a discharge-dependent celerity (`channel:celerity_exponent`, `channel:reference_discharge`), and the celerity is kept within 0.05 and 10 m/s in every scheme. Adding the `route_local_runoff` model option: a subbasin's own runoff then travels through half of its reach instead of joining at its outlet with no delay. The discharge is available in m³/s as well as in mm per time step: `Model.get_outlet_discharge(units=...)`, `Model.get_subbasin_discharge(id, units=...)` and `Results.get_discharge(subbasin_id, units=...)`. `delineate_subbasins` now measures the reach lengths along the `river_network` lines when one is given, instead of the D8 flow path, which misses the meanders of the mapped river; the slope stays the elevation drop over that length, and `reach_lengths='flow_path'` keeps the previous behaviour. A Muskingum-Cunge reach is now divided into sub reaches routed in series, following the Ponce and Theurer criterion for the length of a sub reach; the count is fixed at the first time step from `channel:reference_discharge` and capped at 50. Adding river network diagnostics: `model.get_network_graph()` (a `RiverNetworkGraph` with the subbasin tree, the reach geometry, the travel times and the time from each subbasin to the catchment outlet), `model.print_network()`, `model.plot_network()`, and `Results.get_reach_water_balance()` reporting the closure of every reach. The discharge at every subbasin outlet is available with `Model.get_subbasin_discharge(id)` (mm over the area drained at that outlet), with `get_subbasin_ids()`, `get_subbasin_downstream_ids()` and `get_subbasin_areas()`; a catchment without a declared network is a single subbasin and runs exactly as before. The results file gains a `subbasins` dimension: `subbasin_values[aggregated_values, subbasins, time]` with `subbasin_ids`, `subbasin_downstream_ids`, `subbasin_local_areas` and `subbasin_drained_areas` (global attribute `format_version = 2`), read with `Results.get_subbasin_values(component, subbasin_id)`, which also reads the former 2-D layout. `delineate_subbasins(subbasins=...)` reads the subbasins from polygons instead of outlet points: local areas, nested drained areas or a mix of both (every cell goes to the smallest polygon covering it), with the topology found on the DEM or given by `downstream_column`; with a `river_network`, no flow directions are computed (the reaches are measured along the lines and pysheds is not needed). The subbasins come before the hydro units, as no hydro unit may span two subbasins: the discretization (`discretize_by`, `create_elevation_bands`) then creates the hydro units within them and sets their `subbasin` column, a unit raster loaded afterwards is checked against them, and delineating the subbasins of a catchment that already has hydro units is refused.
- Hydro unit properties from a raster: `Catchment.extract_unit_property_from_raster(raster, name, unit, reclassify=..., statistic='mean')` warps any raster onto the DEM grid and stores its per-unit mean as a hydro unit property, which a parameter can then follow per unit through `ParameterSet.set_spatial`. `reclassify` turns class codes into values for a map given as classes (unmapped codes are ignored rather than averaged in).
- Soil properties of the hydro units from soil maps: `Catchment.extract_soil_properties(available_water_content=..., soil_depth=..., conductivity=...)` writes the `awc` [%], `soil_depth` [m] and `conductivity` properties that `PrevahUniBE.apply_cover_field_capacity` and `apply_cover_percolation` consume — the latter being how PREVAH's KWPER percolation factor gets its input, which nothing in the package could produce until now. `reclassify` takes the name of a class-coded soil map (`'swiss_soil_suitability'`: PREVAH's tables for the Swiss soil suitability map, in `hydrobricks.soils.SOIL_CLASS_TABLES`) or one mapping per property; the conductivity unit follows the tables (relative `-` for the Swiss classes) and is `mm/h` for a raster of values. For soil data outside Switzerland, `hydrobricks.soils` reads class tables from the attribute table of a map such as the HWSD (`class_table_from_csv`, weighting the soils of a mapping unit), averages layered products such as SoilGrids, HiHydroSoil or EU-SoilHydroGrids over a depth (`integrate_layers`), and derives the available water content and conductivity from the texture with the Saxton & Rawls (2006) pedotransfer function (`saxton_rawls`, `hydraulics_from_texture`). Extracting a hydro-unit property from a raster again now replaces it instead of adding a duplicate column.
- Eight aspect classes in the catchment discretization (`discretize_by(..., aspect_classes=8)`, N/NE/E/SE/S/SW/W/NW), and `flat_slope_threshold` to send the cells flatter than a given slope to the `FLAT` class instead of to a direction. It defaults to 3°, below which the aspect of a cell of a 25 m DEM is mostly noise (perturbing the DEM by 1 m moves 37 % of the cells of the 1-2° band to another class out of four, against 14 % at 3-5°) while the north-south difference in annual clear-sky radiation it would resolve stays under 8 %. Set it to 0 for the previous behaviour. A finer DEM calls for a higher threshold, since the error of the aspect scales with the vertical error over the cell size.
- `area_of` brick key in model structures: a storage representing a land cover's area (e.g. a per-cover soil store) has its capacity and its evapotranspiration scaled by that cover's live area fraction.
- Recording the area each hydro-unit value is expressed over, and converting between them. A state is held over the area of its brick and a flux over the area of the element receiving it, so the values do not share one reference: `land_cover` (a land cover and its snowpack or canopy, and the fluxes staying on that cover), `hydro_unit` (the bricks spanning the unit, the fluxes between them, and the fluxes leaving a land cover weighted by its fraction) and `subbasin` (the fluxes to the outlet or to a subbasin brick, as a share of the subbasin). The results file gains the `labels_distributed_basis` attribute and the `hydro_units_subbasin_fractions` variable; `Model.get_recorded_hydro_unit_basis(label)` and `Results.get_hydro_units_basis(component)` return the basis, and `Model.get_recorded_hydro_unit_values(label, basis=...)` and `Results.get_hydro_units_values(..., basis=...)` convert the values to another one from the recorded areas and land-cover fractions. The recorded values themselves are unchanged.

### Changed

- Reading the PREVAH snow-albedo reduction of the soil ET (`et:prevah`) from the cover owning the soil store when the stores are per cover (`share_soil=False`), instead of the area-weighted mean over the hydro unit. A soil store of its own belongs to a single land cover, so the snow lying on a neighbouring cover should not damp its evapotranspiration; with one cover the mean collapses to the switch of a PREVAH hydrotope (the snow albedo while the cover holds snow, `albedo_land` otherwise). The cover is passed as gate brick of the process, and a gate brick is now linked whenever one is declared (`Process::NeedsGateBrickLinking()` only tells whether the process cannot work without one), so a process can read an optional gate. A shared soil store spans several covers and keeps the unit-wide mean, so the default `share_soil=True` is unaffected.
- Renaming the user-facing `sub_basin` spellings to `subbasin` (the usual term in SWAT, HEC-HMS, HYPE and Raven): `attach_to: subbasin` in custom structures, `ModelSettings.add_subbasin_brick()`, the `subbasin_bricks`/`subbasin_splitters` keys and the `subbasin` level of the structure graph, `Results.list_subbasin_components()`, and the `subbasin_values` variable of the results file. The old spellings are still accepted on input (`attach_to`, the two renamed methods) for one release.
- HBV-96 per-class soils (the default with several soil-bearing covers) and PREVAH-UniBE with `share_soil=False`: `fc_<cover>` is now the capacity of the cover's own soil (in mm over the cover area) rather than a share of the unit's soil storage. Each store is scaled by its cover's area fraction in each unit and follows land cover changes, so a cover absent from a unit no longer holds the percolation gate shut, and per-cover soils now work on single-cover units (PREVAH hydrotopes).
- The default solver is now `exponential_euler` instead of `crank_nicolson`. Measured against a converged reference of the same model (Socont on the Sitter catchment, fast reservoirs at a daily step), it is as accurate (NSE 0.999 against 0.997) at about half the cost, and stays unconditionally stable. Models relying on the default give slightly different results; pass `solver="crank_nicolson"` to keep the previous ones. PREVAH-UniBE keeps its `analytic_linear` default and the continuous GR4J/GR6J modes their `heun_explicit`. The solvers page of the documentation compares the schemes and documents the benchmark (`doc/images/benchmark_solvers.py` in the documentation repository).
- The lower bound of `rain_correction_factor` (`rfcf`) is widened from 0.8 to 0.7: beyond gauge undercatch, a calibrated factor also absorbs the bias of the interpolation onto the catchment, so values below 0.8 are legitimate (the Ticino PREVAH calibration uses 0.776, which the previous bound rejected).

### Fixed

- Losing the flat parts of a catchment when discretizing by aspect: the aspect of a flat cell is undefined (the computation marks it -1, and NaN along the border of the DEM), so it matched none of the N/E/S/W classes, was left out of every hydro unit and its area disappeared from the catchment. Such cells now go to a `FLAT` aspect class, which is only created when they exist. On the Sitter test catchment, 69 cells (0.06 % of the area) were dropped. The mean aspect of a unit was affected too: the -1 marker entered the circular mean as an angle of -1° and pulled it north; flat cells are now left out of it.
- Per-cover soil stores of HBV-96 and PREVAH-UniBE (`share_soil=False`) evaporated at the full potential rate of the unit each, instead of over their cover's share of it, so a unit with several covers could evaporate more than the potential (about +45% for PREVAH and +60% for HBV-96 on a 60/40 open/forest test case).
- Delaying by one time step the water a land cover or a snowpack passes to a storage, whatever the solver. The amount is computed before the solver runs and its rate then cleared, so the constraints of the receiving store counted no inflow and limited its outflows to its start-of-step content: rain infiltrating on a given day only left the store on the next one. Such fluxes are now static amounts, available within the step. The effect is largest at a daily step with fast reservoirs: on the Sitter catchment, the solvers scored an NSE of 0.53 to 0.84 against a converged sub-daily reference, and 0.93 to 0.999 since, explicit Euler aside (0.58: it evaluates its rates at the start of the step, so it keeps a one-step lag by construction).
- Keeping water in a store indefinitely with the `implicit_euler` solver, and partly with `crank_nicolson`, when a process switches off as the store runs dry (e.g. a constant percolation). The root of the implicit equation then lies at the empty store, the bracketed iteration could settle on a content read as empty, and every rate was applied as zero. When the bracket closes on such a jump, the rates of its upper bound are now applied, trimmed to the available water by the constraints.
- Rejecting sub-daily runs whose last forcing date differs from the end of the period by a rounding error only (the dates were compared for exact equality): an hourly run from 1981 to 2020, for instance, failed whatever its end date. The dates are now compared within a millisecond.

## 0.9.2 - 2026-10-20

### Breaking changes

- Python 3.11 or later is required (the `StrEnum` back-port dependency is dropped).
- The HBV open-water land cover `lake` is renamed `water` (alias `k_water`); `lake` is reserved for a future regulated-lake cover.

### Added

- PREVAH-UniBE model (`PrevahUniBE`), reproducing most of the PREVAH processes: radiation-corrected Hock melt with seasonal refreezing, liquid water retention, albedo-reduced soil/canopy/snow evaporation, Menzel interception, threshold upper zone, soil-moisture-gated percolation, SLOWCOMP groundwater, wet-surface evaporation, the PREVAH glacier module (ice/firn split at the ELA with translation delays) and the built-in land-use tables (`hydrobricks.land_covers.PREVAH_LAND_USE_*`, `apply_land_use`, `apply_land_use_field_capacity`, `apply_land_use_percolation`). It reproduces the reference Fortran discharge on the Ticino (NSE ~0.985); see `examples/advanced/prevah_ticino*`.
- Sub-daily time steps: `Model.setup(time_step=..., time_step_unit='day'|'hour'|'minute')`. Parameters and process rates stay per day, so calibrated sets carry over; forcing is an amount per step and must match the step resolution. GR4J/GR6J remain daily-only and are rejected otherwise.
- Monthly, spatial (per-unit) and spatial-monthly parameters: `ParameterSet.set_monthly_values`, `set_spatial`, `set_spatial_monthly`.
- `wetland`, `urban` and `rock` land covers in all models (mapped from the ESA WorldCover and CORINE presets), and the `interception_covers` option for a canopy store on any cover.
- New processes usable in any structure: `routing:delay`, `outflow:linear_threshold`, `outflow:split`, `melt:degree_day_seasonal`, `refreeze:degree_day_seasonal`, `interception:menzel`; process gates (`gate` key) reading the state of other bricks.
- Vapour-density Hamon PET (`compute_pet(method="Hamon_vapor_density")`).
- `Forcing.load_spatialized_data_from_csv()` for forcing already resolved per unit or per group of units (dated or day-of-year).
- Project file: `calibration.options` (forwarded to SPOTPY, e.g. SCE-UA's `ngs`), `calibration.database`, a `spatialized` forcing source, a `radiation` section, `hydro_units.connectivity`, an `actions` section (glacier evolution) and per-parameter ranges (`{value, min, max}`).
- Parallel study runs (`--workers N`, `Study.run_all(workers=N)`) with a shared forcing warm-up, per-job worker recycling and `calibration_seconds` in the results.

### Changed

- About nine times faster simulations (Socont, Sitter, 40 years daily: 4.7 s to about 0.5 s): Illinois iteration instead of bisection in the sequential solvers, closed-form affine bricks, and a flattened traversal. Results are unchanged to round-off.
- Link-time optimization on by default (`USE_LTO`), plus an optional profile-guided build (`USE_PGO`, see `ci/pgo_training.py`).
- `Catchment.extract_dem` crops the DEM to the catchment plus a margin, so country-wide DEMs can be used as is.
- Forcing from a project file is trimmed to the modelling period before regridding (regrid caches are recomputed once); gridded sources longer than the other variables are trimmed rather than rejected.
- Project-file calibrations no longer keep each iteration's simulated series in memory (`save_sim`).
- Lateral processes declared without `hydro_units.connectivity` are rejected.

### Fixed

- `day_of_year` gridded sources: trimming by dates and sizing by the simulation period both failed.
- CRS detection of gridded forcing from a CF grid-mapping variable.
- Deprecation warnings from xarray and affine on gridded runs.

## 0.9.1 - 2026-09-03

### Breaking changes

- Project-file YAML keys renamed to match the underlying Python API parameter they map to: `forcing.gridded.<var>.elevation_gradient` → `apply_data_gradient`, `forcing.gridded.<var>.crs` → `data_crs`, `hydro_units.land_cover_areas` → `columns_areas`, `forcing.pet.latitude` → `lat`.

### Added

- Providing wheels for macOS (Apple Silicon, macOS 13.3 or later) on PyPI.
- Adding `GlacierMassBalanceObservations.from_glamos_by_type()` to load the annual, winter and summer mass balances of a GLAMOS file as separate calibration signals, with `name`, `balance_type` and `balance_types` properties to identify them.

### Changed

- Retrieving yaml-cpp through vcpkg instead of building the upstream master branch, so the version is pinned by the manifest.
- Updating the vcpkg dependency baseline.
- Setting the MSVC runtime library explicitly for every build target, so a dependency defaulting to the static runtime can no longer break the link (Windows).
- Ensuring the C++ core compiles with Apple's libc++, which does not provide the C++23 range adaptors used in the solver and logger.
- Reporting an explicit error when a study matrix `include` entry references a dimension value that has no matching variant patch.
- Reporting an explicit error when an auxiliary observation used for calibration contains no data, instead of silently applying the rejection penalty to every run.

### Fixed

- Fixing a double counting of the incoming water in the storage constraints, which let a store be drawn below zero (logged as `Water container <name> has negative content`) and silently created water when the content was clamped back to zero. Bricks computed directly (land covers, snowpacks) book their incoming amounts before the constraints are enforced, and the constraint enforcement was adding those same amounts a second time. Mostly visible on the snowpack liquid store of HBV and PREVAH-UniBE, where the rain is routed to the snowpack and the refreezing takes a content-independent amount: the overdraw appeared when the melting temperature sat inside the rain/snow transition band.
- Fixing the scaling of the simulated glacier mass balance. The ablation term was previously under-counted by one to two orders of magnitude.
- Fixing a crash (segmentation fault) when dumping the outputs of a simulation that records no per–hydro-unit value, for example a project file without a `recordings` section.
- Fixing the release of the raster file handle when upscaling the mean annual potential radiation, which prevented computing the radiation twice into the same output directory on Windows.
- Accepting a DataFrame of hydro units, in addition to a `HydroUnits` object, when extracting the snow cover.


## 0.9.0 - 2026-07-07

### Breaking changes

- The default land cover is now `open` instead of `ground`. `ground` (and `generic`/`generic_land_cover`) are kept as accepted aliases for backward compatibility, but default-run output labels change from `ground:*` to `open:*`.
- The discharge observations class `Observations` is renamed `DischargeObservations`, and the calibration argument `obs` (in `SpotpySetup`) is renamed `discharge`.
- The observation/reference classes moved to a new `hydrobricks.evaluation` subpackage (`DischargeObservations`, `AuxiliaryObservation`, `GlacierMassBalanceObservations`, `evaluate`). They remain importable from the top level (`hb.DischargeObservations`, …); `hydrobricks.observations` is gone.

### Added

- Adding a file-based (yaml) project definition.
- Adding the GR4J model structure and its processes along with the CemaNeige snow model.
- Adding the GR6J model structure and its processes.
- Adding the HBV-96 model structure and its processes.
- Adding custom model structures (based on yaml files).
- Enabling per-hydro-unit model structure variants.
- Automate hydro unit structure assignment from land covers.
- Adding model structure inspection and visualization (a textual `model.summary()` / `model.print_structure()`, and a Graphviz `model.plot_structure()`).
- Introducing parameter transforms for model calibration.
- Adding a Periods class to handle calibration and validation periods.
- Adding dynamic forcing override mechanism (ability to dynamically override forcing values within a simulation timestep).
- Adding a temperature threshold-based snow-rain splitter based on a single temperature threshold (`SplitterSnowRainThreshold`).
- Adding separate snow/rain correction factors.
- Adding rain routing to snowpack liquid water storage (parameter `rain_to_snowpack`).
- Adding open water evaporation process (`et:open_water`). This process evaporates at the full potential rate (PET).
- Adding forest canopy interception with dedicated storage and processes.
- Adding linear, power-law, and exponential actual evapotranspiration.
- Adding Frey & Holzmann (2015) lateral snow redistribution.
- Adding snow sublimation processes.
- Enable calibration with auxiliary observations (e.g., snow coverage or glacier mass balance).
- Enable parallel model calibration with SPOTPY.
- Adding land cover extraction from raster (ESA WorldCover, CORINE) and vector datasets to set hydro unit land-cover fractions.
- Adding catchment discretization by sub-catchments.
- Adding different example scripts.
- Initialize glacier cover from extracted ice thickness rasters.
- Restoring dynamic land cover and glacier state on model reset.
- Adding per-unit total SWE extraction in results.
- Adding in-memory accessors for recorded per–hydro-unit series (no file dump): `model.get_recorded_hydro_unit_values(label)`, `get_recorded_hydro_unit_fractions`, `get_recorded_labels`, `get_recorded_hydro_unit_ids`/`_areas` and `get_recorded_time`.

### Changed

- Generalizing outflow rest process for direct and solver bricks (`ProcessOutflowRest` now correctly determines the remaining water for both direct-computation and solver-driven bricks).
- Refactoring plotting as a class.
- Changing PET method to Oudin in examples.
- Making the parameter aliases case-insensitive.
- Refactoring land covers allowing pre-defined classes (e.g. forest, lake, glacier) and user-defined classes (e.g. urban, wetland).
- Introduce pluggable glacier modules.
- Standardize `open` as default generic land cover (instead of `ground`). `ground` (and `generic`/`generic_land_cover`) are kept as accepted aliases.
- Optimize process rate calculations with reusable buffers.
- Reducing logging verbosity for skipped runs during calibration when parameters are out of range or violate constraints.
- Refactor observation handling for multi-objective calibration.
- When a parameter set is rejected due to constraints, for example, a very bad value is given to the objective function rather than NaNs.

### Fixed

- Water balance is now conserved when a land cover area fraction changes (land-cover change and glacier-evolution actions). The water, snow and canopy content stored on the land that changes hands is transferred between the changed cover and the generic cover that absorbs the area difference, instead of leaving the depth (mm) unchanged over a different area, which silently created or destroyed water.
- Snap near-zero water container content to zero (addresses floating-point precision issues).
- Adding the missing glacier area in examples.


## 0.8.7 - 2026-05-10

### Breaking changes

- The functions model.is_ok() and parameters.is_ok() have been renamed to model.is_valid() and parameters.are_valid() or parameters.is_valid() respectively.

### Added

- Add custom exception classes (C++ and Python).
- Add additional IsValid() functions (C++).
- Add BrickCategory enum (C++).
- Add Has* methods to various classes for better state checking (C++).
- Add TryGet methods to HydroUnit (C++).

### Fixed

- Handle decreasing coordinate axes in TimeSeries2D spatial selection.

### Changed

- Various C++ and Python code refactoring.
- Rename count variables (e.g. GetChangesNb() becomes GetChangesCount() in C++ and get_hydro_unit_nb() becomes get_hydro_unit_count() in Python).
- Externalize results writing from the Logger to a dedicated Results class (C++).
- Provide more information when enums fail to parse (C++).
- Refactor C++ exceptions.
- Improve pointers ownership (C++).
- Refactor Parameters in the C++ core.
- Rename IsOk() to IsValid() in the C++ core.
- Update logging to use the logging module instead of print statements (Python).
- Refactor the Observations class (Python).
- Refactor the trainer class (Python).
- Remove wxWidgets dependency in favor of C++ standard library features.
- Simplify area computation in `compute_area` (Python).
- Improve raster file I/O resource management (Python).
- Centralize object creation with registry pattern (C++).
- Adopt C++23 standard library features (C++).
- Enhance error handling with std::expected (C++).
- Decouple model construction from ModelHydro class (C++).


## 0.8.6 - 2025-12-18

### Added

- Adding computation of ice thickness changes (observation-based delta-h).
- Adding a check that the action has been initialized.
- Checking that the model is initialized before adding actions.
- Adding helpers to the Results class.

### Changed

- Refactoring the parameters definition (no impact for the users).
- Moving examples to the root folder.
- Converting stringly-typed identifiers to enums (no impact for the users).


## 0.8.5 - 2025-08-28

### Added

- Prevents lateral snow slide from causing unrealistic snow accumulation in target areas by limiting redistribution when the target snowpack already exceeds a threshold.


## 0.8.4 - 2025-08-28

### Added

- Adding a snow to ice transformation process for glaciers following the SWAT model approach (Luo et al., 2013).
- Adding a snow to ice transformation action that transforms all snow to ice at a given date (e.g., end of the melt season).

### Fixed

- Fixing an issue when glacier volume is increasing further than the initial volume in the glacier evolution methods.

### Changed

- In the connectivity calculation, the 'force_connectivity' option was changed to False by default due to potential issues in some cases (e.g., exchange back and forth).


## 0.8.3 - 2025-08-14

### Fixed

- Fixing an issue with the gradient-based interpolation for temperature and precipitation when spatializing from gridded data.


## 0.8.2 - 2025-08-12

### Breaking changes

- The extraction of meteorological data from netCDF files (spatialize_from_gridded_data) has changed. The function now computes elevation gradients by default for temperature and precipitation. In order to access to the DEM, the class must be initialized with the catchment object (forcing = hb.Forcing(catchment)) instead of the hydro units object. This gradient-based interpolation can be disabled with apply_data_gradient=False.

### Added

- Adding a gradient-based interpolation for temperature and precipitation when spatializing from gridded data.
- Making the SnowSlide parameters accessible.
- Adding a parameter to the SnowSlide method to control the overall maximum snow depth.

### Fixed

- The actions were not correctly re-initialized when resetting the model. This is now fixed.
- Improving land cover fraction checks in actions.
- Fixing issue related to hydro unit ids when loading from csv file. The hydro units were not retrieved from the file, but assumed increasing, which is wrong when sorted by elevation.
- Fixing hydro unit ids in land cover extraction.
- Fixing index issues related to Pandas iterrows() usage.

### Changed

- Avoid snow redistribution to too small areas.
- Capping the snow redistribution rate to 1000 mm of SWE.


## 0.8.1 - 2025-08-06

### Fixed

- Adding trainer support for failing simulations.
- Fixing precision issue in land cover fraction check.
- Allow for floating-point imprecision in initial glacier extent.


## 0.8.0 - 2025-07-16

### Breaking changes

- The minimum Python version is now 3.10.
- Behaviours were renamed as Actions.
- Changing logging labels for content to type-specific labels ("content" becomes "water_content", "ice" becomes "ice_content", "snow" becomes "snow_content").

### Added

- Adding the delta-h glacier evolution method (Huss et al., 2010) following Seibert et al. (2018) implementation of a look-up table.
- Adding a simple glacier evolution method based on the glacier area and volume changes.
- Adding lateral connections to hydro units in the C++ core.
- Adding the SnowSlide method (Bernhardt & Schulz, 2010) for snow redistribution.
- Adding a snow to ice transformation process (constant rate).
- Adding an option to force the connectivity to neighbors when there is no downstream unit.
- Adding setting an initial state to bricks.
- Allow for sporadic and recursive actions (e.g. glacier area update).
- Adding an option to load the hydro units from the catchment object.

### Fixed

- Fixing an issue with the dlls when installing the module in editable mode.
- Completing documentation of the C++ code.
- Adding type hints to Python function arguments.
- Improving Python code readability.

### Changed

- Updating some example scripts.
- Refactoring the catchment class to extract specific functionalities (no impact for the users).
- Changing raster extraction for a more flexible approach.
- Removing lateral connectivity with weight under 1%.
- Rounding the lateral connectivity weight values.
- Sorting hydro units by elevation to ensure a consistent order for snow redistribution.
- Changing convention for member variables naming in C++.
- Namespacing the global variables.


## 0.7.6 - 2025-01-23

### Added

- A new function to compute the connectivity between hydrological units has been added.
- New reference metrics have been added (bootstrapping approach).

### Fixed

- Fixing issues related to updated dependencies (dask, pyarrow, pysheds).
- Fixing CI issues.


## 0.7.5 - 2024-10-31

### Breaking changes

- The 'isohypse' option has been renamed to 'equal_intervals' in the catchment discretization functions.

### Added

- Adding the option to discretize the catchment by slope intervals.

### Changed

- Using vcpkg for the C++ dependencies instead of conan. This allows building the package for Python >= 3.12.
- The basic requirements are automatically installed when installing the package from PyPI.


## 0.7.4 - 2024-08-13

### Fixed

- Fixing wheels upload to PyPI.


## 0.7.3 - 2024-08-12

### Fixed

- Fixing the package installation from PyPI.
- Fixing issue between pybind11 and numpy 2.


## 0.7.2 - 2024-04-29

### Fixed

- Fixing the computation of the potential radiation for the temperature index melt model.


## 0.7.1 - 2024-04-27

### Added

- Adding a function to compute the units connectivity.
- Adding the cast shadow computation for the potential radiation.


## 0.7.0 - 2024-02-18

### Added

- Adding a new functionality to create a BehaviourLandCoverChange object from shapefiles of glacier extents.
- Adding a function to compute potential clear-sky direct solar radiation as defined by Hock (1999).
- Adding a temperature index melt model based on Hock (1999).
- Adding the possibility to discretize the hydrological units based on the mean annual potential radiation used in Hock (1999).
- Adding an aspect-related degree day factor.
- Adding a reference prediction based on a bootstrapping approach of the observed discharge.
- Adding the option to define a parameter shared by different processes (e.g., snow and ice melt).
- Allow calibrating a single parameter set for multiple catchments.
- Adding a Results class to simplify outputs analysis.
- Adding maps plotting and spatio-temporal animations of the model outputs.
- Handling yearly recurrent forcing with a 'day_of_year' approach.
- Moving the model structure definition from C++ to Python.

### Changed

- Transitioning to a dictionary-based structure definition.
- Moving the parameters generation out of the Socont class.
- Moving the process parameters and forcing definitions to respective classes.
- Changing workflow in load_from_csv.
- Restructuring examples.
- An error is raised if some parameter values are missing.
- The parameter estimation and evaluation functions are restructured to a new trainer.py file.
- Adding the date/time to the log file name.

### Fixed

- Socont quick discharge has been fixed... For real, this time.
- Fixing the SpotpySetup initialization.
- Fixing issue with hydro unit initialization in catchment discretization.
- Fixing a bug that occurred when the study area outline comprised multiple polygons.
- Adapting the FixLandCoverFractionsTotal() function to also work with cases where the hydro unit is fully glaciated at the beginning of the Behaviour time series.


## 0.6.2 - 2023-09-15

### Breaking changes

- The csv files defining the hydro units (e.g., elevation bands) properties have now to include the units of the values (e.g., m for elevation) below the column title.

### Added

- Adding the computation of the slope and aspect from the DEM.
- Transferring slope and aspect values to the hydrological model (e.g., for Socont quick discharge using the unit slope).
- Any hydro unit property can now be extracted from data and provided to the hydrological model (flexible generic approach).
- Adding the computation of the mean latitude and longitude for each hydro unit and allow using it for the PET computation.
- Adding an extraction of the forcing using a faster weighted approach.
- Addition of an example script for the catchment data preparation.
- Allow loading unit ids from raster.
- Adding a hydro unit discretization by elevation and aspect (separately or combined; with an example script).

### Changed

- Refactoring catchment attributes parsing.
- The csv files defining the hydro units (e.g., elevation bands) properties have now to include the units of the values (e.g., m for elevation) below the column title.

### Fixed

- Socont quick discharge has been fixed.


## 0.6.1 - 2023-08-23

### Fixed

- Fixing an issue with the data container shape when using the spatialization from gridded data.


## 0.6.0 - 2023-08-22

### Breaking changes

- Many changes in the Forcing class:
  - load_from_csv() was renamed to load_station_data_from_csv().
  - define_spatialization() was renamed to spatialize_from_station_data() and is only meant for spatialization from station data.
  - correct_station_data() was added and is to be used for applying a correction factor, for example.
  - spatialize_from_gridded_data() was added to load data from gridded netCDF files.
  - compute_pet() was added and uses the pyet package.
  - The operations are not performed immediately, but only applied when apply_operations() is called, which is done automatically before the model run or before saving the forcing data to a netcdf file.
- The Catchment class is now part of the main module.

### Added

- Adding the spatialization from gridded data with spatialize_from_gridded_data().
- Adding PET computation using the pyet library. New function: compute_pet().
- The forcing data can now be loaded from the dumped netCDF file with load_from().

### Changed

- Refactoring of the Forcing and TimeSeries classes to separate 1D and 2D data.
- Enums are used to specify the Forcing variables.
- The Catchment class is now part of the main module.
- The Catchment class can generate a geotiff of the hydro unit ids with save_unit_ids_raster()

### Fixed

- Fixing an issue with the elevation range condition in the elevation bands creation.


## 0.5.0 - 2023-07-06

### Breaking changes

- Removing hyphens for underscores. Any component (including land cover elements) have to use underscores and not hyphens (e.g., glacier_ice instead of glacier-ice, slow_reservoir instead of slow-reservoir)

### Added

- Adding aliases for all parameters.
- Adding examples.
- Adding Socont constraints.

### Changed

- Changing Socont default values.
- Upgrading conan builds.
- Upgrading C++ minimal version.
- Upgrading to Pandas 2.0.

### Fixed

- Code improvements.
- There were issues to build the wheel locally (in editable mode).


## 0.4.11 - 2023-02-17

### Added
- Resetting the behaviour counters when resetting the model.
- Adding land cover fractions to the logger output.

### Fixed
- Fixing issue of behaviour dates not being correctly sorted.
- Fixing issue with dates in land cover behaviours.


## 0.4.10 - 2023-02-17

### Fixed

- Fixing ground area compensation in land cover changes.
- Fixing flux weighting when applying land cover changes.


## 0.4.9 - 2023-02-15

### Added

- Addition of functionalities for behaviours.
- Remove empty changes in land cover changes behaviour.

### Fixed

- Fixing issue with behaviours not triggering.


## 0.4.8 - 2023-01-18

### Fixed

- Fixing precision issue with infinitesimal snow blocking ice melt.
- Fixing issue with precipitation gradient approach with threshold.


## 0.4.7 - 2023-01-18

### Added

- Addition of forcing dumping from SPOTPY.


## 0.4.6 - 2023-01-17

### Added

- Addition of some parameter constraints to Socont.
- Addition of a function to remove a constraint.
- Addition of the constraints listing.

### Fixed

- Fixing dump path creation in SPOTPY.


## 0.4.5 - 2023-01-16

### Changed

- Check parameter constraints in SPOTPY and handle non-conforming cases.


## 0.4.4 - 2023-01-13

### Changed

- Improvement of the parameter constraints handling in SPOTPY.


## 0.4.3 - 2023-01-10

### Changed

- Allow using HydroErr metrics in SPOTPY.


## 0.4.2 - 2023-01-09

### Changed

- Adding an option to dump outputs from SPOTPY.
- Homogenisation of parameter naming in the spatialization of forcings.


## 0.4.1 - 2022-12-23

### Fixed

- Fixing the warmup split when using SPOTPY.


## 0.4.0 - 2022-12-23

### Added

- Addition of a land cover evolution in time.


## 0.3.1 - 2022-12-16

### Added

- Addition of the check of the parameters constraints with SPOTPY.
- Users can now define the prior distribution for SPOTPY to use.

### Changed

- Removing the old Monte Carlo implementation.


## 0.3.0 - 2022-12-07

### Added

- Integration with SPOTPY.

### Changed

- The selection of the parameters to calibrate is now handled by the ParameterSet class.


## 0.2.0 - 2022-12-01

### Added

- Addition of a catchment preprocessing module to build the elevation bands.

### Changed

- Some Python packages are made optional (e.g., netCDF4) and hydrobricks can be used without these.
- The water balance of the Socont model is fully tested.

### Fixed

- Fixed issues with the water balance computation.


## 0.1.4 - 2022-11-28

### Added

- Addition of functions to extract the water balance components.
- Addition of static content change rate (vs dynamic; e.g. for instantaneous fluxes).

### Changed

- The parameter names are now using snake_case.
- The fraction assignment has been moved to the model initialization (so it's not forgotten).

### Fixed

- Fixed issues with fluxes and the water balance.


## 0.1.3 - 2022-11-18

### Added

- Added checks to ensure that each container receiving water has a process attached.
- Creation of specific water containers for snow and ice.

### Changed

- New approach to surface bricks which are now separated into land use and surface components (e.g., snowpack).

### Fixed

- Fixed issue related to processes resolved out of the solver.


## 0.1.2 - 2022-11-12

### Added

- Adding state variables initialization.

### Fixed

- Reinitialize the state of the storages after a model run.


## 0.1.1 - 2022-11-07

### Fixed

- Fixed issue in the function to change the range of the parameter values.


## 0.1.0 - 2022-10-31
First preliminary release.
