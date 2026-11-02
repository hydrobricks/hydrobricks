# hydrobricks

**A modular framework for building, calibrating and comparing conceptual hydrological models.**

[![DOI](https://zenodo.org/badge/DOI/10.5281/zenodo.7139899.svg)](https://doi.org/10.5281/zenodo.7139899)
[![PyPI](https://img.shields.io/pypi/v/hydrobricks)](https://pypi.org/project/hydrobricks/)
[![Python](https://img.shields.io/pypi/pyversions/hydrobricks)](https://pypi.org/project/hydrobricks/)
[![Documentation Status](https://readthedocs.org/projects/hydrobricks/badge/?version=latest)](https://hydrobricks.readthedocs.io/en/latest/?badge=latest)
[![Core Tests](https://github.com/hydrobricks/hydrobricks/actions/workflows/core-tests-linux.yml/badge.svg)](https://github.com/hydrobricks/hydrobricks/actions/workflows/core-tests-linux.yml)
[![Python Tests](https://github.com/hydrobricks/hydrobricks/actions/workflows/python-tests-linux.yml/badge.svg)](https://github.com/hydrobricks/hydrobricks/actions/workflows/python-tests-linux.yml)
[![Python Wheels](https://github.com/hydrobricks/hydrobricks/actions/workflows/python-wheels.yml/badge.svg)](https://github.com/hydrobricks/hydrobricks/actions/workflows/python-wheels.yml)
[![Codecov](https://img.shields.io/codecov/c/github/hydrobricks/hydrobricks)](https://codecov.io/gh/hydrobricks/hydrobricks)
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

Pre-built wheels are available on [PyPI](https://pypi.org/project/hydrobricks/)
for Linux (x86_64), Windows (x86_64) and macOS (Apple Silicon):

```bash
pip install hydrobricks          # core
pip install "hydrobricks[all]"   # with pre-processing, calibration and plotting
```

To build from source, see the [wiki](https://github.com/hydrobricks/hydrobricks/wiki).

## Quick start

Describe the model, the inputs and the periods in a project file, then run it:

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

More complete examples are in the [examples](https://github.com/hydrobricks/hydrobricks/tree/main/examples) folder.

## Resources

*   **Documentation**: https://hydrobricks.readthedocs.io
*   **Examples**: https://github.com/hydrobricks/hydrobricks/tree/main/examples
*   **Building from source**: https://github.com/hydrobricks/hydrobricks/wiki
*   **C++ core API**: https://hydrobricks.github.io/hydrobricks-doc-core/

### Related
*   Downscaling of hydrobricks outputs to sub-daily flow duration curves in alpine
    glacierized catchments: [glacial-discharge-downscaling](https://github.com/ALArgentin/glacial-discharge-downscaling)

## Citation

If you use hydrobricks in your work, please cite it. The citation metadata is in
[CITATION.cff](CITATION.cff), and GitHub's "Cite this repository" button gives it
in APA and BibTeX formats. Each release is archived on Zenodo:
https://doi.org/10.5281/zenodo.7139899

## Contributing

Contributions are very welcome, from bug reports to new processes and models:

1.  Fork the repository and create a branch.
2.  Make your changes, with tests where relevant
    (`pip install -e ".[test]"`, then `pytest`).
3.  Run `pre-commit run --all-files` for formatting.
4.  Open a pull request.

Questions and bug reports go to the
[issue tracker](https://github.com/hydrobricks/hydrobricks/issues).

## License

hydrobricks is released under the [MIT License](LICENSE).
