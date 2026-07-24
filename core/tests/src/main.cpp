#include <gtest/gtest.h>

#include <cstdio>
#include <filesystem>

#include "Includes.h"
#include "Utils.h"

int main(int argc, char** argv) {
    int resultTest = -2;

    try {
        ::testing::InitGoogleTest(&argc, argv);

        // Initialize the library
        if (!InitHydrobricks()) {
            printf("Failed to initialize hydrobricks\n");
            return 1;
        }

        // Locate the test files directory relative to CWD
        auto cwd = std::filesystem::current_path();
        if (std::filesystem::is_directory(cwd / "core/tests/files")) {
            std::filesystem::current_path(cwd / "core/tests");
        } else if (std::filesystem::is_directory(cwd / "../core/tests/files")) {
            std::filesystem::current_path(cwd / "../core/tests");
        } else if (std::filesystem::is_directory(cwd / "../../core/tests/files")) {
            std::filesystem::current_path(cwd / "../../core/tests");
        } else {
            printf("Cannot find the files directory\n");
            printf("Original working directory: %s\n", cwd.string().c_str());
            return 0;
        }

        resultTest = RUN_ALL_TESTS();

    } catch (std::exception& e) {
        printf("Exception caught: %s\n", e.what());
    }

    return resultTest;
}
