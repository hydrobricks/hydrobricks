
# Project name
project(tests)

# Output path
set(EXECUTABLE_OUTPUT_PATH ${CMAKE_BINARY_DIR}/tests)


##### SOURCE FILES

# List source files
file(GLOB_RECURSE src_tests_h src/*.h)
file(GLOB_RECURSE src_tests_cpp src/*.cpp)
list(APPEND src_tests ${src_tests_h})
list(APPEND src_tests ${src_tests_cpp})

# Remove eventual duplicates
list(REMOVE_DUPLICATES src_tests)

# Include source directories
list(APPEND inc_dirs
        "${CMAKE_SOURCE_DIR}/src/base"
        "${CMAKE_SOURCE_DIR}/src/components"
        "${CMAKE_SOURCE_DIR}/src/processes"
        "${CMAKE_SOURCE_DIR}/src/spatial"
        "${PROJECT_SOURCE_DIR}/test/src/"
        )
include_directories(${inc_dirs})


##### DECLARE EXECUTABLE

if (WIN32)
    add_executable(hydrobricks-tests WIN32 ${src_tests})
elseif (UNIX AND NOT APPLE)
    add_executable(hydrobricks-tests ${src_tests})
elseif (APPLE)
    add_executable(hydrobricks-tests MACOSX_BUNDLE ${src_tests})
else ()
    add_executable(hydrobricks-tests ${src_tests})
endif ()


##### DEFINITIONS

if(WIN32)
    set_target_properties(hydrobricks-tests PROPERTIES LINK_FLAGS "/SUBSYSTEM:CONSOLE")
    set_target_properties(hydrobricks-tests PROPERTIES COMPILE_DEFINITIONS "UNIT_TESTING; wxUSE_GUI=0; _CONSOLE")
else()
    set_target_properties(hydrobricks-tests PROPERTIES COMPILE_DEFINITIONS "UNIT_TESTING; wxUSE_GUI=0")
endif()


##### LINKING

add_dependencies(hydrobricks-tests core)
target_link_libraries(hydrobricks-tests core)
target_link_libraries(hydrobricks-tests ${CONAN_LIBS})
if (UNIX)
    target_link_libraries(hydrobricks-tests pthread)
endif ()
if (USE_VLD)
    target_link_libraries(hydrobricks-tests ${VLD_LIBRARIES})
endif ()


##### CTEST

set(CTEST_TEST_TIMEOUT 3600)
set(DART_TESTING_TIMEOUT 3600)
set(CTEST_OUTPUT_ON_FAILURE TRUE)
add_test(HydroBricks-tests hydrobricks-tests)
include(CTest)
enable_testing()