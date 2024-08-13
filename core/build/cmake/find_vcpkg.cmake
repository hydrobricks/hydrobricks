function (find_vcpkg)
    if (NOT DEFINED CMAKE_TOOLCHAIN_FILE)
        if (DEFINED ENV{VCPKG_ROOT})
            # Try to use the VCPKG_ROOT environment variable
            set(CMAKE_TOOLCHAIN_FILE "$ENV{VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake")
        elseif (EXISTS "${CMAKE_SOURCE_DIR}/vcpkg/scripts/buildsystems/vcpkg.cmake")
            # If vcpkg is cloned into the project directory
            set(CMAKE_TOOLCHAIN_FILE "${CMAKE_SOURCE_DIR}/vcpkg/scripts/buildsystems/vcpkg.cmake")
        elseif (EXISTS "${CMAKE_SOURCE_DIR}/../vcpkg/scripts/buildsystems/vcpkg.cmake")
            # If vcpkg is in the parent directory of the project
            set(CMAKE_TOOLCHAIN_FILE "${CMAKE_SOURCE_DIR}/../vcpkg/scripts/buildsystems/vcpkg.cmake")
        elseif (EXISTS "C:/vcpkg/scripts/buildsystems/vcpkg.cmake")
            # Common path on Windows
            set(CMAKE_TOOLCHAIN_FILE "C:/vcpkg/scripts/buildsystems/vcpkg.cmake")
        elseif (EXISTS "/usr/local/vcpkg/scripts/buildsystems/vcpkg.cmake")
            # Common path on Linux/macOS
            set(CMAKE_TOOLCHAIN_FILE "/usr/local/vcpkg/scripts/buildsystems/vcpkg.cmake")
        else ()
            message(FATAL_ERROR "Could not find vcpkg.cmake. Please set CMAKE_TOOLCHAIN_FILE manually.")
        endif ()
    endif ()

    message(STATUS "Using vcpkg toolchain file: ${CMAKE_TOOLCHAIN_FILE}")
endfunction ()
