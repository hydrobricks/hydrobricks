# Project name
project(tests)

# Output path
set(EXECUTABLE_OUTPUT_PATH ${CMAKE_BINARY_DIR}/tests)

# SOURCE FILES

# List source files
file(GLOB_RECURSE src_tests_h src/*.h)
file(GLOB_RECURSE src_tests_cpp src/*.cpp)
list(APPEND src_tests ${src_tests_h})
list(APPEND src_tests ${src_tests_cpp})

# Remove eventual duplicates
list(REMOVE_DUPLICATES src_tests)

# Include source directories
list(
    APPEND
    inc_dirs
    "${CMAKE_SOURCE_DIR}/core/src/base"
    "${CMAKE_SOURCE_DIR}/core/src/actions"
    "${CMAKE_SOURCE_DIR}/core/src/bricks"
    "${CMAKE_SOURCE_DIR}/core/src/containers"
    "${CMAKE_SOURCE_DIR}/core/src/fluxes"
    "${CMAKE_SOURCE_DIR}/core/src/processes"
    "${CMAKE_SOURCE_DIR}/core/src/spatial")
include_directories(${inc_dirs})

# DECLARE EXECUTABLE

if (WIN32)
    add_executable(hydrobricks-tests WIN32 ${src_tests})
elseif (UNIX AND NOT APPLE)
    add_executable(hydrobricks-tests ${src_tests})
elseif (APPLE)
    add_executable(hydrobricks-tests MACOSX_BUNDLE ${src_tests})
else ()
    add_executable(hydrobricks-tests ${src_tests})
endif ()

# DEFINITIONS

if (WIN32)
    set_target_properties(hydrobricks-tests PROPERTIES LINK_FLAGS "/SUBSYSTEM:CONSOLE")
else ()
    set_target_properties(hydrobricks-tests PROPERTIES COMPILE_DEFINITIONS "wxUSE_GUI=0")
endif ()

if (WIN32)
    add_definitions(-D_CRTDBG_MAP_ALLOC)
endif (WIN32)

# LINKING

add_dependencies(hydrobricks-tests core)
target_link_libraries(hydrobricks-tests core)
target_link_libraries(hydrobricks-tests GTest::gtest)

if (UNIX)
    target_link_libraries(hydrobricks-tests pthread)
endif ()
if (USE_VLD)
    target_link_libraries(hydrobricks-tests ${VLD_LIBRARIES})
endif ()

# CTEST

set(CTEST_TEST_TIMEOUT 3600)
set(DART_TESTING_TIMEOUT 3600)
set(CTEST_OUTPUT_ON_FAILURE TRUE)
add_test(HydroBricks-tests hydrobricks-tests)
include(CTest)
enable_testing()
