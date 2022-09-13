
# Project name
project(core)


##### SOURCE FILES

# List source files
file(GLOB_RECURSE src_app_h app/*.h)
file(GLOB_RECURSE src_app_cpp app/*.cpp)
file(GLOB_RECURSE src_base_h base/*.h)
file(GLOB_RECURSE src_base_cpp base/*.cpp)
file(GLOB_RECURSE src_behaviours_h behaviours/*.h)
file(GLOB_RECURSE src_behaviours_cpp behaviours/*.cpp)
file(GLOB_RECURSE src_bricks_h bricks/*.h)
file(GLOB_RECURSE src_bricks_cpp bricks/*.cpp)
file(GLOB_RECURSE src_fluxes_h fluxes/*.h)
file(GLOB_RECURSE src_fluxes_cpp fluxes/*.cpp)
file(GLOB_RECURSE src_processes_h processes/*.h)
file(GLOB_RECURSE src_processes_cpp processes/*.cpp)
file(GLOB_RECURSE src_spatial_h spatial/*.h)
file(GLOB_RECURSE src_spatial_cpp spatial/*.cpp)
list(APPEND src_core ${src_base_h} ${src_behaviours_h} ${src_bricks_h} ${src_fluxes_h} ${src_processes_h} ${src_spatial_h})
list(APPEND src_core ${src_base_cpp} ${src_behaviours_cpp} ${src_bricks_cpp} ${src_fluxes_cpp} ${src_processes_cpp} ${src_spatial_cpp})
list(APPEND src_app ${src_app_h})
list(APPEND src_app ${src_app_cpp})

# Remove eventual duplicates
list(REMOVE_DUPLICATES src_core)
list(REMOVE_DUPLICATES src_app)

# Include source directories
list(APPEND inc_dirs
        "${CMAKE_SOURCE_DIR}/core/src/app"
        "${CMAKE_SOURCE_DIR}/core/src/base"
        "${CMAKE_SOURCE_DIR}/core/src/behaviours"
        "${CMAKE_SOURCE_DIR}/core/src/bricks"
        "${CMAKE_SOURCE_DIR}/core/src/fluxes"
        "${CMAKE_SOURCE_DIR}/core/src/processes"
        "${CMAKE_SOURCE_DIR}/core/src/spatial"
        )
include_directories(${inc_dirs})


##### LIBRARIES

# Own libraries
add_library(core STATIC ${src_core})


#### DECLARE EXECUTABLE

if (WIN32)
    add_executable(hydrobricks-cli WIN32 ${src_app})
elseif (UNIX AND NOT APPLE)
    add_executable(hydrobricks-cli ${src_app})
elseif (APPLE)
    add_executable(hydrobricks-cli MACOSX_BUNDLE ${src_app})
else (WIN32)
    add_executable(hydrobricks-cli ${src_app})
endif (WIN32)


##### DEFINITIONS

# Specific definitions
set_target_properties(core PROPERTIES COMPILE_DEFINITIONS "wxUSE_GUI=0")

if (MSVC)
    set_target_properties(hydrobricks-cli PROPERTIES LINK_FLAGS "/SUBSYSTEM:CONSOLE")
    #set_target_properties(hydrobricks-cli PROPERTIES COMPILE_DEFINITIONS "wxUSE_GUI=0; _CONSOLE")
    set_target_properties(hydrobricks-cli PROPERTIES COMPILE_DEFINITIONS "wxUSE_GUI=0")
else (MSVC)
    set_target_properties(hydrobricks-cli PROPERTIES COMPILE_DEFINITIONS "wxUSE_GUI=0")
endif (MSVC)

if (WIN32)
    add_definitions(-D_CRTDBG_MAP_ALLOC)
endif (WIN32)


##### LINKING

# Link libraries explicitly to not link Google Tests to the main app.
target_link_libraries(core CONAN_PKG::wxbase CONAN_PKG::netcdf CONAN_PKG::yaml-cpp
        #CONAN_PKG::expat CONAN_PKG::jbig CONAN_PKG::libcurl CONAN_PKG::libdeflate
        #CONAN_PKG::libjpeg CONAN_PKG::libpng CONAN_PKG::libtiff CONAN_PKG::libwebp
        #CONAN_PKG::openssl CONAN_PKG::xz_utils CONAN_PKG::zlib CONAN_PKG::zstd
        )

target_link_libraries(hydrobricks-cli core)