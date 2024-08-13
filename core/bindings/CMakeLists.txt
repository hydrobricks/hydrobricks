# SOURCE FILES

# List source files
file(GLOB_RECURSE src_bind_h *.h)
file(GLOB_RECURSE src_bind_cpp *.cpp)
list(APPEND src_bind ${src_bind_h})
list(APPEND src_bind ${src_bind_cpp})

# Remove eventual duplicates
list(REMOVE_DUPLICATES src_bind)

# Core
include(${CMAKE_SOURCE_DIR}/build/cmake/hydrobricks_core_source.cmake)

list(APPEND src_bind ${src_core})

# LIBRARIES

FetchContent_MakeAvailable(pybind11)
add_subdirectory("${pybind11_SOURCE_DIR}" "${EXECUTABLE_OUTPUT_PATH}/pybind11")

# GENERATED MODULE

pybind11_add_module(_hydrobricks ${src_bind})
target_link_libraries(_hydrobricks PRIVATE wx::base netcdf yaml-cpp)
