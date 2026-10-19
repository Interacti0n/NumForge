"""Build the thin CPython adapter using NumForge's authoritative CMake targets."""
import os
from pathlib import Path
import platform
import subprocess
import sys

from setuptools import Extension, find_packages, setup
from setuptools.command.build_ext import build_ext


class CMakeExtension(Extension):
    def __init__(self, name):
        super().__init__(name, sources=[])


class CMakeBuild(build_ext):
    def build_extension(self, ext):
        if sys.implementation.name != "cpython":
            raise RuntimeError("The initial NumForge adapter requires CPython")
        source = Path(__file__).resolve().parent
        destination = Path(self.get_ext_fullpath(ext.name)).resolve().parent
        # Keep nested source-distribution builds below MSBuild's path limit.
        # The interpreter tag separates incompatible native build caches.
        build = source / "build" / ("nf-" + sys.implementation.cache_tag)
        build.mkdir(parents=True, exist_ok=True)
        args = ["cmake", "-S", str(source), "-B", str(build),
                "-DCMAKE_BUILD_TYPE=Release", "-DBUILD_TESTING=OFF",
                "-DNUMFORGE_BUILD_APPS=OFF", "-DNUMFORGE_BUILD_ENGINE=ON",
                "-DNUMFORGE_BUILD_PYTHON=ON", "-DNUMFORGE_BUILD_BENCHMARKS=OFF",
                "-DNUMFORGE_BUILD_FUZZERS=OFF", "-DNUMFORGE_ENABLE_COVERAGE=OFF",
                "-DNUMFORGE_ENABLE_SANITIZERS=OFF",
                "-DNUMFORGE_WARNINGS_AS_ERRORS=ON",
                "-DPython3_EXECUTABLE=" + sys.executable,
                "-DPython3_ROOT_DIR=" + sys.base_prefix,
                "-DPython3_FIND_STRATEGY=LOCATION",
                "-DNUMFORGE_PYTHON_OUTPUT_DIRECTORY=" + str(destination)]
        if os.name == "nt":
            args.append("-DCMAKE_MSVC_RUNTIME_LIBRARY=MultiThreadedDLL")
            generator = os.environ.get("CMAKE_GENERATOR", "Visual Studio")
            if generator.startswith("Visual Studio"):
                architecture = "Win32" if sys.maxsize <= 2**32 else (
                    "ARM64" if platform.machine().upper() == "ARM64" else "x64")
                args.extend(["-A", architecture])
        subprocess.run(args, check=True)
        subprocess.run(["cmake", "--build", str(build), "--config", "Release",
                        "--target", "numforge_python", "--parallel", "2"], check=True)


setup(
    name="numforge",
    version="0.1.0.dev0",
    description="Exact and high-precision mathematics through the NumForge C engine",
    long_description=Path("bindings/python/README.md").read_text(encoding="utf-8"),
    long_description_content_type="text/markdown",
    license="MIT",
    python_requires=">=3.10",
    packages=find_packages("bindings/python"),
    package_dir={"": "bindings/python"},
    ext_modules=[CMakeExtension("numforge._native")],
    cmdclass={"build_ext": CMakeBuild},
    zip_safe=False,
)
