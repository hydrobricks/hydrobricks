# SOURCE FILES

# List source files
file(GLOB_RECURSE src_base_h ${CMAKE_SOURCE_DIR}/core/src/base/*.h)
file(GLOB_RECURSE src_base_cpp ${CMAKE_SOURCE_DIR}/core/src/base/*.cpp)
file(GLOB_RECURSE src_behaviours_h ${CMAKE_SOURCE_DIR}/core/src/behaviours/*.h)
file(GLOB_RECURSE src_behaviours_cpp ${CMAKE_SOURCE_DIR}/core/src/behaviours/*.cpp)
file(GLOB_RECURSE src_bricks_h ${CMAKE_SOURCE_DIR}/core/src/bricks/*.h)
file(GLOB_RECURSE src_bricks_cpp ${CMAKE_SOURCE_DIR}/core/src/bricks/*.cpp)
file(GLOB_RECURSE src_containers_h ${CMAKE_SOURCE_DIR}/core/src/containers/*.h)
file(GLOB_RECURSE src_containers_cpp ${CMAKE_SOURCE_DIR}/core/src/containers/*.cpp)
file(GLOB_RECURSE src_fluxes_h ${CMAKE_SOURCE_DIR}/core/src/fluxes/*.h)
file(GLOB_RECURSE src_fluxes_cpp ${CMAKE_SOURCE_DIR}/core/src/fluxes/*.cpp)
file(GLOB_RECURSE src_processes_h ${CMAKE_SOURCE_DIR}/core/src/processes/*.h)
file(GLOB_RECURSE src_processes_cpp ${CMAKE_SOURCE_DIR}/core/src/processes/*.cpp)
file(GLOB_RECURSE src_spatial_h ${CMAKE_SOURCE_DIR}/core/src/spatial/*.h)
file(GLOB_RECURSE src_spatial_cpp ${CMAKE_SOURCE_DIR}/core/src/spatial/*.cpp)
list(
    APPEND
    src_core
    ${src_base_h}
    ${src_behaviours_h}
    ${src_bricks_h}
    ${src_containers_h}
    ${src_fluxes_h}
    ${src_processes_h}
    ${src_spatial_h})
list(
    APPEND
    src_core
    ${src_base_cpp}
    ${src_behaviours_cpp}
    ${src_bricks_cpp}
    ${src_containers_cpp}
    ${src_fluxes_cpp}
    ${src_processes_cpp}
    ${src_spatial_cpp})

# Remove eventual duplicates
list(REMOVE_DUPLICATES src_core)

# Include source directories
list(
    APPEND
    inc_dirs
    "${CMAKE_SOURCE_DIR}/core/src/app"
    "${CMAKE_SOURCE_DIR}/core/src/base"
    "${CMAKE_SOURCE_DIR}/core/src/behaviours"
    "${CMAKE_SOURCE_DIR}/core/src/bricks"
    "${CMAKE_SOURCE_DIR}/core/src/containers"
    "${CMAKE_SOURCE_DIR}/core/src/fluxes"
    "${CMAKE_SOURCE_DIR}/core/src/processes"
    "${CMAKE_SOURCE_DIR}/core/src/spatial")
include_directories(${inc_dirs})

# Make it visible to other CMake files
set(src_core
    ${src_core}
    PARENT_SCOPE)
