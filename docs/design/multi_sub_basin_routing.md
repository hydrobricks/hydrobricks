# Multi-sub-basin routing design

Design for running a catchment as a tree of sub-basins connected by river reaches,
with channel routing between them and discharge available at every sub-basin outlet.
Status: draft for discussion, 2026-09-18. Nothing here is implemented yet.

Companion documents: `solver_analysis.md` (the solver contract the reaches must
respect) and the gap review of 2026-09-18 (this is Tier 1, item 1).

---

## 1. Goals and non-goals

Goals:

- Simulate a catchment as several sub-basins, each with its own hydro units and its own
  copy of the model structure, connected in a tree (each sub-basin drains into at most
  one downstream sub-basin).
- Route the water between sub-basins through a channel reach with a selectable routing
  scheme: instantaneous, pure translation, Muskingum.
- Record discharge at every sub-basin outlet (interior gauges), in a form the existing
  evaluation and calibration code can consume.
- Keep single-sub-basin runs bit-for-bit identical to today.
- Keep the core machinery unchanged where it can be: bricks, processes, fluxes, solver,
  logger, actions, spin-up and calibration must not need special cases for the network.

Non-goals (later tiers):

- Braided networks, diversions and inter-basin transfers (a sub-basin with two
  downstream targets). These become actions or splitters on a reach once reservoirs
  exist (Tier 2).
- Reservoirs and lakes on reaches (Tier 2), kinematic wave and hydraulic routing.
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
  `unit area / sub-basin area` (`ModelBuilder.cpp:538`, `:706`), so the outlet total is
  in **mm per time step over the sub-basin area**, not a volume.
- The logger stores sub-basin values as one series per label
  (`Logger::_subBasinValues`, `vecAxd`) and the result file has no sub-basin dimension
  (`ResultWriter.cpp:30-77`: dimensions `time`, `hydro_units`, `aggregated_values`,
  `distributed_values`, `land_covers`).
- `Logger::GetOutletDischarge` looks up the label `"outlet"`; Python evaluation and
  calibration read `model.get_outlet_discharge()` (`periods.py:360`, `trainer.py:1062`).
- Forcing is attached per hydro-unit id (`ModelHydro::AttachTimeSeriesToHydroUnits`), so
  any partition of the units into sub-basins keeps working for free.
- Lateral connections exist between hydro units only, for snow
  (`HydroUnitLateralConnection`, `ProcessLateral`, built at `ModelBuilder.cpp:545`).
- `ProcessRoutingDelay` (`core/src/processes/ProcessRoutingDelay.cpp`) is a
  schedule-based translation that reads its inflow from the container's incoming rates
  and same-step amounts. It is the template for reach routing processes.
- The solver contract (`solver_analysis.md` §2, memory note on flux ordering): fluxes
  must flow toward later-declared solver bricks. A reach brick must therefore be declared
  after every brick that feeds it.
- `SettingsBasin::Parse` reads `hydro_units.nc` (`id`, `area`, `elevation`, one variable
  per land cover, any other 1-D double variable as a property). Python writes that file in
  `HydroUnits.save_as`.
- The project YAML schema has no network concept; `observations` is a single file and
  column (`project.py:1453`).
- Test data already contains a nested pair: Sitter at Appenzell (CAMELS-CH 2112) lies
  inside Sitter at St. Gallen (CAMELS-CH 2468), both with discharge, meteo, DEM and
  outline. This is the natural end-to-end test case.

## 3. Design decisions

### D1. Topology: a tree of sub-basins, stored in `SettingsBasin`

- A sub-basin has an integer `id` and a `downstream_id` (0 or absent = terminal outlet).
  Exactly one terminal sub-basin is allowed per model.
- Every hydro unit carries a `sub_basin_id` (default 1). A model without sub-basin
  settings gets one implicit sub-basin containing all units: this is the current case.
- Validation at build time: no cycles, one root, every referenced id exists, every
  sub-basin has at least one hydro unit.
- Stored as `SubBasinSettings {int id; int downstreamId; string name;}` in
  `SettingsBasin`, with `AddSubBasin(id, downstreamId, name)` and a new optional
  argument on `AddHydroUnit(id, area, elevation, subBasinId = 1)`.
- File format: `hydro_units.nc` gains an integer variable `sub_basin` on the
  `hydro_units` dimension and a `sub_basins` dimension with `sub_basin_id`,
  `sub_basin_downstream_id` and a `sub_basin_names` attribute. Old files without these
  parse as one sub-basin.

Why a tree and not a DAG: river networks are trees; a DAG would force fractional
splits at every node and complicate the topological run order for no current use.
Diversions come later as explicit splitters on a reach.

### D2. Units: sub-basins compute in mm over their local area, connectors carry volumes

The whole brick/process machinery works in mm over the sub-basin area. Keep that.

- `SubBasin::_outletTotal` stays the sub-basin's outflow in mm over its **local** area
  (own hydro units only). For a single sub-basin, nothing changes.
- A `Connector` converts on transfer: `volume = outletTotal * localArea(upstream)`,
  then delivers `volume / localArea(downstream)` mm into the downstream reach brick.
- Reported discharge per sub-basin comes in three forms, all derived from the same
  volume: `outlet` in mm over the **drained** area (local plus all upstream, which is
  what a gauge normalized by its catchment area shows and what `discharge.csv` files
  already contain), `outlet_local` in mm over the local area, and `outlet_volume` in m³
  per step. `outlet` keeps its name and meaning for the terminal sub-basin, so
  `get_outlet_discharge()` and every existing evaluation path stay valid.

`SubBasin` gains `GetLocalArea()` (today's `_area`) and `GetDrainedArea()` (computed
once from the tree). The existing `GetArea()` keeps returning the local area, since that
is what the flux weighting at `ModelBuilder.cpp:538` needs.

### D3. Routing lives in ordinary bricks and processes, not in the connector

A reach is a sub-basin-level brick of type `storage` named `reach` (name reserved like
`outlet`), carrying one routing process whose output targets `outlet`. The model
structure declares it once; the builder instantiates it only in sub-basins that have an
upstream inflow. The connector is a dumb carrier: it moves the upstream volume into the
reach as an **instantaneous amount** for the current step.

Why: the reach then gets solver integration, parameter handling (`reach:k`,
`reach:x`), spatial overrides, logging, calibration and spin-up for free. Putting
routing inside `Connector` would duplicate all of that.

Processes available on the reach (all sub-basin-level, all `outflow`-family):

| Process | Parameters | Behaviour |
| --- | --- | --- |
| `routing:none` | none | Instantaneous pass-through; the reach is transparent. Default. |
| `routing:delay` (existing) | `delay` [d] | Pure translation with the existing delivery schedule. |
| `routing:muskingum` (new) | `k` [d], `x` [-] | Storage `S = K[X·I + (1−X)·O]`, so `O = (S − K·X·I) / (K·(1−X))`, clamped at 0. `x = 0` reduces to a linear reservoir. |

Muskingum as an ODE fits the solver directly: the process rate depends on the state
`S` and on the inflow `I`, which the process reads exactly as `ProcessRoutingDelay`
does (`SumIncomingChangeRates` + `SumIncomingAmounts / dt`). The implicit solvers keep
it stable for any `K`; the explicit ones need `dt < 2K(1−X)`, which the existing
stability warnings can report. Kinematic wave is out of scope.

Reach parameters may be spatial per sub-basin (see D6) so that `k` can be derived
from reach length and celerity in preprocessing.

### D4. Execution: topological order inside one time step, one `Processor` per sub-basin

A new `RiverNetwork` class (owned by `ModelHydro`, replacing `_ownedSubBasin`) holds:

- `vector<unique_ptr<SubBasin>>` plus an id map;
- `vector<unique_ptr<Connector>>`, one per non-root sub-basin;
- the topological order (upstream first), computed once;
- a global hydro-unit id map so lateral snow connections may cross sub-basin
  boundaries (they stay hydro-unit-to-hydro-unit and do not touch the network).

Per time step, `ModelHydro::Run` does, for each sub-basin in topological order:

1. `connector->Transfer()` for each incoming connector: read the upstream
   `outlet_volume` computed earlier in this same step, deposit it as an instantaneous
   amount into this sub-basin's `reach` brick.
2. `processor.ProcessTimeStep(dt)` for this sub-basin (direct pass, solve, outlet).

Each sub-basin gets its own `Processor` and `Solver` instance, because its state
vector and traversal tables are independent. The single-sub-basin case is the loop with
one element and no connectors, so it cannot change numerically.

Coupling is explicit and one-directional (upstream never sees downstream), so
sequential order gives the exact same result as any other scheme. There is no
one-step lag artefact: the routed water enters the reach in the step it leaves the
upstream sub-basin, and the routing process adds the physical delay. Independent
branches (siblings in the tree) are trivially parallelizable later.

### D5. Hydro units, forcing, actions: unchanged

- Hydro units keep global ids; only the `sub_basin_id` property is new. Forcing
  attachment, land cover change, glacier evolution and snow redistribution work per
  unit and need no change.
- `SubBasin::AssignFractions`, `Reset`, `SaveAsInitialState`,
  `RestoreInitialAreaFractions` are called per sub-basin by `RiverNetwork`.
- Spin-up (`RunSpinup`, `RewindAfterSpinup`) already loops over time; it loops over the
  network inside each step through the same code path as `Run`.

### D6. Model structure and parameters: one structure, per-sub-basin instances

- The `SettingsModel` structure is shared. Sub-basin bricks (e.g. Socont's
  `slow_reservoir`) are instantiated once per sub-basin, exactly as hydro-unit bricks
  are instantiated once per unit.
- Parameters are shared by default. Per-sub-basin values use the existing spatial
  mechanism: `set_parameter_spatial_from_property` reads hydro-unit properties today;
  it gets a sub-basin counterpart reading sub-basin properties (`reach_length`,
  `celerity`, or a direct `reach:k`). `SettingsBasin` therefore also stores
  double/string properties per sub-basin, parsed from `hydro_units.nc` like unit
  properties.
- The `reach` brick is added to the structure by the base `Model` class when a
  network is present (like snowpacks and canopies are generated), declared **last**
  among sub-basin bricks so that every store feeding `outlet` is declared before it
  (solver ordering rule). The `outlet` target of the existing stores is not
  rewritten: in a sub-basin with a reach, the builder redirects `outlet` fluxes of the
  upstream connector only, while the sub-basin's own stores keep draining straight to
  the outlet. Local runoff therefore joins the routed inflow at the sub-basin outlet,
  which is the usual semi-distributed convention (routing represents the upstream
  reach, local runoff is already lumped at the sub-basin outlet).

### D7. Logging and output: a sub-basin dimension

- `Logger` keeps one label set for sub-basin values, but stores them as
  `[label][sub_basin][time]`, mirroring the hydro-unit layout. `SetSubBasinValuePointer`
  gains a sub-basin index. Labels absent in a sub-basin (no `reach`) stay NaN, exactly as
  per-structure hydro-unit labels do today.
- `ResultWriter` adds a `sub_basins` dimension with `sub_basin_ids`,
  `sub_basin_downstream_ids`, `sub_basin_local_areas`, `sub_basin_drained_areas`, and
  writes `sub_basin_values[aggregated_values, sub_basins, time]`. The file `version`
  attribute is bumped; `Results` (Python) reads both layouts and squeezes the sub-basin
  axis when there is one sub-basin, so existing scripts keep working.
- `ModelHydro::GetOutletDischarge()` returns the terminal sub-basin's `outlet`.
  New: `GetSubBasinDischarge(id)`, `GetSubBasinIds()`, `GetSubBasinAreas()` (local and
  drained). Bound to Python as `get_sub_basin_discharge(id)` and friends.
- Water-balance totals (`GetTotalET`, storage changes) become drained-area-weighted
  sums over sub-basins so that `get_total_*` remain catchment totals in mm.

### D8. Evaluation and calibration: gauges are observations with a sub-basin id

- `DischargeObservations` gains an optional `sub_basin` (int id or name). `evaluate`,
  `evaluate_periods` and `SpotpySetup` fetch the simulation with
  `get_sub_basin_discharge(id)` when set, else the outlet as today.
- `SpotpySetup` accepts a list of gauge observations. Each gauge is an objective
  term; the existing multi-objective and weighted-combination machinery
  (`n_objectives`, `_objective_with_extra_observations`) handles the combination. No
  new algorithm code.
- Nested calibration (upstream gauge first, freeze, then downstream) is an example
  script, using `allow_changing` on the parameter set.

### D9. Python API, preprocessing and project file

- `HydroUnits`: optional `sub_basin` column (CSV column mapping or `add_property`),
  plus `set_sub_basins(table)` with columns `id`, `downstream`, optional `name` and
  properties. `save_as` writes the new variables.
- `Catchment.delineate_sub_basins(outlets)` (new, in `preprocessing/`): from the DEM
  flow direction already computed by the connectivity module (pysheds), label each
  pixel with the nearest downstream outlet point, derive the tree, assign hydro units by
  majority pixel (a unit spanning two sub-basins is split with the existing
  `split_discontinuous` mechanism), and compute reach length per sub-basin from the
  main stem to the next outlet. Output: the sub-basin table and a `sub_basin_ids.tif`.
- Project YAML:

```yaml
sub_basins:
  file: sub_basins.csv          # id, downstream, name, reach_length [m], ...
  hydro_units_column: sub_basin # column of the hydro units file (default: sub_basin)
  routing: muskingum            # none | delay | muskingum (default: none)

observations:                   # a single mapping stays valid (terminal outlet)
  - {file: q_stgallen.csv, time: {column: Date, format: "%d/%m/%Y"}, column: Q, sub_basin: 1}
  - {file: q_appenzell.csv, time: {column: Date, format: "%d/%m/%Y"}, column: Q, sub_basin: 2}

parameters:
  reach_k: 0.8                  # alias of reach:k (shared) or spatial: {property: reach_k}
  reach_x: 0.2
```

`validate` cross-checks: every `sub_basin` in observations exists, the hydro-units
column references only declared ids, `routing` other than `none` requires the reach
parameters.

## 4. Work plan (one reviewable PR each)

| PR | Content | Acceptance |
| --- | --- | --- |
| 1 | `SubBasinSettings`, `sub_basin` on hydro units, `RiverNetwork` owning the (single) `SubBasin`, topological order and validation. `HydroUnits` column and `save_as`/`Parse` round trip. No behaviour change. | All C++ and Python tests pass unchanged; a new test builds a 3-sub-basin tree from settings and checks order and drained areas. |
| 2 | `Connector::Transfer`, per-sub-basin `Processor`, run loop over the network, `routing:none`, `reach` brick generation, logger and result file sub-basin dimension, `Results` reader, `get_sub_basin_discharge`. | Sitter St. Gallen split into Appenzell + remainder with `routing:none` reproduces the single-basin run's outlet series to 1e-10 (mass conservation); `sub_basin_values` round-trips through `Results`. |
| 3 | `routing:muskingum`; `routing:delay` allowed on the reach; stability warning for explicit solvers. | Unit test against the analytical Muskingum response to a pulse; daily vs hourly convergence test like `test_time_step.py`. |
| 4 | Gauge-aware `DischargeObservations`, `evaluate`, `evaluate_periods`, `SpotpySetup`; project YAML `sub_basins` and observation lists; CLI validation; docs (`basics.rst` spatial structure, `project-files.rst`, `processes.rst` routing). | Calibration example with two gauges on the Sitter runs; docs build clean. |
| 5 | `Catchment.delineate_sub_basins`, reach length extraction, preprocessing example, `sub_basin_ids.tif`. | Example produces the Sitter tree from the two outlet points and the tree matches the outlines. |

Later, outside this plan: OpenMP over sibling sub-basins (Tier 3), reservoirs on
reaches (Tier 2), diversions as reach splitters.

## 5. Risks and mitigations

- **Output format change.** Old result files lack the sub-basin dimension. Mitigation:
  `version` attribute and a dual-layout reader in `Results`; the one-sub-basin file keeps
  today's shape by squeezing on read.
- **Solver ordering rule.** A reach declared before a store that feeds `outlet` would
  strand water. Mitigation: the base `Model` appends the reach last, and the structure
  validator refuses a user-declared reach that is not last.
- **Area weighting mistakes.** The mm/volume conversion is the one place where errors
  hide. Mitigation: the PR 2 mass-conservation test (split basin equals lumped basin with
  `routing:none`) catches any factor error; add an assertion that the sum of
  `outlet_volume` over all terminal sub-basins equals the sum of local runoff volumes when
  routing is transparent.
- **Per-sub-basin processors and memory.** Each processor holds traversal tables and a
  solver; with hundreds of sub-basins this is still small next to per-unit state. Not a
  concern at the target scale (tens of sub-basins).
- **Spatial parameters keyed on hydro-unit properties.** Sub-basin properties are a
  second namespace. Mitigation: separate setter with an explicit name, no fallbacks.
- **Hydro units crossing sub-basin borders** after delineation. Mitigation: split via
  the existing discontinuous-unit mechanism, documented.

## 6. Open questions

1. Should interior-gauge observations in m³/s be accepted directly, with the drained
   area used for conversion? Today all discharge files are in mm/d. Proposal: yes, via
   the existing `DischargeTransform` layer, using the sub-basin drained area.
2. Local runoff joins the routed inflow at the outlet (D6). Should there be an option to
   route local runoff through the reach too (half-reach convention)? Proposal: not in
   the first version; a `route_local_runoff` option can be added on the reach later.
3. May sub-basins use different structure variants (e.g. a glacier sub-model only where
   glaciers exist)? Per-unit variants already cover glacier presence; sub-basin-level
   bricks are the only thing that would differ. Proposal: not needed now.
4. Naming: `sub_basin` (matches existing code and the result file) versus `subbasin`
   or `catchment`. Proposal: keep `sub_basin`.
5. Should the network be a separate file (`sub_basins.nc`) or live in
   `hydro_units.nc`? Proposal: same file, to keep one spatial-structure artefact and one
   `Parse` entry point.
