
# Project name
project(core)


##### SOURCE FILES

file(GLOB_RECURSE src_app_h ${CMAKE_SOURCE_DIR}/core/src/app/*.h)
file(GLOB_RECURSE src_app_cpp ${CMAKE_SOURCE_DIR}/core/src/app/*.cpp)

list(APPEND src_app ${src_app_h})
list(APPEND src_app ${src_app_cpp})

list(REMOVE_DUPLICATES src_app)

include(${CMAKE_SOURCE_DIR}/build/cmake/hydrobricks_core_source.cmake)


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
target_link_libraries(core CONAN_PKG::wxbase CONAN_PKG::netcdf CONAN_PKG::yaml-cpp)

target_link_libraries(hydrobricks-cli core)
