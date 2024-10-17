# SOURCE FILES

# Define an option for the Python root directory
set(PYBIND11_PYTHON_ROOT CACHE STRING "Path to the root of the desired Python installation")
set(PYBIND11_PYTHON_VERSION CACHE STRING "Python version")

# Find Pybind11 and Python
if (PYBIND11_PYTHON_ROOT_XXXXXXXXX)
    message(STATUS "PYBIND11_PYTHON_ROOT = ${PYBIND11_PYTHON_ROOT}")

    set(Python3_ROOT_DIR ${PYBIND11_PYTHON_ROOT})
    list(APPEND CMAKE_PREFIX_PATH "${PYBIND11_PYTHON_ROOT}")

    # Clear previously cached Python variables
    unset(Python3_EXECUTABLE CACHE)
    unset(Python3_INCLUDE_DIR CACHE)
    unset(Python3_LIBRARY CACHE)
    unset(Python3_LIBRARIES CACHE)
    unset(Python3_VERSION_STRING CACHE)

    # Remove the dot in the version number
    string(REPLACE "." "" PYTHON_VERSION_CONT ${PYBIND11_PYTHON_VERSION})

    # Set Python paths
    set(Python3_ROOT_DIR "${PYBIND11_PYTHON_ROOT}")
    if (WIN32)
        set(Python3_EXECUTABLE "${PYBIND11_PYTHON_ROOT}/python.exe")
        set(Python3_LIBRARY "${PYBIND11_PYTHON_ROOT}/libs/python${PYTHON_VERSION_CONT}.lib")
        set(Python3_INCLUDE_DIR "${PYBIND11_PYTHON_ROOT}/include")
    else ()
        set(Python3_EXECUTABLE "${PYBIND11_PYTHON_ROOT}/bin/python${PYTHON_VERSION_CONT}")
        set(Python3_LIBRARY "${PYBIND11_PYTHON_ROOT}/lib/libpython${PYTHON_VERSION_CONT}.so")
        set(Python3_INCLUDE_DIR "${PYBIND11_PYTHON_ROOT}/include/python${PYTHON_VERSION_CONT}")
    endif ()

    # Prioritize PYBIND11_PYTHON_ROOT in CMAKE_PREFIX_PATH
    list(APPEND CMAKE_PREFIX_PATH "${PYBIND11_PYTHON_ROOT}")

    if (PYBIND11_PYTHON_VERSION)
        message(STATUS "PYBIND11_PYTHON_VERSION = ${PYBIND11_PYTHON_VERSION}")
        find_package(
                Python3 ${PYBIND11_PYTHON_VERSION}
                COMPONENTS Interpreter Development
                REQUIRED)
    else ()
        find_package(
                Python3
                COMPONENTS Development Interpreter
                REQUIRED)
    endif ()
    message(STATUS "Python3_EXECUTABLE = ${Python3_EXECUTABLE}")
    message(STATUS "Python3_INCLUDE_DIR = ${Python3_INCLUDE_DIR}")
    message(STATUS "Python3_LIBRARY = ${Python3_LIBRARY}")
endif ()

set(PYBIND11_NEWPYTHON ON)
set(PYBIND11_FINDPYTHON ON)
FetchContent_MakeAvailable(pybind11)
add_subdirectory("${pybind11_SOURCE_DIR}" "${EXECUTABLE_OUTPUT_PATH}/pybind11")

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
