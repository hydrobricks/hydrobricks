
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
file(GLOB_RECURSE src_base_h ${CMAKE_SOURCE_DIR}/src/*.h)
file(GLOB_RECURSE src_base_cpp ${CMAKE_SOURCE_DIR}/src/*.cpp)
list(APPEND src_tests ${src_base_h})
list(APPEND src_tests ${src_base_cpp})

# Remove eventual duplicates
list(REMOVE_DUPLICATES src_tests)

# Include source directories
list(APPEND inc_dirs
        "${CMAKE_SOURCE_DIR}/src/"
        "${PROJECT_SOURCE_DIR}/test/src/"
        )
include_directories(${inc_dirs})


##### LIBRARIES

include(define_common_libraries)

# Google Test
if (MINGW OR MSYS)
    set(gtest_disable_pthreads ON CACHE BOOL "" FORCE)
endif ()
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
set(BUILD_GMOCK OFF CACHE BOOL "" FORCE)
set(INSTALL_GTEST OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)
include_directories(${googletest_SOURCE_DIR}/googletest/include)

##### DECLARE EXECUTABLE

if (WIN32)
    add_executable(flhy-tests WIN32 ${src_tests})
elseif (UNIX AND NOT APPLE)
    add_executable(flhy-tests ${src_tests})
elseif (APPLE)
    add_executable(flhy-tests MACOSX_BUNDLE ${src_tests})
else ()
    add_executable(flhy-tests ${src_tests})
endif ()


##### DEFINITIONS

if(WIN32)
    set_target_properties(flhy-tests PROPERTIES LINK_FLAGS "/SUBSYSTEM:CONSOLE")
    set_target_properties(flhy-tests PROPERTIES COMPILE_DEFINITIONS "UNIT_TESTING; wxUSE_GUI=0; _CONSOLE")
else()
    set_target_properties(flhy-tests PROPERTIES COMPILE_DEFINITIONS "UNIT_TESTING; wxUSE_GUI=0")
endif()


##### LINKING

add_dependencies(flhy-tests base)
add_dependencies(flhy-tests gtest)
target_link_libraries(flhy-tests base)
target_link_libraries(flhy-tests gtest gtest_main)
if (UNIX)
    target_link_libraries(flhy-tests pthread)
endif ()
if (USE_VLD)
    target_link_libraries(flhy-tests ${VLD_LIBRARIES})
endif ()


##### DLLS

if (WIN32)
    file (TO_NATIVE_PATH "${CMAKE_BINARY_DIR}/*.dll" ALL_DLL_PATH_ORIGIN)
    file (TO_NATIVE_PATH "${CMAKE_BINARY_DIR}/tests" ALL_DLL_PATH_DESTINATION)
    add_custom_command(TARGET flhy-tests POST_BUILD
            COMMAND copy
            ${ALL_DLL_PATH_ORIGIN}
            ${ALL_DLL_PATH_DESTINATION})
endif ()


##### CTEST

set(CTEST_TEST_TIMEOUT 3600)
set(DART_TESTING_TIMEOUT 3600)
set(CTEST_OUTPUT_ON_FAILURE TRUE)
add_test(FlHy-tests flhy-tests)
include(CTest)
enable_testing()