
# Project name
project(base)


##### SOURCE FILES

# List source files
file(GLOB_RECURSE src_base_h *.h)
file(GLOB_RECURSE src_base_cpp *.cpp)
list(APPEND src_base ${src_base_h})
list(APPEND src_base ${src_base_cpp})

# Remove eventual duplicates
list(REMOVE_DUPLICATES src_base)

# Include source directories
list(APPEND inc_dirs "${PROJECT_SOURCE_DIR}/")
include_directories(${inc_dirs})


##### LIBRARIES

include(define_common_libraries)

# Own libraries
add_library(base STATIC ${src_base})


##### DEFINITIONS

# Specific definitions
set_target_properties(base PROPERTIES COMPILE_DEFINITIONS "wxUSE_GUI=0")
