# SOURCE FILES

# Define an option for the Python root directory
set(PYBIND11_PYTHON_ROOT CACHE STRING "Path to the root of the desired Python installation")
set(PYBIND11_PYTHON_VERSION CACHE STRING "Python version")

message(STATUS "PYBIND11_PYTHON_ROOT = ${PYBIND11_PYTHON_ROOT}")
message(STATUS "PYBIND11_PYTHON_VERSION = ${PYBIND11_PYTHON_VERSION}")

# Find Pybind11 and Python
if (PYBIND11_PYTHON_ROOT)
    set(Python3_ROOT_DIR ${PYBIND11_PYTHON_ROOT})
    set(Python_ROOT_DIR ${PYBIND11_PYTHON_ROOT})
    list(APPEND CMAKE_PREFIX_PATH "${PYBIND11_PYTHON_ROOT}")
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

# COPY BINARY FILES

if (WIN32)
    # Find all DLL files in the vcpkg installed directory
    file(GLOB VCPKG_DLLS "${CMAKE_BINARY_DIR}/vcpkg_installed/*/bin/*.dll")
    set(OUTPUT_DIR_DLLS "${CMAKE_SOURCE_DIR}")

    foreach (DLL ${VCPKG_DLLS})
        add_custom_command(
            TARGET _hydrobricks
            POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different "${DLL}" "${OUTPUT_DIR_DLLS}"
            COMMENT "Copying ${DLL} to ${OUTPUT_DIR_DLLS}")
    endforeach ()
endif ()
