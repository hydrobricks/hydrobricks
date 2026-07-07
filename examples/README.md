# hydrobricks examples

Most examples declare their whole setup (data, model, forcing, periods) in a
**YAML project file** loaded with `hb.load_project()` — the same files the
`hydrobricks init/validate/run` command-line tools use — and keep Python for
what the file cannot express (calibration loops, analysis, plotting). The
examples that go beyond the project-file schema are pure Python.

The data used is in `tests/files/catchments/` (referenced with relative paths
from the project files), except where noted.

## Suggested path

### 1. First steps (`basics/`)

| Example | Shows |
|---|---|
| `sitter_project.yaml` + `run_from_project_file.py` | A complete project file; load it, run it, evaluate per period. Also runnable without Python: `hydrobricks run sitter_project.yaml`. |
| `gridded_dem_project.yaml` | Template: hydro units delineated from the DEM + gridded (MeteoSwiss) forcing. Point the two grid paths to your local products. |
| `plot_results_socont.py` | Explore the results: scores vs a benchmark, hydrograph, per-unit maps and animations. Uses the dict escape hatch to switch `record_all` on. |
| `custom_structure.yaml` + `run_custom_structure.py` | Define a full model structure (stores, processes, fluxes) as data and run it through a project configuration. |
| `inspect_model_structure.py` | The model structure graph (summary, serialization, diagram). |

### 2. Calibration (`basics/`)

All use `sitter_calibration_project.yaml` (no parameter values; forcing
corrections declared as calibratable `param:` references with their ranges in
`data_parameters`).

| Example | Shows |
|---|---|
| `calibrate_sceua_socont.py` | SCE-UA calibration with SPOTPY. |
| `calibrate_sceua_socont_parallel.py` | The same, parallelized (`parallel='mpc'`, needs `pathos`); the project is loaded inside the picklable factory. |
| `analyse_mc_socont.py` | Monte Carlo sampling, priors, posterior analysis. |
| `analyse_mc_socont_parallel.py` | The same, parallelized. |
| `calibrate_validate_periods.py` | Split-sample workflow with `sitter_periods_project.yaml`: `load_project(..., setup=False)` + `project.setup(period='calibration')`, then a full-span rerun scored per period. |

### 3. Advanced (`advanced/`)

| Example | Shows |
|---|---|
| `calibrate_multiple_catchments.py` | One parameter set calibrated on two catchments at once; the per-catchment configs are built as dicts (`load_project` accepts dicts too). |
| `calibrate_with_snow_cover.py` | Multi-signal calibration: discharge + MODIS snow cover (`sitter_snow_cover_project.yaml`). |
| `calibrate_with_glacier_mass_balance.py` | Multi-signal calibration: discharge + GLAMOS glacier mass balance (pure Python: glacier cover initialized from an ice-thickness raster). |
| `compare_snow_melt_processes_socont.py` | Snowmelt methods compared on a glacierized catchment (pure Python: per-method discretizations). |
| `snow_redistribution.py` | Lateral snow transport with unit connectivity (pure Python). |
| `simulate_glacier_evolution_delta_h.py` / `..._simple_area_scaling.py` | Dynamic glacier geometry through actions (pure Python). |

### 4. Preprocessing (`preprocessing/`)

Catchment delineation, discretization by elevation/aspect/slope, potential
solar radiation, glacier evolution lookup tables, forcing extraction from
gridded products — the API behind the `hydro_units` section of project files.

## Escape hatch

A project file never limits you: `hb.load_project()` returns the live objects
(`project.model`, `project.forcing`, `project.parameters`, ...), so anything
beyond the file is regular Python on top — exactly what the calibration
examples do.
