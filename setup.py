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
    """Setuptools extension for CMake-based builds."""

    def __init__(self, name, source_dir=""):
        super().__init__(name, sources=[])
        self.source_dir = os.path.abspath(source_dir)


class CMakeBuild(build_ext):
    """Custom build_ext command to run CMake and build the C++ extension."""

    def build_extension(self, ext):
        # Flag to build in debug mode
        # self.debug = True

        # Check if the debug mode is enabled
        debug = int(os.environ.get("DEBUG", 0)) if self.debug is None else self.debug
        cfg = "Debug" if debug else "Release"
        print(f"-- Building in {cfg} mode")

        # Define the build directory. For an editable install, setuptools points
        # build_temp at a throw-away directory that it deletes as soon as the build is
        # over; on Windows the MSBuild processes outlive the build and keep a handle on
        # their output directory, so that deletion fails ("[WinError 32] The process
        # cannot access the file because it is being used by another process") and takes
        # the whole install down after the extension has been built. Build in a stable
        # directory under build/ instead (the one a non-editable build already uses):
        # the temporary directory is then empty when setuptools removes it, and the
        # CMake cache survives from one build to the next.
        ext_dir = os.path.abspath(os.path.dirname(self.get_ext_fullpath(ext.name)))
        build_root = os.path.join(
            os.path.dirname(os.path.abspath(__file__)),
            "build",
            f"temp.{self.plat_name}-{sys.implementation.cache_tag}",
            cfg,
        )
        build_temp = os.path.join(build_root, ext.name)
        os.makedirs(build_temp, exist_ok=True)

        # Required for auto-detection & inclusion of auxiliary "native" libs
        if not ext_dir.endswith(os.path.sep):
            ext_dir += os.path.sep

        # Ensure VCPKG is set up
        os.environ["VCPKG_LIBRARY_LINKAGE"] = "static"

        # pip's build isolation installs CMake as a console-script shim that needs its
        # own package on PYTHONPATH, and puts it first on the PATH. vcpkg picks it up as
        # the system CMake but scrubs the environment when it spawns build tools, so the
        # shim then fails with "No module named 'cmake'" (compiler detection, and every
        # port build after it). Let PYTHONPATH through to keep that shim usable.
        kept = [v for v in os.environ.get("VCPKG_KEEP_ENV_VARS", "").split(";") if v]
        if "PYTHONPATH" not in kept:
            kept.append("PYTHONPATH")
        os.environ["VCPKG_KEEP_ENV_VARS"] = ";".join(kept)

        if "VCPKG_ROOT" not in os.environ:
            print("-- VCPKG_ROOT not found. Setting up vcpkg...")
            if not os.path.exists("vcpkg"):
                subprocess.check_call(
                    ["git", "clone", "https://github.com/microsoft/vcpkg.git"]
                )
            subprocess.check_call(["git", "pull"], cwd="vcpkg")
            if sys.platform.startswith("win"):
                subprocess.check_call(["vcpkg\\bootstrap-vcpkg.bat"])
            else:
                subprocess.check_call(["vcpkg/bootstrap-vcpkg.sh"])
            os.environ["VCPKG_ROOT"] = os.path.abspath("vcpkg")
            os.environ["PATH"] = os.pathsep.join(
                [os.environ["VCPKG_ROOT"], os.environ["PATH"]]
            )
        else:
            print(f"-- VCPKG_ROOT found: {os.environ['VCPKG_ROOT']}")

        os.environ["CMAKE_TOOLCHAIN_FILE"] = os.path.join(
            os.environ["VCPKG_ROOT"], "scripts/buildsystems/vcpkg.cmake"
        )

        # Print some debug information
        print(f"-- Working directory: {os.getcwd()}")
        print(f"-- Directory content: {os.listdir()}")
        print(f"-- Path build_temp: {build_temp}")
        print(f"-- Path ext_dir: {ext_dir}")
        print(f"-- Python executable: {sys.executable}")

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

        # Add user-specified CMake arguments (for cross-compilation, etc.)
        if "CMAKE_ARGS" in os.environ:
            cmake_args.extend(
                item for item in os.environ["CMAKE_ARGS"].split(" ") if item
            )
        if "PYBIND11_PYTHON_VERSION" in os.environ:
            cmake_args.append(
                f"-DPYBIND11_PYTHON_VERSION={os.environ['PYBIND11_PYTHON_VERSION']}"
            )
        if "PYBIND11_PYTHON_ROOT" in os.environ:
            cmake_args.append(
                f"-DPYBIND11_PYTHON_ROOT={os.environ['PYBIND11_PYTHON_ROOT']}"
            )

        # Handle MSVC-specific configurations
        cmake_generator = os.environ.get("CMAKE_GENERATOR", "")
        if self.compiler.compiler_type == "msvc":
            single_config = any(x in cmake_generator for x in {"NMake", "Ninja"})
            contains_arch = any(x in cmake_generator for x in {"ARM", "Win64"})

            if not single_config and not contains_arch:
                cmake_args.append(f"-A {PLAT_TO_CMAKE[self.plat_name]}")

            if not single_config:
                cmake_args.append(
                    f"-DCMAKE_LIBRARY_OUTPUT_DIRECTORY_{cfg.upper()}={ext_dir}"
                )
                build_args.append(f"--config {cfg}")

        # Handle macOS cross-compilation
        if sys.platform.startswith("darwin"):
            archs = re.findall(r"-arch (\S+)", os.environ.get("ARCHFLAGS", ""))
            if archs:
                cmake_args.append(f"-DCMAKE_OSX_ARCHITECTURES={';'.join(archs)}")

        # Set parallel build level if specified
        if "CMAKE_BUILD_PARALLEL_LEVEL" not in os.environ:
            if hasattr(self, "parallel") and self.parallel:
                build_args.append(f"-j{self.parallel}")

        subprocess.check_call(["vcpkg", "install"])
        subprocess.check_call(["cmake", ext.source_dir] + cmake_args, cwd=build_temp)
        subprocess.check_call(["cmake", "--build", "."] + build_args, cwd=build_temp)


# Read long description from README
this_directory = Path(__file__).parent
readme_file = this_directory / "python" / "README.md"
long_description = (
    readme_file.read_text()
    if readme_file.exists()
    else "A modular hydrological modelling framework."
)

# Setup configuration
setup(
    long_description=long_description,
    long_description_content_type="text/markdown",
    ext_modules=[CMakeExtension("hydrobricks._hydrobricks")],
    cmdclass={"build_ext": CMakeBuild},
)
