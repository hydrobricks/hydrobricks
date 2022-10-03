# hydrobricks
Modular Hydrological Modelling Framework

[![Core Tests Linux](https://github.com/hydrobricks/hydrobricks/actions/workflows/core-tests-linux.yml/badge.svg)](https://github.com/hydrobricks/hydrobricks/actions/workflows/core-tests-linux.yml)
[![Python Wheels](https://github.com/hydrobricks/hydrobricks/actions/workflows/python-wheels.yml/badge.svg)](https://github.com/hydrobricks/hydrobricks/actions/workflows/python-wheels.yml)
[![codecov](https://codecov.io/gh/hydrobricks/hydrobricks/branch/main/graph/badge.svg?token=G1PBSK8EG2)](https://codecov.io/gh/hydrobricks/hydrobricks)
[![pre-commit.ci status](https://results.pre-commit.ci/badge/github/hydrobricks/hydrobricks/main.svg)](https://results.pre-commit.ci/latest/github/hydrobricks/hydrobricks/main)
[![Core Doxygen](https://github.com/hydrobricks/hydrobricks/actions/workflows/core-doxygen.yml/badge.svg)](https://github.com/hydrobricks/hydrobricks/actions/workflows/core-doxygen.yml)

> :warning: **Nothing to see here. This is a very preliminary code base that is not doing much yet...**

## Resources

* Code documentation of the core: https://hydrobricks.github.io/hydrobricks-doc-core/

## Dependencies

Get conan (requires Python 3) and setup the default profile:
```
pip install conan
conan profile new default --detect

# On Linux:
conan profile update settings.compiler.libcxx=libstdc++11 default
```

Add GitLab as a remote:
```
conan remote add gitlab https://gitlab.com/api/v4/packages/conan
```

Get or build release libraries:
```
mkdir cmake-build-release
cd cmake-build-release
conan install .. --build=missing
```

Get or build debug libraries (the option ``-s compiler.runtime=MDd`` is for Windows only):
```
mkdir cmake-build-debug
cd cmake-build-debug
conan install .. -s build_type=Debug -s compiler.runtime=MDd --build=missing
```

Build Python wheels locally:
```
cibuildwheel --platform windows
```

Test Python wheels locally:
```
import hydrobricks as hb
help(hb.Parameter)
```
