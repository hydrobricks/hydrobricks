
# Project name
project(core)


##### SOURCE FILES

# List source files
file(GLOB_RECURSE src_base_h base/*.h)
file(GLOB_RECURSE src_base_cpp base/*.cpp)
file(GLOB_RECURSE src_components_h components/*.h)
file(GLOB_RECURSE src_components_cpp components/*.cpp)
file(GLOB_RECURSE src_processes_h processes/*.h)
file(GLOB_RECURSE src_processes_cpp processes/*.cpp)
file(GLOB_RECURSE src_spatial_h spatial/*.h)
file(GLOB_RECURSE src_spatial_cpp spatial/*.cpp)
list(APPEND src_core ${src_base_h} ${src_components_h} ${src_processes_h} ${src_spatial_h})
list(APPEND src_core ${src_base_cpp} ${src_components_cpp} ${src_processes_cpp} ${src_spatial_cpp})

# Remove eventual duplicates
list(REMOVE_DUPLICATES src_core)

# Include source directories
list(APPEND inc_dirs
        "${CMAKE_SOURCE_DIR}/src/base"
        "${CMAKE_SOURCE_DIR}/src/components"
        "${CMAKE_SOURCE_DIR}/src/processes"
        "${CMAKE_SOURCE_DIR}/src/spatial"
        )
include_directories(${inc_dirs})


##### LIBRARIES

# Own libraries
add_library(core STATIC ${src_core})


##### DEFINITIONS

# Specific definitions
set_target_properties(core PROPERTIES COMPILE_DEFINITIONS "wxUSE_GUI=0")
