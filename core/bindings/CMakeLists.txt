
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

include(${CONAN_PYBIND11_ROOT})

# Own libraries
add_library(pybindings STATIC ${src_bind})


##### LINKING

target_link_libraries(pybindings hydrobricks)