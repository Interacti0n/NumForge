"""Build an sdist, build its wheel, install it and test without source imports."""
import argparse
import os
from pathlib import Path
import shutil
import subprocess
import sys
import tarfile


def run(arguments, directory):
    # Normalize PATH/Path duplicates inherited from some Windows desktop shells.
    environment, seen = {}, set()
    for key, value in os.environ.items():
        if key.upper() not in seen:
            environment[key] = value
            seen.add(key.upper())
    result = subprocess.run([str(value) for value in arguments], cwd=directory, env=environment,
                            stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
                            encoding="utf-8", errors="replace", timeout=600,
                            **({"creationflags": subprocess.CREATE_NO_WINDOW} if os.name == "nt" else {}))
    if result.returncode:
        print(result.stdout, flush=True)
        result.check_returncode()


def extract_source(archive, destination):
    """Accept only directories/regular files confined to this fresh destination."""
    destination.mkdir()
    root = destination.resolve()
    with tarfile.open(archive, "r:gz") as bundle:
        for member in bundle.getmembers():
            target = (root / member.name).resolve()
            if target != root and root not in target.parents:
                raise RuntimeError("Source archive path escapes staging directory")
            if member.isdir():
                target.mkdir(parents=True, exist_ok=True)
            elif member.isfile():
                target.parent.mkdir(parents=True, exist_ok=True)
                with bundle.extractfile(member) as source, target.open("wb") as output:
                    shutil.copyfileobj(source, output)
            else:
                raise RuntimeError("Source archive contains a non-regular entry")
    roots = [path for path in root.iterdir() if path.is_dir()]
    if len(roots) != 1:
        raise RuntimeError("Expected one source-distribution root")
    return roots[0]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    source = Path(__file__).resolve().parents[1]
    output = args.output.resolve()
    if output.exists():
        parser.error("Use a fresh output directory")
    output.mkdir(parents=True)
    distributions = output / "dist"
    distributions.mkdir()
    existing_directories = {path.resolve() for path in source.iterdir() if path.is_dir()}
    run([sys.executable, "-c", "import sys,setuptools.build_meta as b; b.build_sdist(sys.argv[1])",
         distributions], source)
    archives = list(distributions.glob("numforge-*.tar.gz"))
    if len(archives) != 1:
        raise RuntimeError("Expected one source distribution")
    # Some Windows setuptools versions leave a partial release tree behind.
    # Preserve only a tree created by this invocation, below the supplied output.
    temporary = (source / archives[0].name.removesuffix(".tar.gz")).resolve()
    if temporary.exists() and temporary not in existing_directories:
        if temporary.parent != source:
            raise RuntimeError("Unexpected source staging path")
        shutil.move(temporary, output / "sdist-staging")
    extracted = extract_source(archives[0], output / "source")
    run([sys.executable, "-m", "pip", "wheel", extracted, "--no-build-isolation", "--no-index",
         "--no-deps", "--no-cache-dir", "--wheel-dir", distributions], output)
    wheels = list(distributions.glob("numforge-*.whl"))
    if len(wheels) != 1:
        raise RuntimeError("Expected one platform wheel")
    installed = output / "site-packages"
    run([sys.executable, "-m", "pip", "install", wheels[0], "--no-index", "--no-deps",
         "--no-cache-dir", "--target", installed], output)
    consumer = output / "consumer"
    consumer.mkdir()
    shutil.copy2(source / "tests/python/test_numforge.py", consumer / "test_numforge.py")
    shutil.copy2(source / "examples/python/standalone.py", consumer / "standalone.py")
    test_code = (
        "import pathlib,sys,unittest; site=pathlib.Path(sys.argv[1]).resolve(); "
        "sys.path.insert(0,str(site)); import numforge; from numforge import _native; "
        "assert pathlib.Path(numforge.__file__).resolve().is_relative_to(site); "
        "assert pathlib.Path(_native.__file__).resolve().is_relative_to(site); "
        "suite=unittest.defaultTestLoader.discover(sys.argv[2]); "
        "result=unittest.TextTestRunner(verbosity=2).run(suite); sys.exit(not result.wasSuccessful())"
    )
    run([sys.executable, "-I", "-c", test_code, installed, consumer], consumer)
    run([sys.executable, "-I", "-c",
         "import sys,runpy; sys.path.insert(0,sys.argv[1]); runpy.run_path(sys.argv[2],run_name='__main__')",
         installed, consumer / "standalone.py"], consumer)
    print("Verified source distribution:", archives[0].name)
    print("Verified installed wheel:", wheels[0].name)
    print("Isolated adapter tests and standalone example passed.")


if __name__ == "__main__":
    main()
