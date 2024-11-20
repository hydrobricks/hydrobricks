import os
import re
import subprocess
import sys
from pathlib import Path

from setuptools import Extension, setup
from setuptools.command.build_ext import build_ext

# Convert distutils Windows platform specifiers to CMake -A arguments
PLAT_TO_CMAKE = {
    "win32": "Win32",
    "win-amd64": "x64",
    "win-arm32": "ARM",
    "win-arm64": "ARM64",
}


class CMakeExtension(Extension):
    def __init__(self, name, source_dir=""):
        Extension.__init__(self, name, sources=[])
        self.source_dir = os.path.abspath(source_dir)


class CMakeBuild(build_ext):
    def build_extension(self, ext):
        # Flag to build in debug mode
        # self.debug = True

        # Define the build directory
        ext_dir = os.path.abspath(os.path.dirname(self.get_ext_fullpath(ext.name)))
        build_temp = os.path.join(self.build_temp, ext.name)
        if not os.path.exists(build_temp):
            os.makedirs(build_temp)

        # Required for auto-detection & inclusion of auxiliary "native" libs
        if not ext_dir.endswith(os.path.sep):
            ext_dir += os.path.sep

        # Set VCPKG_LIBRARY_LINKAGE to static
        os.environ["VCPKG_LIBRARY_LINKAGE"] = "static"

        # Check if VCPKG_ROOT is set
        if "VCPKG_ROOT" not in os.environ:
            print("-- VCPKG_ROOT is not set. Trying to install vcpkg.")
            # Install vcpkg
            if not os.path.exists("vcpkg"):
                subprocess.check_call(
                    ["git", "clone", "https://github.com/microsoft/vcpkg.git"])
            else:
                subprocess.check_call(["git", "pull"], cwd="vcpkg")
            if sys.platform.startswith("win"):
                subprocess.check_call(["vcpkg\\bootstrap-vcpkg.bat"])
            else:
                subprocess.check_call(["vcpkg/bootstrap-vcpkg.sh"])
            os.environ["VCPKG_ROOT"] = os.path.abspath("vcpkg")
            os.environ["PATH"] = (os.path.abspath("vcpkg") + os.pathsep +
                                  os.environ["PATH"])
            os.environ["CMAKE_TOOLCHAIN_FILE"] = os.path.abspath(
                "vcpkg/scripts/buildsystems/vcpkg.cmake")
        else:
            print(f"-- VCPKG_ROOT is set to {os.environ['VCPKG_ROOT']}")
            os.environ["CMAKE_TOOLCHAIN_FILE"] = os.path.join(
                os.environ["VCPKG_ROOT"], "scripts/buildsystems/vcpkg.cmake")

        # Print some debug information
        print(f"-- Working directory: {os.getcwd()}")
        print(f"-- Directory content: {os.listdir()}")
        print(f"-- Path build_temp: {build_temp}")
        print(f"-- Path ext_dir: {ext_dir}")
        print(f"-- Python executable: {sys.executable}")

        # Check if the debug mode is enabled
        debug = int(os.environ.get("DEBUG", 0)) if self.debug is None else self.debug
        cfg = "Debug" if debug else "Release"
        print(f"-- Build configuration: {cfg}")

        # CMake configuration
        cmake_generator = os.environ.get("CMAKE_GENERATOR", "")
        cmake_args = [
            f"-DCMAKE_LIBRARY_OUTPUT_DIRECTORY={ext_dir}",
            f"-DPYTHON_EXECUTABLE={sys.executable}",
            f"-DCMAKE_BUILD_TYPE={cfg}",
            f"-DCMAKE_BINARY_DIR={build_temp}",
            f"-DCMAKE_INSTALL_PREFIX={ext_dir}",
            f"-DCMAKE_TOOLCHAIN_FILE={os.environ['CMAKE_TOOLCHAIN_FILE']}",
            "-DBUILD_PYBINDINGS=1",
            "-DBUILD_CLI=0",
            "-DBUILD_TESTS=0",
        ]

        build_args = []
        # Adding CMake arguments set as environment variable
        # (needed e.g. to build for ARM OSx on conda-forge)
        if "CMAKE_ARGS" in os.environ:
            cmake_args += [item for item in os.environ["CMAKE_ARGS"].split(" ") if item]

        if "PYBIND11_PYTHON_VERSION" in os.environ:
            cmake_args += [f"-DPYBIND11_PYTHON_VERSION={os.environ['PYBIND11_PYTHON_VERSION']}"]
            print(f"-- Setting Python version: {os.environ['PYBIND11_PYTHON_VERSION']}")
        else:
            print("-- Python version not set.")

        if "PYBIND11_PYTHON_ROOT" in os.environ:
            cmake_args += [f"-DPYBIND11_PYTHON_ROOT={os.environ['PYBIND11_PYTHON_ROOT']}"]
            print(f"-- Setting Python root: {os.environ['PYBIND11_PYTHON_ROOT']}")
        else:
            print("-- Python root not set.")

        if self.compiler.compiler_type == "msvc":
            single_config = any(x in cmake_generator for x in {"NMake", "Ninja"})
            contains_arch = any(x in cmake_generator for x in {"ARM", "Win64"})

            # Specify the arch if using MSVC generator, but only if it doesn't
            # contain a backward-compatibility arch spec already in the
            # generator name.
            if not single_config and not contains_arch:
                cmake_args += ["-A", PLAT_TO_CMAKE[self.plat_name]]

            # Multi-config generators have a different way to specify configs
            if not single_config:
                cmake_args += [
                    f"-DCMAKE_LIBRARY_OUTPUT_DIRECTORY_{cfg.upper()}={ext_dir}"
                ]
                build_args += ["--config", cfg]

        if sys.platform.startswith("darwin"):
            # Cross-compile support for macOS - respect ARCHFLAGS if set
            archs = re.findall(r"-arch (\S+)", os.environ.get("ARCHFLAGS", ""))
            if archs:
                cmake_args += ["-DCMAKE_OSX_ARCHITECTURES={}".format(";".join(archs))]

        # Set CMAKE_BUILD_PARALLEL_LEVEL to control the parallel build level.
        if "CMAKE_BUILD_PARALLEL_LEVEL" not in os.environ:
            if hasattr(self, "parallel") and self.parallel:
                build_args += [f"-j{self.parallel}"]

        subprocess.check_call(["vcpkg", "install"])
        subprocess.check_call(["cmake", ext.source_dir] + cmake_args, cwd=build_temp)
        subprocess.check_call(["cmake", "--build", "."] + build_args, cwd=build_temp)


# Read the contents of the README file
this_directory = Path(__file__).parent
read_me_file = this_directory / "python" / "README.md"
if read_me_file.exists():
    long_description = open("python/README.md").read()
else:
    long_description = "A modular hydrological modelling framework."

# Setup
setup(
    name="hydrobricks",
    version="0.7.5",
    author="Pascal Horton",
    author_email="pascal.horton@unibe.ch",
    description="A modular hydrological modelling framework",
    long_description=long_description,
    long_description_content_type="text/markdown",
    ext_modules=[CMakeExtension("_hydrobricks")],
    cmdclass={"build_ext": CMakeBuild},
    packages=['hydrobricks', 'hydrobricks.models', 'hydrobricks.behaviours'],
    package_dir={'hydrobricks': 'python/src/hydrobricks',
                 'hydrobricks.models': 'python/src/hydrobricks/models',
                 'hydrobricks.behaviours': 'python/src/hydrobricks/behaviours',
                 },
    zip_safe=False,
    extras_require={"test": ["pytest>=6.0"]},
    python_requires=">=3.8",
    install_requires=[
        'cftime',
        'numpy',
        'HydroErr',
        'pandas',
        'pyyaml',
        'StrEnum'
    ],
    classifiers=[
        "Programming Language :: C++",
        "Programming Language :: Python :: 3",
        "License :: OSI Approved :: MIT License",
    ],
    license="MIT",
    project_urls={
        "Source Code": "https://github.com/hydrobricks/hydrobricks",
        "Bug Tracker": "https://github.com/hydrobricks/hydrobricks/issues",
    },
)
