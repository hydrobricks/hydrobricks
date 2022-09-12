
# Project name
project(pybindings)


##### SOURCE FILES

# List source files
file(GLOB_RECURSE src_bind_h *.h)
file(GLOB_RECURSE src_bind_cpp *.cpp)
list(APPEND src_bind ${src_bind_h})
list(APPEND src_bind ${src_bind_cpp})

# Remove eventual duplicates
list(REMOVE_DUPLICATES src_bind)

# Include source directories
list(APPEND inc_dirs
        "${CMAKE_SOURCE_DIR}/core/bindings"
        )
include_directories(${inc_dirs})


##### LIBRARIES

add_subdirectory("${CMAKE_SOURCE_DIR}/core/extern/pybind11" "${EXECUTABLE_OUTPUT_PATH}/pybind11")


##### GENERATED MODULE

pybind11_add_module(hydrobricks ${src_bind})
