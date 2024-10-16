# SOURCE FILES

# Define an option for the Python root directory
option(PYTHON_ROOT "Path to the root of the desired Python installation" "")
message(STATUS "PYTHON_ROOT = ${PYTHON_ROOT}")
option(PYTHON_VERSION "Python version" "")
message(STATUS "PYTHON_VERSION = ${PYTHON_VERSION}")

# Find Pybind11 and Python
if (PYTHON_ROOT)
    set(Python3_ROOT_DIR ${PYTHON_ROOT})
    list(APPEND CMAKE_PREFIX_PATH "${PYTHON_ROOT}")

    # Clear previously cached Python variables
    unset(Python3_EXECUTABLE CACHE)
    unset(Python3_INCLUDE_DIR CACHE)
    unset(Python3_LIBRARY CACHE)
    unset(Python3_LIBRARIES CACHE)
    unset(Python3_VERSION_STRING CACHE)

    # Remove the dot in the version number
    string(REPLACE "." "" PYTHON_VERSION_CONT ${PYTHON_VERSION})

    # Set Python paths
    set(Python3_ROOT_DIR "${PYTHON_ROOT}")
    set(Python3_EXECUTABLE "${PYTHON_ROOT}/python.exe")
    set(Python3_INCLUDE_DIR "${PYTHON_ROOT}/include")
    set(Python3_LIBRARY "${PYTHON_ROOT}/libs/python${PYTHON_VERSION_CONT}.lib")

    # Prioritize PYTHON_ROOT in CMAKE_PREFIX_PATH
    list(APPEND CMAKE_PREFIX_PATH "${PYTHON_ROOT}")
endif ()

if (PYTHON_VERSION)
    find_package(
        Python3 ${PYTHON_VERSION}
        COMPONENTS Interpreter Development
        REQUIRED)
else ()
    find_package(
        Python3
        COMPONENTS Development Interpreter
        REQUIRED)
endif ()
find_package(pybind11 CONFIG REQUIRED)
message(STATUS "Python3_EXECUTABLE = ${Python3_EXECUTABLE}")
message(STATUS "Python3_INCLUDE_DIR = ${Python3_INCLUDE_DIR}")
message(STATUS "Python3_LIBRARY = ${Python3_LIBRARY}")

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

pybind11_add_module(_hydrobricks MODULE ${src_bind})
target_link_libraries(_hydrobricks PUBLIC wx::base netCDF::netcdf yaml-cpp::yaml-cpp pybind11::headers)
