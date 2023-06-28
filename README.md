# hydrobricks

> :warning: **hydrobricks is still in its very early stages and options are limited at this point...**

Modular Hydrological Modelling Framework

[![DOI](https://zenodo.org/badge/301952016.svg)](https://zenodo.org/badge/latestdoi/301952016)
[![Core Tests Linux](https://github.com/hydrobricks/hydrobricks/actions/workflows/core-tests-linux.yml/badge.svg)](https://github.com/hydrobricks/hydrobricks/actions/workflows/core-tests-linux.yml)
[![Python Wheels](https://github.com/hydrobricks/hydrobricks/actions/workflows/python-wheels.yml/badge.svg)](https://github.com/hydrobricks/hydrobricks/actions/workflows/python-wheels.yml)
[![codecov](https://codecov.io/gh/hydrobricks/hydrobricks/branch/main/graph/badge.svg?token=G1PBSK8EG2)](https://codecov.io/gh/hydrobricks/hydrobricks)
[![pre-commit.ci status](https://results.pre-commit.ci/badge/github/hydrobricks/hydrobricks/main.svg)](https://results.pre-commit.ci/latest/github/hydrobricks/hydrobricks/main)
[![Core Doxygen](https://github.com/hydrobricks/hydrobricks/actions/workflows/core-doxygen.yml/badge.svg)](https://github.com/hydrobricks/hydrobricks/actions/workflows/core-doxygen.yml)
[![Codacy Badge](https://app.codacy.com/project/badge/Grade/639e5bb76690488f9aac5feb89722bfa)](https://www.codacy.com/gh/hydrobricks/hydrobricks/dashboard?utm_source=github.com&amp;utm_medium=referral&amp;utm_content=hydrobricks/hydrobricks&amp;utm_campaign=Badge_Grade)

## Install

Wheels are available to install from PyPI (https://pypi.org/project/hydrobricks/)

Install: ```pip install hydrobricks```

## Resources
*   **Documentation**: https://hydrobricks.readthedocs.io/en/latest/
*   How to build: https://github.com/hydrobricks/hydrobricks/wiki
*   Code documentation of the core: https://hydrobricks.github.io/hydrobricks-doc-core/

## Build it
To build hydrobricks, you need:
* A C++ compiler (GCC, Visual Studio)
* CMake
* Python 3

Get Conan package manager (v1 and not v2):
```
pip install conan==1.*
```

Configure Conan:
```
conan profile new --detect
conan remote add gitlab https://gitlab.com/api/v4/packages/conan
conan profile update settings.compiler.libcxx=libstdc++11 default  # Only for Linux
conan config set general.revisions_enabled=1
```

Create a bin directory and cd to it. For example, for Linux:
```
mkdir bin
cd bin
```

Install the dependencies (set the desired build type: Debug or Release, for example):
```
conan install .. --build=missing -s build_type=Debug -pr:b=default
```

Configure and build (set the desired build type: Debug or Release, for example):
```
cmake .. -DCMAKE_BUILD_TYPE=Debug
cmake --build . --config Debug
```

Install the Python module in “editable” mode
```
pip install -e .
```
