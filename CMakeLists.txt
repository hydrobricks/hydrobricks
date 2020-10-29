cmake_minimum_required(VERSION 3.14) # To support FetchContent / FetchContent_MakeAvailable

if (POLICY CMP0043)
    cmake_policy(SET CMP0043 NEW)
endif ()
if (POLICY CMP0074)
    cmake_policy(SET CMP0074 NEW)
endif ()
if (POLICY CMP0077)
    cmake_policy(SET CMP0077 NEW)
endif ()

# Project name
if (UNIX)
    project(flhy)
else (UNIX)
    project(FlHy)
endif (UNIX)


##### DEPENDENCIES

include(FetchContent)

FetchContent_Declare(
        googletest
        GIT_REPOSITORY https://github.com/google/googletest.git
        GIT_TAG release-1.10.0)


##### OPTIONS

# Version number
set(VERSION_MAJOR "0")
set(VERSION_MINOR "0")
set(VERSION_PATCH "1")
add_definitions(-DFLHY_MAJOR_VERSION=${VERSION_MAJOR})
add_definitions(-DFLHY_MINOR_VERSION=${VERSION_MINOR})
add_definitions(-DFLHY_PATCH_VERSION=${VERSION_PATCH})

set(CMAKE_MODULE_PATH ${PROJECT_SOURCE_DIR}/build/cmake ${CMAKE_MODULE_PATH})

# Output path
if (WIN32)
    set(EXECUTABLE_OUTPUT_PATH ${CMAKE_BINARY_DIR})
else (WIN32)
    set(EXECUTABLE_OUTPUT_PATH ${CMAKE_BINARY_DIR}/${CMAKE_BUILD_TYPE})
endif (WIN32)

# Targets
option(BUILD_TESTS "Do you want to build the tests (recommended) ?" ON)
mark_as_advanced(CLEAR BUILD_TESTS)

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

# Disable testing tree
set(BUILD_TESTING OFF)
mark_as_advanced(BUILD_TESTING)

# Installer
if (WIN32)
    set(INSTALL_DIR_BIN .)
else (WIN32)
    set(INSTALL_DIR_BIN bin)
endif (WIN32)


##### FIND ANALYSIS TOOLS

# Visual Leak Detector
if (USE_VLD)
    find_package(VLD)
    include_directories(${VLD_INCLUDE_DIRS})
else (USE_VLD)
    # unset for wxhgversion
    unset(VLD_INCLUDE_DIR CACHE)
    unset(VLD_LIBRARY CACHE)
    unset(VLD_LIBRARY_DEBUG CACHE)
    unset(VLD_LIBRARY_DIR CACHE)
    unset(VLD_ROOT_DIR CACHE)
endif (USE_VLD)

# Code coverage
if (USE_CODECOV)
    include(CodeCoverage)
    append_coverage_compiler_flags()
    set(COVERAGE_EXCLUDES '/usr/*'
        '${PROJECT_SOURCE_DIR}/tests/src/*'
        '${CMAKE_BINARY_DIR}/_deps/*')
    setup_target_for_coverage_lcov(NAME flhy-coverage EXECUTABLE flhy-tests DEPENDENCIES flhy-tests)
endif (USE_CODECOV)


##### GET SUBDIRECTORIES CONTENT

include(define_compilation_flags)

# Add components
add_subdirectory(src)
if (BUILD_TESTS)
    add_subdirectory(tests)
endif ()


##### DISPLAY SOME INFORMATION

message(STATUS "CMAKE_CXX_FLAGS = ${CMAKE_CXX_FLAGS}")
message(STATUS "CMAKE_CXX_FLAGS_RELEASE = ${CMAKE_CXX_FLAGS_RELEASE}")
message(STATUS "CMAKE_CXX_FLAGS_DEBUG = ${CMAKE_CXX_FLAGS_DEBUG}")
message(STATUS "CMAKE_CXX_FLAGS_RELWITHDEBINFO = ${CMAKE_CXX_FLAGS_RELWITHDEBINFO}")
