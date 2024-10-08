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
        # To build the extension in debug mode.
        # self.debug = True

        ext_dir = os.path.abspath(os.path.dirname(self.get_ext_fullpath(ext.name)))

        build_temp = os.path.join(self.build_temp, ext.name)
        if not os.path.exists(build_temp):
            os.makedirs(build_temp)

        print(f"build_temp: {build_temp}")
        print(f"ext_dir: {ext_dir}")

        # Required for auto-detection & inclusion of auxiliary "native" libs
        if not ext_dir.endswith(os.path.sep):
            ext_dir += os.path.sep

        debug = int(os.environ.get("DEBUG", 0)) if self.debug is None else self.debug
        cfg = "Debug" if debug else "Release"

        cmake_generator = os.environ.get("CMAKE_GENERATOR", "")

        cmake_args = [
            f"-DCMAKE_LIBRARY_OUTPUT_DIRECTORY={ext_dir}",
            f"-DPYTHON_EXECUTABLE={sys.executable}",
            f"-DCMAKE_BUILD_TYPE={cfg}",
            "-DBUILD_PYBINDINGS=1",
            "-DBUILD_CLI=0",
            "-DBUILD_TESTS=0",
        ]

        build_args = []
        # Adding CMake arguments set as environment variable
        # (needed e.g. to build for ARM OSx on conda-forge)
        if "CMAKE_ARGS" in os.environ:
            cmake_args += [item for item in os.environ["CMAKE_ARGS"].split(" ") if item]

        if self.compiler.compiler_type != "msvc":
            cmake_args += ["--preset=linux-release"]

        else:
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
            cmake_args += ["--preset=osx-release"]
            # Cross-compile support for macOS - respect ARCHFLAGS if set
            archs = re.findall(r"-arch (\S+)", os.environ.get("ARCHFLAGS", ""))
            if archs:
                cmake_args += ["-DCMAKE_OSX_ARCHITECTURES={}".format(";".join(archs))]

        if sys.platform.startswith("linux"):
            cmake_args += ["--preset=linux-release"]

        # Set CMAKE_BUILD_PARALLEL_LEVEL to control the parallel build level.
        if "CMAKE_BUILD_PARALLEL_LEVEL" not in os.environ:
            if hasattr(self, "parallel") and self.parallel:
                build_args += [f"-j{self.parallel}"]

        # Override the build directory
        cmake_args += [f"-DCMAKE_BINARY_DIR={build_temp}"]

        # Override the CMAKE_INSTALL_PREFIX
        cmake_args += [f"-DCMAKE_INSTALL_PREFIX={ext_dir}"]

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
    version="0.7.4",
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
