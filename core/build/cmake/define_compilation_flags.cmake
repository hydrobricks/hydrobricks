# Compilation flags
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")
set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -DNDEBUG")
set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -D_DEBUG -D__WXDEBUG__")
set(CMAKE_CXX_FLAGS_RELWITHDEBINFO "${CMAKE_CXX_FLAGS_RELWITHDEBINFO} -DNDEBUG ")

if (MINGW
    OR MSYS
    OR UNIX
    AND NOT APPLE)
    # Add comprehensive warning flags
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -Wpedantic -Wconversion")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fno-strict-aliasing -Wno-sign-compare -Wno-attributes")
    if (NOT ${CMAKE_SYSTEM_PROCESSOR} MATCHES "arm")
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -msse2")
    endif ()
    set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -O0")
    set(CMAKE_CXX_FLAGS_RELWITHDEBINFO "${CMAKE_CXX_FLAGS_RELWITHDEBINFO} -fno-omit-frame-pointer ")

    # Enable sanitizers for debug builds
    if (USE_SANITIZERS AND CMAKE_BUILD_TYPE MATCHES Debug)
        message(STATUS "Enabling AddressSanitizer and UndefinedBehaviorSanitizer for debug build")
        set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -fsanitize=address,undefined -fno-omit-frame-pointer")
        set(CMAKE_EXE_LINKER_FLAGS_DEBUG "${CMAKE_EXE_LINKER_FLAGS_DEBUG} -fsanitize=address,undefined")
        set(CMAKE_SHARED_LINKER_FLAGS_DEBUG "${CMAKE_SHARED_LINKER_FLAGS_DEBUG} -fsanitize=address,undefined")
    endif ()
elseif (WIN32)
    if (MSVC)
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /MP /EHsc")
        # Force to always compile with W4
        if (CMAKE_CXX_FLAGS MATCHES "/W[0-4]")
            string(REGEX REPLACE "/W[0-4]" "/W4" CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")
        else ()
            set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /W4")
        endif ()
        # Add strict standards compliance
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /permissive-")

        # Enable AddressSanitizer for debug builds (requires VS 2019 16.9+)
        if (USE_SANITIZERS AND CMAKE_BUILD_TYPE MATCHES Debug)
            message(STATUS "Enabling AddressSanitizer for MSVC debug build")
            set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} /fsanitize=address")
            # Disable incremental linking with ASan
            set(CMAKE_EXE_LINKER_FLAGS_DEBUG "${CMAKE_EXE_LINKER_FLAGS_DEBUG} /INCREMENTAL:NO")
            set(CMAKE_SHARED_LINKER_FLAGS_DEBUG "${CMAKE_SHARED_LINKER_FLAGS_DEBUG} /INCREMENTAL:NO")
        endif ()
    endif ()
endif ()

# Link-time (interprocedural) optimization for the optimized configurations. The simulation hot loops are spread over
# many translation units and reach the processes through virtual calls, so the cross-module inlining and
# devirtualization LTO enables are worth the longer link time.
option(USE_LTO "Do you want link-time optimization in the optimized builds ?" ON)
if (USE_LTO)
    include(CheckIPOSupported)
    check_ipo_supported(RESULT LTO_SUPPORTED OUTPUT LTO_NOT_SUPPORTED_REASON)
    if (LTO_SUPPORTED)
        set(CMAKE_INTERPROCEDURAL_OPTIMIZATION_RELEASE ON)
        set(CMAKE_INTERPROCEDURAL_OPTIMIZATION_RELWITHDEBINFO ON)
        message(STATUS "Link-time optimization enabled for the optimized builds.")
    else ()
        message(STATUS "Link-time optimization unavailable: ${LTO_NOT_SUPPORTED_REASON}")
    endif ()
endif ()

# Profile-guided optimization. Built in two passes: configure with USE_PGO=generate, build, run a representative
# simulation (ci/pgo_training.py) to collect the profile, then configure with USE_PGO=use and build again. The second
# pass only relinks, because the instrumentation and the final code generation both happen at link time with the
# link-time optimization the option requires. The profile teaches the compiler which branches the simulation actually
# takes -- which processes a structure holds, whether a store is empty, whether a constraint binds -- none of which is
# visible from the sources alone.
set(USE_PGO
    "off"
    CACHE STRING "Profile-guided optimization: off, generate (instrument), use (apply the collected profile)")
set_property(CACHE USE_PGO PROPERTY STRINGS off generate use)
set(PGO_DATA_DIR
    "${CMAKE_BINARY_DIR}/pgo"
    CACHE PATH "Directory holding the profile-guided optimization data")

if (NOT USE_PGO STREQUAL "off")
    if (NOT USE_LTO OR NOT LTO_SUPPORTED)
        message(FATAL_ERROR "USE_PGO requires USE_LTO: the profile is applied during the link-time code generation.")
    endif ()
    file(MAKE_DIRECTORY "${PGO_DATA_DIR}")
    if (USE_PGO STREQUAL "use"
        AND MSVC
        AND NOT EXISTS "${PGO_DATA_DIR}/hydrobricks.pgd")
        message(
            FATAL_ERROR
                "USE_PGO=use but no profile was found in ${PGO_DATA_DIR}. Configure with USE_PGO=generate, build, run "
                "ci/pgo_training.py against the instrumented module, merge the collected .pgc files into the .pgd with "
                "'pgomgr /merge', then configure again with USE_PGO=use.")
    endif ()

    # The Python extension is a MODULE library, which takes its flags from CMAKE_MODULE_LINKER_FLAGS rather than from
    # the shared-library one: set all three, or the option would silently do nothing for the very target that matters
    # most here.
    if (MSVC)
        if (USE_PGO STREQUAL "generate")
            set(PGO_LINK_FLAG " /GENPROFILE:PGD=\"${PGO_DATA_DIR}/hydrobricks.pgd\"")
        else ()
            set(PGO_LINK_FLAG " /USEPROFILE:PGD=\"${PGO_DATA_DIR}/hydrobricks.pgd\"")
        endif ()
        string(APPEND CMAKE_EXE_LINKER_FLAGS "${PGO_LINK_FLAG}")
        string(APPEND CMAKE_SHARED_LINKER_FLAGS "${PGO_LINK_FLAG}")
        string(APPEND CMAKE_MODULE_LINKER_FLAGS "${PGO_LINK_FLAG}")
    elseif (CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
        if (USE_PGO STREQUAL "generate")
            string(APPEND CMAKE_CXX_FLAGS " -fprofile-generate -fprofile-dir=${PGO_DATA_DIR}")
            string(APPEND CMAKE_EXE_LINKER_FLAGS " -fprofile-generate")
            string(APPEND CMAKE_SHARED_LINKER_FLAGS " -fprofile-generate")
            string(APPEND CMAKE_MODULE_LINKER_FLAGS " -fprofile-generate")
        else ()
            # The profile is collected from one representative run, so it legitimately leaves rarely taken code paths
            # unmeasured: do not let that turn into a wall of warnings.
            string(APPEND CMAKE_CXX_FLAGS
                   " -fprofile-use -fprofile-correction -fprofile-dir=${PGO_DATA_DIR} -Wno-missing-profile")
        endif ()
    else ()
        message(FATAL_ERROR "USE_PGO is only wired up for MSVC and GCC, not for ${CMAKE_CXX_COMPILER_ID}.")
    endif ()

    message(STATUS "Profile-guided optimization: ${USE_PGO} (profile in ${PGO_DATA_DIR})")
endif ()

if (WIN32)
    add_definitions(-D_CRT_SECURE_NO_WARNINGS)
endif (WIN32)

if (USE_VLD)
    add_definitions(-DUSE_VLD)
endif (USE_VLD)
