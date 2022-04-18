# HydroBricks
Modular Hydrological Modelling Framework

![Linux](https://github.com/hydrobricks/hydrobricks/workflows/CMake/badge.svg)
[![codecov](https://codecov.io/gh/hydrobricks/hydrobricks/branch/main/graph/badge.svg?token=G1PBSK8EG2)](https://codecov.io/gh/hydrobricks/hydrobricks)

> :warning: **Nothing to see here. This is a very preliminary code base that is not doing much yet...**

## Dependencies
Get or build release libraries:
```
mkdir cmake-build-release
cd cmake-build-release
conan install .. -o build_tests=True --build=missing
```

Get or build debug libraries (the option ``-s compiler.runtime=MDd`` is for Windows only):
```
mkdir cmake-build-debug
cd cmake-build-debug
conan install .. -o build_tests=True -s build_type=Debug -s compiler.runtime=MDd --build=missing  
```
