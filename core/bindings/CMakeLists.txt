# SOURCE FILES

# Find Pybind11
set(PYBIND11_FINDPYTHON ON)
find_package(Python COMPONENTS Interpreter Development REQUIRED)
find_package(pybind11 CONFIG REQUIRED)

# List source files
file(GLOB_RECURSE src_bind_h *.h)
file(GLOB_RECURSE src_bind_cpp *.cpp)
list(APPEND src_bind ${src_bind_h})
list(APPEND src_bind ${src_bind_cpp})

# Remove eventual duplicates
list(REMOVE_DUPLICATES src_bind)

# Core
include(${CMAKE_SOURCE_DIR}/core/build/cmake/hydrobricks_core_source.cmake)

list(APPEND src_bind ${src_core})

# GENERATED MODULE

pybind11_add_module(_hydrobricks ${src_bind})
target_link_libraries(_hydrobricks PRIVATE wx::base netCDF::netcdf yaml-cpp::yaml-cpp pybind11::headers)
