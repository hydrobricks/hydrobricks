cmake_minimum_required(VERSION 3.15)

include(FetchContent)

if (POLICY CMP0043)
    cmake_policy(SET CMP0043 NEW)
endif ()
if (POLICY CMP0074)
    cmake_policy(SET CMP0074 NEW)
endif ()
if (POLICY CMP0077)
    cmake_policy(SET CMP0077 NEW)
endif ()

# vcpkg
if (NOT DEFINED ENV{VCPKG_ROOT})
    message(FATAL_ERROR "Please define the environment variable VCPKG_ROOT")
endif ()
set(CMAKE_TOOLCHAIN_FILE
    "$ENV{VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake"
    CACHE STRING "Vcpkg toolchain file")
message(STATUS "VCPKG_TARGET_TRIPLET = ${VCPKG_TARGET_TRIPLET}")
set(VCPKG_LIBRARY_LINKAGE "static")

# Project name
project(hydrobricks)

# OPTIONS

# Version number
set(VERSION_MAJOR "0")
set(VERSION_MINOR "8")
set(VERSION_PATCH "5")
add_definitions(-DHYDROBRICKS_MAJOR_VERSION=${VERSION_MAJOR})
add_definitions(-DHYDROBRICKS_MINOR_VERSION=${VERSION_MINOR})
add_definitions(-DHYDROBRICKS_PATCH_VERSION=${VERSION_PATCH})

set(CMAKE_MODULE_PATH ${PROJECT_SOURCE_DIR}/core/build/cmake ${CMAKE_MODULE_PATH})

# Output path
if (WIN32)
    set(EXECUTABLE_OUTPUT_PATH ${CMAKE_BINARY_DIR})
else (WIN32)
    set(EXECUTABLE_OUTPUT_PATH ${CMAKE_BINARY_DIR}/${CMAKE_BUILD_TYPE})
endif (WIN32)

# Targets
option(BUILD_TESTS "Do you want to build the tests (recommended) ?" ON)
option(BUILD_CLI "Do you want to build the command-line version ?" ON)
option(BUILD_PYBINDINGS "Do you want to build the Python bindings ?" ON)

# Disable testing tree
set(BUILD_TESTING OFF)
mark_as_advanced(BUILD_TESTING)

# Pre-set wxWidgets options
set(wxUSE_GUI OFF)
set(wxBUILD_INSTALL OFF)
set(wxBUILD_SAMPLES OFF)
set(wxBUILD_TESTS OFF)
set(wxBUILD_SHARED OFF)

# Get and build wxbase from wxWidgets
FetchContent_Declare(
    wxWidgets
    GIT_REPOSITORY https://github.com/wxWidgets/wxWidgets.git
    GIT_TAG v3.2.7)
FetchContent_MakeAvailable(wxWidgets)

include_directories(${wxWidgets_SOURCE_DIR}/include)
link_directories(${wxWidgets_BINARY_DIR}/lib)
message(STATUS "wxWidgets include dir = ${wxWidgets_SOURCE_DIR}/include")
message(STATUS "wxWidgets lib dir = ${wxWidgets_BINARY_DIR}/lib")

# Get and build yaml-cpp (master branch)
FetchContent_Declare(
    yaml-cpp
    GIT_REPOSITORY https://github.com/jbeder/yaml-cpp.git
    GIT_TAG master)
set(YAML_CPP_BUILD_TESTS OFF)
FetchContent_MakeAvailable(yaml-cpp)

include_directories(${yaml-cpp_SOURCE_DIR}/include)
link_directories(${yaml-cpp_BINARY_DIR}/lib)
message(STATUS "yaml-cpp include dir = ${yaml-cpp_SOURCE_DIR}/include")
message(STATUS "yaml-cpp lib dir = ${yaml-cpp_BINARY_DIR}/lib")

# Pybind 11 - not retrieved through vcpkg to avoid getting Python automatically (without control of the version)
FetchContent_Declare(
    pybind11
    GIT_REPOSITORY https://github.com/pybind/pybind11.git
    GIT_TAG v3.0.0)

# Find packages
find_package(netCDF CONFIG REQUIRED)
find_package(Eigen3 CONFIG REQUIRED)
find_package(GTest CONFIG REQUIRED)
find_package(ZLIB REQUIRED)

# Enable Visual Leak Detector
if (WIN32)
    option(USE_VLD "Should we use Visual Leak Detector (https://vld.codeplex.com) ?" OFF)
else (WIN32)
    set(USE_VLD OFF)
endif (WIN32)

# Enable code coverage
if (CMAKE_COMPILER_IS_GNUCXX)
    option(USE_CODECOV "Should we do code coverage with lcov ?" OFF)
else (CMAKE_COMPILER_IS_GNUCXX)
    set(USE_CODECOV OFF)
endif ()

# Installer
if (WIN32)
    set(INSTALL_DIR_BIN .)
else (WIN32)
    set(INSTALL_DIR_BIN bin)
endif (WIN32)

# FIND ANALYSIS TOOLS

# Visual Leak Detector
if (USE_VLD)
    find_package(VLD)
    include_directories(${VLD_INCLUDE_DIRS})
endif (USE_VLD)

# Code coverage
if (USE_CODECOV)
    include(code_coverage)
    append_coverage_compiler_flags()
    setup_target_for_coverage_lcov(
        NAME
        hydrobricks-coverage
        EXECUTABLE
        hydrobricks-tests
        EXCLUDE
        "/usr/*"
        "core/tests/*"
        "bin/*"
        DEPENDENCIES
        hydrobricks-tests)
endif (USE_CODECOV)

# GET SUBDIRECTORIES CONTENT

include(define_compilation_flags)

# Add components
if (BUILD_CLI)
    add_subdirectory(core/src)
endif ()
if (BUILD_TESTS)
    add_subdirectory(core/tests)
endif ()
if (BUILD_PYBINDINGS)
    add_subdirectory(core/bindings)
endif ()

# DISPLAY SOME INFORMATION

message(STATUS "CMAKE_CXX_FLAGS = ${CMAKE_CXX_FLAGS}")
message(STATUS "CMAKE_CXX_FLAGS_RELEASE = ${CMAKE_CXX_FLAGS_RELEASE}")
message(STATUS "CMAKE_CXX_FLAGS_DEBUG = ${CMAKE_CXX_FLAGS_DEBUG}")
message(STATUS "CMAKE_CXX_FLAGS_RELWITHDEBINFO = ${CMAKE_CXX_FLAGS_RELWITHDEBINFO}")
