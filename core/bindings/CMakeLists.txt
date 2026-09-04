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
target_compile_features(_hydrobricks PUBLIC cxx_std_23)
target_link_libraries(_hydrobricks PUBLIC netCDF::netcdf yaml-cpp::yaml-cpp pybind11::headers Eigen3::Eigen)

# Install the module into the hydrobricks package directory
install(TARGETS _hydrobricks LIBRARY DESTINATION hydrobricks)

# COPY BINARY FILES

if (WIN32)
    # Find all DLL files in the vcpkg installed directory. vcpkg installs the manifest next to the directory it runs
    # from, which is the build directory for a plain CMake build but the source directory when pip drives the build (its
    # build directory is a temporary one): both are searched, or the module would be built without the runtime DLLs it
    # needs and fail to import with "DLL load failed".
    file(GLOB VCPKG_DLLS "${CMAKE_BINARY_DIR}/vcpkg_installed/*/bin/*.dll"
         "${CMAKE_SOURCE_DIR}/vcpkg_installed/*/bin/*.dll")

    if (NOT VCPKG_DLLS)
        message(
            WARNING "No vcpkg DLL found next to '${CMAKE_BINARY_DIR}' nor '${CMAKE_SOURCE_DIR}'. "
                    "The _hydrobricks module will not import unless the netCDF/HDF5 runtime "
                    "DLLs are on the DLL search path. Run 'vcpkg install' from one of these "
                    "directories, or copy the DLLs next to the built module.")
    endif ()

    foreach (DLL ${VCPKG_DLLS})
        add_custom_command(
            TARGET _hydrobricks
            POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different "${DLL}" $<TARGET_FILE_DIR:_hydrobricks>
            COMMENT "Copying ${DLL} next to the _hydrobricks module")
    endforeach ()
endif ()
