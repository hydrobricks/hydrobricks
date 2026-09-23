# Multi-subbasin routing design

Design for running a catchment as a tree of subbasins connected by river reaches,
with channel routing between them and discharge available at every subbasin outlet.
Status: agreed 2026-09-18. PR 1 to 5 implemented.

Companion documents: `solver_analysis.md` (the solver contract) and the gap review of
2026-09-18 (this is Tier 1, item 1).

## Decisions taken (2026-09-18)

- **Naming: `subbasin`.** The single word is what SWAT, HEC-HMS, HYPE and Raven use.
  All user-facing identifiers use it (`attach_to: subbasin`, `add_subbasin_brick`,
  `subbasin_values` in result files, `Results.list_subbasin_components`, the
  `subbasin` hydro-unit column). The old `sub_basin` spellings stay accepted on input
  for one release. The C++ class `SubBasin` keeps its name (internal only); a separate
  mechanical rename is possible later.
- **"Channel routing", never "routing" alone.** The routing along the reaches is the
  *channel routing* (option `channel_routing`, parameter component `channel`, C++
  `ChannelRoutingSettings`), as opposed to the in-catchment `routing:*` processes of a
  structure (unit hydrographs, MAXBAS, translation delay), which keep their names.
- **Routing is not a brick.** Reaches are first-class elements of the river network
  with their own geometry and a pluggable routing scheme (§3, D3). Bricks stay what
  they are: mm-based stores of the land phase.
- **Reach geometry comes from data**, not from calibration: a river-network shapefile
  gives the segments and their lengths, the DEM gives the slope. Calibration only
  touches the routing scheme's parameters (celerity, Muskingum X, Manning n).
- Interior gauges in m³/s are accepted, converted with the subbasin's drained area.
- Local runoff of a subbasin joins the routed upstream inflow at the subbasin outlet
  (the Raven convention). Routing through the reach applies to the upstream inflow
  only. See D6 for the reasoning; an option to route local runoff too can come later.
- Subbasins may use different structure variants (e.g. a glacier sub-model only where
  glaciers exist). Per-unit variants already exist; extending the mechanism to
  subbasin-level bricks is deferred to a later PR (PR 2 builds structure 1 in every
  subbasin).
- The network lives in the same `hydro_units.nc` file as the hydro units.

---

## 1. Goals and non-goals

Goals:

- Simulate a catchment as several subbasins, each with its own hydro units and its own
  instance of the model structure, connected in a tree (each subbasin drains into at
  most one downstream subbasin).
- Route the water between subbasins through channel reaches with a selectable routing
  scheme: instantaneous, pure translation, Muskingum; later Muskingum-Cunge or
  kinematic wave using the reach slope and width.
- Record discharge at every subbasin outlet (interior gauges), in a form the existing
  evaluation and calibration code can consume.
- Keep single-subbasin runs bit-for-bit identical to today.
- Keep the land-phase machinery unchanged: bricks, processes, fluxes, solver, logger,
  actions, spin-up and calibration must not need special cases for the network.

Non-goals (later tiers):

- Braided networks, diversions and inter-basin transfers. These become explicit
  network elements once reservoirs exist (Tier 2).
- Reservoirs and lakes on reaches (Tier 2), full hydraulic routing.
- Parallel evaluation of independent branches (Tier 3). The design must not prevent it.
- Nested calibration strategies (upstream first, then downstream). That is a Python
  workflow on top of per-gauge evaluation, not a core feature.

## 2. Where the code stands

Facts that constrain the design, with the code that establishes them:

- `ModelHydro` holds exactly one `SubBasin` (`core/src/base/ModelHydro.h`,
  `_subBasin`). `Processor::ConnectToElementsToSolve` and `ProcessTimeStep` walk that
  one basin (`core/src/base/Processor.cpp:62`, `:304`).
- `Connector` (`core/src/spatial/Connector.h`) only stores two `SubBasin*` and registers
  itself on both. Nothing calls it, and `SubBasin::HasIncomingFlow` is never true.
- The outlet is a sum of `FluxToOutlet` amounts (`SubBasin::ComputeOutletDischarge`,
  `core/src/spatial/SubBasin.cpp:322`). Hydro-unit fluxes are pre-weighted by
  `unit area / subbasin area` (`ModelBuilder.cpp:538`, `:706`), so the outlet total is
  in **mm per time step over the subbasin area**, not a volume.
- The logger stores subbasin values as one series per label
  (`Logger::_subBasinValues`, `vecAxd`) and the result file has no subbasin dimension
  (`ResultWriter.cpp:30-77`).
- `Logger::GetOutletDischarge` looks up the label `"outlet"`; Python evaluation and
  calibration read `model.get_outlet_discharge()` (`periods.py:360`, `trainer.py:1062`).
- Forcing is attached per hydro-unit id (`ModelHydro::AttachTimeSeriesToHydroUnits`), so
  any partition of the units into subbasins keeps working for free.
- Lateral connections exist between hydro units only, for snow
  (`HydroUnitLateralConnection`, `ProcessLateral`, built at `ModelBuilder.cpp:545`).
- `ProcessRoutingDelay` (`core/src/processes/ProcessRoutingDelay.cpp`) is a
  schedule-based translation. Its delivery-schedule logic is the template for the
  translation routing scheme, but it lives in a process and works in mm.
- `SettingsBasin::Parse` reads `hydro_units.nc` (`id`, `area`, `elevation`, one variable
  per land cover, any other 1-D double variable as a property). Python writes that file in
  `HydroUnits.save_as`.
- The project YAML schema has no network concept; `observations` is a single file and
  column (`project.py:1453`).
- Test data already contains a nested pair: Sitter at Appenzell (CAMELS-CH 2112) lies
  inside Sitter at St. Gallen (CAMELS-CH 2468), both with discharge, meteo, DEM and
  outline. This is the natural end-to-end test case.

## 3. Design

### D1. Topology: a tree of subbasins, stored in `SettingsBasin`

- A subbasin has an integer `id` (> 0), a `downstream_id` (0 = terminal outlet) and an
  optional name. Exactly one terminal subbasin is allowed per model.
- Every hydro unit carries a `subbasin_id` (default 1). A model without subbasin
  settings gets one implicit subbasin containing all units: the current case.
- Validation (`SettingsBasin::ValidateNetwork`): unique ids, no cycles, one root, every
  referenced id exists, every subbasin has at least one hydro unit, every unit points
  to a declared subbasin.
- Subbasins carry double and string properties like hydro units do (reach length,
  slope, gauge name, ...). They are parsed from any 1-D variable on the `subbasins`
  dimension of `hydro_units.nc`.
- File format (`hydro_units.nc`, version 1.1): an integer variable `subbasin` on the
  `hydro_units` dimension; a `subbasins` dimension with `subbasin_id`,
  `subbasin_downstream_id`, a `subbasin_names` attribute and one variable per
  property. Files without these parse as one subbasin.

Why a tree and not a DAG: river networks are trees; a DAG would force fractional
splits at every node and complicate the run order for no current use. Diversions come
later as explicit network elements.

### D2. Units: subbasins compute in mm over their local area, the network in volumes

The whole brick/process machinery works in mm over the subbasin area. Keep that.

- `SubBasin::_outletTotal` stays the subbasin's local outflow in mm over its **local**
  area (own hydro units only). For a single subbasin, nothing changes.
- The network converts on transfer: `volume = outletTotal · localArea`. Reaches route
  volumes (m³ per step). Nothing downstream of the land phase is ever in mm.
- Reported discharge per subbasin is derived from the outlet volume: `outlet` in mm
  over the **drained** area (local plus all upstream, which is what a gauge normalized by
  its catchment area shows and what `discharge.csv` files already contain), plus
  `outlet_volume` in m³ per step. `outlet` keeps its name and meaning for the terminal
  subbasin, so `get_outlet_discharge()` and every existing evaluation path stay valid.
  Observations in m³/s are converted with the drained area by the existing
  `DischargeTransform` layer.

`SubBasin` gains `GetLocalArea()` (today's `_area`) and `GetDrainedArea()` (computed
once from the tree). `GetArea()` keeps returning the local area, since that is what the
flux weighting at `ModelBuilder.cpp:538` needs.

### D3. Reaches: network elements with geometry and a routing scheme

A `Reach` belongs to exactly one subbasin: it is the main channel from the point where
the upstream subbasins enter to the subbasin outlet. Headwater subbasins have a reach
too (used only if local runoff routing is enabled later).

```
                upstream subbasins
   local runoff        │ volumes
   (mm → m³) ─┐        ▼
              │   ┌─────────┐   routed inflow
              │   │  Reach  │──────────────┐
              │   └─────────┘              ▼
              └───────────────────────► outlet node ──► downstream reach
```

- **Geometry** (properties, from preprocessing): `length` [m], `slope` [-], optional
  `width` [m], `manning_n` [-], `elevation_upstream`/`elevation_downstream` [m]. Stored
  as subbasin properties in `hydro_units.nc`.
- **Routing scheme** (strategy object, one per reach, chosen model-wide with per-reach
  override):

| Scheme | Parameters | Behaviour |
| --- | --- | --- |
| `none` | – | Instantaneous pass-through. Default. |
| `lag` | `celerity` [m/s] | Pure translation by `length / celerity`, with the delivery schedule of `ProcessRoutingDelay` moved into a shared helper. |
| `muskingum` | `celerity` [m/s], `x` [-] | `K = length / celerity`; `S = K[X·I + (1−X)·O]`, integrated per step with the standard Muskingum coefficients (exact for the linear scheme, no ODE solver needed); outflow clamped at 0. |
| `muskingum_cunge` | `width` [m], `manning` | DONE. `K`, `X` recomputed each step from slope, width, Manning and the discharge (Manning depth, kinematic celerity 5/3·v, Cunge's X). Needs no calibration. |

All schemes but `muskingum_cunge` accept a discharge-dependent celerity through `celerity_exponent`
and `reference_discharge`; every celerity is clamped to [0.05, 10] m/s. The local runoff of a subbasin
is routed through half the reach when `route_local_runoff` is set (a second routing branch, D6).

- **State**: reach storage (and the schedule for `lag`), reset and saved with the
  model state like brick containers (`Reset`, `SaveAsInitialState`).
- **Parameters**: registered in `SettingsModel` under a `routing` component
  (`routing:celerity`, `routing:x`, `routing:manning_n`) so that the Python
  `ParameterSet` can expose, constrain and calibrate them exactly like process
  parameters; per-reach spatial overrides read subbasin properties, mirroring
  `set_parameter_spatial_from_property` for hydro units.
- **Time step**: the schemes work at the model step. Muskingum stability
  (`2KX ≤ Δt ≤ 2K(1−X)`) is checked at initialization and reported; a reach shorter
  than `celerity · Δt` is treated as instantaneous with a debug message.

Why not a brick: a brick is a store in mm over an area and is integrated with the
subbasin's ODE system; a reach is a channel in m³ with its own geometry, and there may
later be several per subbasin. Keeping routing out of the solver also keeps the solver
ordering rule out of the picture and makes the network trivially parallelizable across
independent branches.

### D4. Execution: `RiverNetwork` runs subbasins upstream-first inside one time step

A new `RiverNetwork` class (owned by `ModelHydro`, replacing `_ownedSubBasin`) holds:

- `vector<unique_ptr<SubBasin>>` plus an id map, each with its `Reach`;
- the topological order (upstream first), computed once; the drained areas;
- a global hydro-unit id map so lateral snow connections may cross subbasin
  boundaries (they stay hydro-unit-to-hydro-unit and do not touch the network).

Per time step, `ModelHydro::Run` does, for each subbasin in topological order:

1. `processor.ProcessTimeStep(dt)` for this subbasin (direct pass, solve, local outlet
   in mm);
2. `reach.Route(inflowVolume, dt)` where `inflowVolume` is the sum of the outlet
   volumes of the upstream subbasins, all computed earlier in this same step;
3. `outletVolume = routedInflow + localOutlet · localArea`, stored on the subbasin for
   the downstream reach and for logging.

Each subbasin gets its own `Processor` and `Solver` instance, because its state vector
and traversal tables are independent. The single-subbasin case is the loop with one
element and a `none` reach, so it cannot change numerically.

Coupling is explicit and one-directional (upstream never sees downstream), so the
sequential order gives the exact result. There is no one-step lag artefact: routed
water leaves the upstream subbasin and enters the reach within the same step, and the
routing scheme adds the physical delay. Siblings in the tree can run in parallel later.

### D5. Hydro units, forcing, actions: unchanged

- Hydro units keep global ids; only the `subbasin_id` property is new. Forcing
  attachment, land cover change, glacier evolution and snow redistribution work per
  unit and need no change.
- `SubBasin::AssignFractions`, `Reset`, `SaveAsInitialState`,
  `RestoreInitialAreaFractions` are called per subbasin by `RiverNetwork`.
- Spin-up (`RunSpinup`, `RewindAfterSpinup`) loops over time; each step goes through
  the same network loop as `Run`.

### D6. Model structure and parameters: one structure, per-subbasin instances

- The `SettingsModel` structure is shared. Subbasin-level bricks (e.g. Socont's
  `slow_reservoir`) are instantiated once per subbasin, exactly as hydro-unit bricks
  are instantiated once per unit. Structure variants may differ per subbasin: the
  variant mechanism (`AddStructure`, per-unit `structure_id`) gets a subbasin
  counterpart for the subbasin-level bricks (auto-assigned from the land covers present
  in the subbasin, like today's per-unit assignment).
- Parameters are shared by default. Per-subbasin values use the spatial mechanism
  extended to subbasin properties.
- Local runoff and routed inflow meet at the outlet node (Raven convention: the
  in-catchment travel time of local runoff is already represented by the subbasin's own
  stores and routing processes; the reach represents the channel between the upstream
  outlet and this outlet). Routing local runoff through the reach would double-count
  the transit for water that enters near the outlet. If a user wants it, a
  `route_local_runoff` option on the reach can be added later.

### D7. Logging and output: a subbasin dimension

- `Logger` keeps one label set for subbasin values, stored as
  `[label][subbasin][time]`, mirroring the hydro-unit layout. Labels absent in a
  subbasin stay NaN, as per-structure hydro-unit labels do today. Reach values
  (`reach:inflow`, `reach:outflow`, `reach:storage`, mm over the drained area) are
  logged whenever the channel routing is on.
- `ResultWriter` adds a `subbasins` dimension with `subbasin_ids`,
  `subbasin_downstream_ids`, `subbasin_local_areas`, `subbasin_drained_areas`, and
  writes `subbasin_values[aggregated_values, subbasins, time]`. The file `version`
  attribute is bumped; `Results` (Python) reads both layouts and squeezes the subbasin
  axis when there is one subbasin, so existing scripts keep working.
- `ModelHydro::GetOutletDischarge()` returns the terminal subbasin's `outlet`.
  New: `GetSubbasinDischarge(id)`, `GetSubbasinIds()`, `GetSubbasinAreas()` (local and
  drained). Bound to Python as `get_subbasin_discharge(id)` and friends.
- Water-balance totals (`GetTotalET`, storage changes) become area-weighted sums over
  subbasins so that `get_total_*` remain catchment totals in mm.

### D8. Evaluation and calibration: gauges are observations with a subbasin id

- `DischargeObservations` gains an optional `subbasin` (int id or name). `evaluate`,
  `evaluate_periods` and `SpotpySetup` fetch the simulation with
  `get_subbasin_discharge(id)` when set, else the outlet as today.
- `SpotpySetup` accepts a list of gauge observations. Each gauge is an objective
  term; the existing multi-objective and weighted-combination machinery handles the
  combination. No new algorithm code.
- Nested calibration (upstream gauge first, freeze, then downstream) is an example
  script, using `allow_changing` on the parameter set.

### D9. Python API, preprocessing and project file

- `HydroUnits`: optional `subbasin` column (picked up from the CSV like `slope`, or
  mapped through `columns:`), `set_subbasins(table)` with columns `id`, `downstream`,
  optional `name` and any property columns (`length`, `slope`, ...). `save_as` writes
  the new variables; `get_subbasin_ids()`.
- `Catchment.delineate_subbasins(outlets, river_network=None)` (new, in
  `preprocessing/`):
  1. snap the outlet/gauge points to the river network (or to the flow-accumulation
     stream if no shapefile is given);
  2. delineate each subbasin with pysheds from its outlet, upstream-first, so that
     nested outlets carve the upstream area out of the downstream subbasin;
  3. assign hydro units by majority pixel; a unit spanning two subbasins is split
     with the existing discontinuous-unit mechanism;
  4. clip the river network to each subbasin, keep the main stem from the entry
     node(s) to the outlet, measure its length, and take the slope from the DEM
     elevations at its ends (fallback: regression along the profile), plus optional
     width from the shapefile attributes;
  5. write the subbasin table, `subbasin_ids.tif` and the clipped reaches.
- Project YAML:

```yaml
subbasins:
  file: subbasins.csv          # id, downstream, name, length [m], slope [-], ...
  hydro_units_column: subbasin # column of the hydro units file (default: subbasin)
  routing: muskingum           # none | lag | muskingum (default: none)

observations:                  # a single mapping stays valid (terminal outlet)
  - {file: q_stgallen.csv, time: {column: Date, format: "%d/%m/%Y"}, column: Q, subbasin: 1}
  - {file: q_appenzell.csv, time: {column: Date, format: "%d/%m/%Y"}, column: Q, subbasin: 2, units: m3/s}

parameters:
  routing_celerity: 1.2        # alias of routing:celerity (shared) or spatial: {property: celerity}
  routing_x: 0.2
```

`validate` cross-checks: every `subbasin` in observations exists, the hydro-units
column references only declared ids, `routing` other than `none` requires the reach
geometry and parameters.

## 4. Work plan (one reviewable PR each)

| PR | Content | Acceptance |
| --- | --- | --- |
| 1 | `subbasin` naming (user-facing, old spellings accepted), `SubbasinSettings` and `subbasin_id` on hydro units, `ValidateNetwork`, `hydro_units.nc` round trip, `RiverNetwork` owning the (single) `SubBasin` with topological order and drained areas, `HydroUnits.set_subbasins` and the `subbasin` column. A model with more than one subbasin is refused at initialization with a clear message. | All C++ and Python tests pass unchanged; new tests build a 4-subbasin tree from settings and check order, drained areas, cycle and root errors, cross-subbasin hydro-unit lookup, and the file round trip. |
| 2 | DONE. `Reach` with the `none` scheme, per-subbasin `Processor`, run loop over the network, volume transfer, logger and result file subbasin dimension, `Results` reader, `get_subbasin_discharge`. Subbasin-level structure variants were deferred to a later PR (every subbasin builds structure 1; hydro-unit variants work as before). | Sitter St. Gallen split into Appenzell + remainder with `none` routing reproduces the single-basin run's outlet series to 1e-10 (mass conservation); `subbasin_values` round-trips through `Results`. |
| 3 | DONE. `lag` and `muskingum` schemes on `Reach`, `RoutingSettings` in `SettingsModel` (component `routing`), the `routing` model option and its `ParameterSet` entries, sub-stepping for the Muskingum stability bound, reach logging labels, per-reach `celerity` / `muskingum_x` property overrides. | Unit test against the analytical Muskingum response to a pulse; daily vs hourly convergence test like `test_time_step.py`. |
| 4 | DONE. Gauge-aware `DischargeObservations` (`subbasin`, `units="m3/s"` + `drained_area`), scored at the subbasin outlet by `Model.eval`, `evaluate_periods` and `SpotpySetup` (primary or additional signal through the auxiliary-observation machinery); project YAML `subbasins` section and observation lists (`project.gauges`); validation in `load_project` (the CLI reuses it); docs. | Calibration example with two gauges on the Sitter runs; docs build clean. |
| 5 | DONE. `Catchment.delineate_subbasins` (`CatchmentNetwork` in `preprocessing/catchment_network.py`): outlets snapped to the accumulation-based streams or to a river shapefile, nested delineation, implicit catchment-outlet subbasin, unit assignment by majority or split, reach length along the D8 path and slope from the DEM; `save_subbasin_ids_raster`; example `delineate_subbasins.py`. Not done: reach length taken from the shapefile geometry (the D8 path is used in all cases). | Example produces the Sitter tree from the two outlet points; reach lengths match the shapefile within 1 %. |

Later, outside this plan: OpenMP over sibling subbasins (Tier 3), reservoirs and
diversions as network elements (Tier 2), Muskingum-Cunge and kinematic wave.

## 5. Risks and mitigations

- **Output format change.** Old result files lack the subbasin dimension. Mitigation:
  `version` attribute and a dual-layout reader in `Results`; the one-subbasin file keeps
  today's shape by squeezing on read.
- **Area weighting mistakes.** The mm/volume conversion is the one place where errors
  hide. Mitigation: the PR 2 mass-conservation test (split basin equals lumped basin with
  `none` routing) catches any factor error; add an assertion that the outlet volume of
  the terminal subbasin equals the sum of local runoff volumes when routing is
  transparent.
- **Per-subbasin processors and memory.** Each processor holds traversal tables and a
  solver; with hundreds of subbasins this is still small next to per-unit state.
- **Spatial parameters keyed on hydro-unit properties.** Subbasin properties are a
  second namespace. Mitigation: a separate setter with an explicit name, no fallbacks.
- **Hydro units crossing subbasin borders** after delineation. Mitigation: split via
  the existing discontinuous-unit mechanism, documented.
- **Reach geometry quality.** A coarse DEM gives noisy slopes on flat reaches.
  Mitigation: slope from the end-point elevations with a floor (e.g. 1e-4) and the
  profile-regression fallback; the shapefile attributes win when present.

## 6. Resolved questions

Kept for the record; the answers are in "Decisions taken" above.

1. Interior gauges in m³/s: accepted, converted with the drained area.
2. Local runoff joins at the outlet; routing applies to upstream inflow only.
3. Per-subbasin structure variants: allowed (PR 2).
4. Naming: `subbasin`.
5. Network stored in `hydro_units.nc`.
