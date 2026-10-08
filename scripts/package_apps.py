"""Create and smoke-check portable application archives from a Release build.

Python is a maintainer dependency only; downloaded applications need none.
Existing package directories/archives are never overwritten.
"""
import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import shutil
import socket
import struct
import subprocess
import tarfile
import time
import urllib.request
import zipfile


def execute(arguments, **kwargs):
    if os.name == "nt":
        kwargs["creationflags"] = subprocess.CREATE_NO_WINDOW
    return subprocess.run(arguments, check=True, capture_output=True, text=True, **kwargs).stdout


def smoke(directory, windows):
    suffix = ".exe" if windows else ""
    output = execute([str(directory / ("calculator" + suffix))],
                     input="precision full\nnotation plain\n0.1+0.2\n2^128\nnotation fraction\n1/3\nquit\n", timeout=15)
    results = re.findall(r"= ([^\r\n]*)", output)
    if results != ["0.3", "340282366920938463463374607431768211456", "1/3"]:
        raise RuntimeError("Extracted CLI returned unexpected results")
    # Find an unused loopback port without changing a user's existing server.
    with socket.socket() as probe:
        probe.bind(("127.0.0.1", 0))
        port = probe.getsockname()[1]
    options = {"creationflags": subprocess.CREATE_NO_WINDOW} if windows else {}
    process = subprocess.Popen([str(directory / ("numforge_web" + suffix)),
                                "--no-browser", "--port", str(port)],
                               cwd=directory, stdout=subprocess.DEVNULL,
                               stderr=subprocess.PIPE, **options)
    base = f"http://127.0.0.1:{port}"
    try:
        deadline = time.monotonic() + 15
        while True:
            if process.poll() is not None:
                raise RuntimeError("Extracted web server exited before startup")
            try:
                with urllib.request.urlopen(base + "/?lang=en", timeout=1) as response:
                    html = response.read().decode("utf-8")
                break
            except OSError:
                if time.monotonic() > deadline:
                    raise RuntimeError("Extracted web server did not start")
                time.sleep(0.1)
        if 'id="expression"' not in html:
            raise RuntimeError("Calculator HTML missing from extracted package")
        for asset in set(re.findall(r'(?:src|href)="(/assets/[^"?#]+)', html)) | {"/LICENSE"}:
            with urllib.request.urlopen(base + asset, timeout=5) as response:
                if not response.read():
                    raise RuntimeError(f"Empty embedded asset: {asset}")
        request = urllib.request.Request(base + "/api/evaluate?precision=full&angle=rad",
                                         data=b"1/3", method="POST")
        with urllib.request.urlopen(request, timeout=5) as response:
            result = json.load(response)
        if not result.get("ok") or result.get("result") != "1/3":
            raise RuntimeError("Extracted HTTP calculator returned an unexpected result")
    finally:
        process.terminate()
        try:
            process.wait(timeout=5)
        except subprocess.TimeoutExpired:
            process.kill()
            process.wait(timeout=5)
        process.stderr.close()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--source", required=True, type=Path)
    parser.add_argument("--build", required=True, type=Path)
    parser.add_argument("--output", required=True, type=Path)
    parser.add_argument("--version", required=True)
    parser.add_argument("--commit", required=True)
    parser.add_argument("--platform", required=True, choices=["win-x64", "linux-x64"])
    args = parser.parse_args()
    if (not re.fullmatch(r"\d+\.\d+\.\d+(?:-[0-9A-Za-z-]+(?:\.[0-9A-Za-z-]+)*)?", args.version)
            or not re.fullmatch(r"[0-9a-f]{40}", args.commit)):
        parser.error("Use an X.Y.Z or X.Y.Z-prerelease version and full source commit SHA")
    source, build, output = (p.resolve() for p in (args.source, args.build, args.output))
    version = re.search(r"project\(NumForge\s+VERSION\s+([\d.]+)", (source / "CMakeLists.txt").read_text())
    if version is None or version.group(1) != args.version.split("-", 1)[0]:
        raise RuntimeError("Package version does not match source version")
    cache = {}
    for line in (build / "CMakeCache.txt").read_text().splitlines():
        match = re.match(r"([^:#]+):[^=]+=(.*)", line)
        if match:
            cache[match.group(1)] = match.group(2)
    if Path(cache["CMAKE_HOME_DIRECTORY"]).resolve() != source:
        raise RuntimeError("Build cache belongs to a different source checkout")
    if (cache.get("NUMFORGE_BUILD_APPS") != "ON" or cache.get("NUMFORGE_BUILD_BENCHMARKS") != "OFF"
            or cache.get("BUILD_TESTING") != "OFF"):
        raise RuntimeError("Build production applications with tests and benchmarks OFF")
    windows = args.platform == "win-x64"
    if windows != (os.name == "nt"):
        raise RuntimeError("Package platform does not match this host")
    if windows and cache.get("CMAKE_MSVC_RUNTIME_LIBRARY") != "MultiThreaded":
        raise RuntimeError("Windows packages require an MSVC static Release runtime (MultiThreaded)")
    if not windows and cache.get("CMAKE_BUILD_TYPE") != "Release":
        raise RuntimeError("Linux packages require CMAKE_BUILD_TYPE=Release")
    name = f"NumForge-{args.version}-{args.platform}"
    extension = ".zip" if windows else ".tar.gz"
    directory, archive = output / name, output / (name + extension)
    if directory.exists() or archive.exists() or (output / "extracted-check").exists():
        raise RuntimeError("Use a fresh output directory; existing packages are preserved")
    directory.mkdir(parents=True)
    compiler_files = list((build / "CMakeFiles").glob("*/CMakeCCompiler.cmake"))
    if len(compiler_files) != 1:
        raise RuntimeError("Cannot identify the build compiler")
    compiler_text = compiler_files[0].read_text()
    def compiler_field(key):
        match = re.search(rf'set\({key} "([^"]*)"\)', compiler_text)
        if not match:
            raise RuntimeError(f"Missing compiler field {key}")
        return match.group(1)
    info = {"version": args.version, "source_commit": args.commit,
            "platform": args.platform, "configuration": "Release",
            "compiler": compiler_field("CMAKE_C_COMPILER_ID"),
            "compiler_version": compiler_field("CMAKE_C_COMPILER_VERSION"),
            "embedded_web_assets": True, "benchmarks": False, "dependencies": {}}
    for app in ["numforge_web", "calculator"]:
        filename = app + (".exe" if windows else "")
        binary = build / "Release" / filename if windows else build / filename
        data = binary.read_bytes()
        if windows:
            offset = struct.unpack_from("<I", data, 60)[0]
            if data[offset:offset + 4] != b"PE\0\0" or struct.unpack_from("<H", data, offset + 4)[0] != 0x8664:
                raise RuntimeError("Expected an x64 Windows executable")
            dumpbin = Path(compiler_field("CMAKE_C_COMPILER")).with_name("dumpbin.exe")
            dependencies = re.findall(r"^\s+([\w.-]+\.dll)\s*$",
                                      execute([str(dumpbin), "/dependents", str(binary)]), re.MULTILINE | re.IGNORECASE)
            if any(re.match(r"(vcruntime|msvcp|ucrtbase|api-ms-win-crt|libgcc|libwinpthread)", dep, re.IGNORECASE) for dep in dependencies):
                raise RuntimeError("Executable still imports a non-bundled compiler runtime")
            info["dependencies"][filename] = dependencies
        else:
            if data[:5] != b"\x7fELF\x02" or data[5] != 1 or struct.unpack_from("<H", data, 18)[0] != 62:
                raise RuntimeError("Expected an x64 Linux ELF executable")
            dependencies = execute(["ldd", str(binary)])
            if "not found" in dependencies:
                raise RuntimeError("Missing Linux runtime dependency")
            info["dependencies"][filename] = re.findall(r"^\s*(\S+)\s+=>", dependencies, re.MULTILINE)
            versions = re.findall(r"GLIBC_(\d+\.\d+(?:\.\d+)?)", execute(["readelf", "--version-info", str(binary)]))
            if versions:
                required = max(versions, key=lambda value: tuple(map(int, value.split("."))))
                previous = info.get("minimum_glibc", "0.0")
                info["minimum_glibc"] = max([required, previous], key=lambda value: tuple(map(int, value.split("."))))
        shutil.copy2(binary, directory / filename)
    shutil.copy2(source / "LICENSE", directory / "LICENSE")
    launch = "Double-click numforge_web.exe, or run .\\numforge_web.exe from PowerShell." if windows else "Run ./numforge_web from a terminal."
    cli = ".\\calculator.exe" if windows else "./calculator"
    compatibility = "Windows x64; C runtime linked statically. Standard Windows system DLLs are required." if windows else f"Linux x64 with glibc {info.get('minimum_glibc', 'unknown')} or newer; not an Alpine/musl build."
    (directory / "START_HERE.txt").write_text(
        f"NumForge {args.version} ({args.platform})\n\nExtract the entire archive before running.\n"
        f"{launch}\nBrowser: http://127.0.0.1:8765\n"
        "No compiler, Node.js, Python or database is needed.\n"
        "Keep the server running while using the calculator; close its terminal to stop it.\n"
        "If the port is busy, use --port 8766. Use --no-browser to launch without opening a browser.\n\n"
        f"CLI: {cli} (type quit to exit).\n\n"
        f"Compatibility: {compatibility}\nSource commit: {args.commit}\n"
        + ("Unsigned binaries; Windows may show a reputation warning.\n" if windows else "") +
        "Source, documentation and issues: https://github.com/Interacti0n/NumForge\n",
        encoding="utf-8")
    expected_files = {"numforge_web" + (".exe" if windows else ""),
                      "calculator" + (".exe" if windows else ""), "LICENSE", "START_HERE.txt"}
    if {file.name for file in directory.iterdir()} != expected_files:
        raise RuntimeError("Portable archive contains unexpected files")
    if windows:
        with zipfile.ZipFile(archive, "w", zipfile.ZIP_DEFLATED) as bundle:
            for file in sorted(directory.iterdir()):
                bundle.write(file, f"{name}/{file.name}")
        extracted = output / "extracted-check"
        with zipfile.ZipFile(archive) as bundle:
            bundle.extractall(extracted)
    else:
        with tarfile.open(archive, "w:gz") as bundle:
            bundle.add(directory, arcname=name)
        extracted = output / "extracted-check"
        with tarfile.open(archive) as bundle:
            bundle.extractall(extracted, filter="data")
    smoke(extracted / name, windows)
    checksum = hashlib.sha256(archive.read_bytes()).hexdigest()
    (output / (archive.name + ".sha256")).write_text(
        f"{checksum}  {archive.name}\n", encoding="ascii", newline="\n")
    print(f"Created and verified {archive.name}\nSHA256: {checksum}")
    print("Build verification: " + json.dumps(info, sort_keys=True))


if __name__ == "__main__":
    main()
