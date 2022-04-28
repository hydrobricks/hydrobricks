# HydroBricks
Modular Hydrological Modelling Framework

[![Linux-Tests](https://github.com/hydrobricks/hydrobricks/actions/workflows/linux-tests.yml/badge.svg)](https://github.com/hydrobricks/hydrobricks/actions/workflows/linux-tests.yml)
[![codecov](https://codecov.io/gh/hydrobricks/hydrobricks/branch/main/graph/badge.svg?token=G1PBSK8EG2)](https://codecov.io/gh/hydrobricks/hydrobricks)

> :warning: **Nothing to see here. This is a very preliminary code base that is not doing much yet...**

## Dependencies

Get conan (requires Python 3) and setup the default profile:
```
pip install conan
conan profile new default --detect
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
