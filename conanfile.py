from conans import ConanFile, CMake
import os


class Toolmap(ConanFile):
    settings = "os", "compiler", "build_type", "arch"

    requires = [
        "wxwidgets/3.1.4@terranum-conan+wxwidgets/stable",
        "gdal/3.4.1@terranum-conan+gdal/stable",
        "netcdf/4.7.4",
        "proj/9.0.0",
        "libcurl/7.80.0",
        "libdeflate/1.10",
        "zlib/1.2.12"
    ]

    options = {"build_tests": [True, False]}
    generators = "cmake", "gcc", "txt"

    def requirements(self):
        if self.options.build_tests:
            self.requires("gtest/1.11.0")

    def configure(self):
        self.options["gdal"].with_curl = True
        self.options["gdal"].shared = True
        if self.settings.os == "Linux":
            self.options["wxwidgets"].webview = False  # Webview control isn't available on linux.

    def imports(self):
        # Copy libraries
        self.copy("*.dll", dst="bin", src="bin")  # From bin to bin
        self.copy("*.dylib", dst="bin", src="lib")
        if self.settings.os == "Linux":
            self.copy("*.so*", dst="bin", src="lib")

        # Copy proj library datum
        if self.settings.os == "Windows" or self.settings.os == "Linux":
            self.copy("*", dst="bin", src="res", root_package="proj")
        if self.settings.os == "Macos":
            self.copy("*", dst="bin/ToolMap.app/Contents/share/proj", src="res", root_package="proj")

    def build(self):
        cmake = CMake(self)
        if self.options.unit_test:
            cmake.definitions["BUILD_TESTS"] = "ON"
        cmake.configure()
        cmake.build()
        cmake.install()
