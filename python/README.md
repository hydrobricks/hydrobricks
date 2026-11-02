# hydrobricks

**A modular framework for building, calibrating and comparing conceptual hydrological models.**

[![DOI](https://zenodo.org/badge/DOI/10.5281/zenodo.7139899.svg)](https://doi.org/10.5281/zenodo.7139899)
[![Documentation Status](https://readthedocs.org/projects/hydrobricks/badge/?version=latest)](https://hydrobricks.readthedocs.io/en/latest/?badge=latest)
[![Python Tests](https://github.com/hydrobricks/hydrobricks/actions/workflows/python-tests-linux.yml/badge.svg)](https://github.com/hydrobricks/hydrobricks/actions/workflows/python-tests-linux.yml)
[![Python Wheels](https://github.com/hydrobricks/hydrobricks/actions/workflows/python-wheels.yml/badge.svg)](https://github.com/hydrobricks/hydrobricks/actions/workflows/python-wheels.yml)
[![pre-commit.ci status](https://results.pre-commit.ci/badge/github/hydrobricks/hydrobricks/main.svg)](https://results.pre-commit.ci/latest/github/hydrobricks/hydrobricks/main)
[![Codacy Badge](https://app.codacy.com/project/badge/Grade/639e5bb76690488f9aac5feb89722bfa)](https://www.codacy.com/gh/hydrobricks/hydrobricks/dashboard?utm_source=github.com&amp;utm_medium=referral&amp;utm_content=hydrobricks/hydrobricks&amp;utm_campaign=Badge_Grade)

hydrobricks assembles hydrological models from reusable components: storages,
land covers, snowpacks and glaciers (the *bricks*), the processes acting on them,
and the fluxes connecting them. The established models it ships with (Socont,
GR4J, GR6J, HBV, PREVAH) are built from the same components, so they share one
numerical core and one workflow. Changing a single process or comparing whole
model structures then becomes a matter of configuration rather than a new code
base. The computation engine is written in C++ and driven from Python.

## Features

*   **Ready-made models**: Socont, GR4J, GR6J, HBV-96 and
    PREVAH, plus custom structures declared in YAML or Python.
*   **Semi-distributed**: catchments divided into hydrological units
    (e.g. elevation bands), with land cover fractions per unit.
*   **Snow and glaciers**: several snowmelt and ice-melt formulations, snow
    redistribution, and glacier evolution over time (Δh parametrization, area
    scaling); land cover changes can also be prescribed.
*   **Pre-processing**: catchment delineation and discretization from a DEM,
    potential solar radiation, and spatialization of station or
    gridded meteorological data.
*   **Calibration and evaluation**: calibration with SPOTPY, and objective
    functions on discharge, snow cover and glacier mass balance.
*   **Project files**: a whole setup in a single YAML file, run from Python or
    from the `hydrobricks` command line; study files run the same comparison over
    many catchments, models and objectives.
*   **Numerical choice**: several ODE solvers (explicit and implicit) for the
    continuous model formulations.

## Installation

Wheels are available for Linux (x86_64), Windows (x86_64) and macOS (Apple
Silicon, macOS 13.3 or later). On other platforms, pip builds from the source
distribution.

```bash
pip install hydrobricks          # core
pip install "hydrobricks[all]"   # with pre-processing, calibration and plotting
```

## Quick start

```bash
hydrobricks init project.yaml       # interactive creation
hydrobricks validate project.yaml   # check the configuration
hydrobricks run project.yaml        # run and report the scores
```

or from Python:

```python
import hydrobricks as hb

project = hb.load_project("project.yaml")
discharge = project.run()
best = project.calibrate()
```

## Resources

*   **Documentation**: https://hydrobricks.readthedocs.io
*   **Source code**: https://github.com/hydrobricks/hydrobricks
*   **Examples**: https://github.com/hydrobricks/hydrobricks/tree/main/examples
*   **Issue tracker**: https://github.com/hydrobricks/hydrobricks/issues

## Citation

If you use hydrobricks in your work, please cite it:
https://doi.org/10.5281/zenodo.7139899 (citation metadata in
[CITATION.cff](https://github.com/hydrobricks/hydrobricks/blob/main/CITATION.cff)).

## License

MIT
