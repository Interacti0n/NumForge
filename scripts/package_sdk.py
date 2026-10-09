"""Package the installed numeric C API and verify relocated C/C++ consumers."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import shutil
import subprocess
import tarfile
import zipfile


def run(arguments):
    # Windows tools may inherit both PATH and Path from the desktop shell.
    environment = {}
    seen = set()
    for key, value in os.environ.items():
        if key.upper() not in seen:
            environment[key] = value
            seen.add(key.upper())
    subprocess.run([str(value) for value in arguments], check=True, env=environment, timeout=600,
                   **({'creationflags': subprocess.CREATE_NO_WINDOW} if os.name == 'nt' else {}))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ['source', 'build', 'output', 'support']:
        parser.add_argument('--' + name, required=True, type=Path)
    parser.add_argument('--version', required=True)
    parser.add_argument('--commit', required=True)
    parser.add_argument('--platform', required=True, choices=['win-x64', 'linux-x64'])
    args = parser.parse_args()
    if (not re.fullmatch(r'\d+\.\d+\.\d+(?:-[0-9A-Za-z-]+(?:\.[0-9A-Za-z-]+)*)?', args.version)
            or not re.fullmatch(r'[0-9a-f]{40}', args.commit)):
        parser.error('Use a release version and full source SHA')
    source, build, output, support = [p.resolve() for p in [args.source, args.build, args.output, args.support]]
    version = re.search(r'project\(NumForge\s+VERSION\s+([\d.]+)', (source / 'CMakeLists.txt').read_text())
    if not version or version.group(1) != args.version.split('-', 1)[0]:
        raise RuntimeError('Source version differs from archive version')
    cache = dict(re.findall(r'^([^\r\n:#]+):[^=\r\n]+=(.*)$', (build / 'CMakeCache.txt').read_text(), re.MULTILINE))
    if Path(cache['CMAKE_HOME_DIRECTORY']).resolve() != source:
        raise RuntimeError('Build uses a different checkout')
    if any(cache.get(key) != 'OFF' for key in ['BUILD_TESTING', 'NUMFORGE_BUILD_APPS', 'NUMFORGE_BUILD_BENCHMARKS']):
        raise RuntimeError('SDK requires an uninstrumented library-only build')
    if any(cache.get(key) == 'ON' for key in ['NUMFORGE_ENABLE_SANITIZERS', 'NUMFORGE_ENABLE_COVERAGE', 'NUMFORGE_BUILD_FUZZERS']):
        raise RuntimeError('Instrumentation is unsuitable for the SDK')
    windows = args.platform == 'win-x64'
    if windows != (os.name == 'nt'):
        raise RuntimeError('Host/platform mismatch')
    if windows and (cache.get('CMAKE_MSVC_RUNTIME_LIBRARY') != 'MultiThreadedDLL' or cache.get('CMAKE_GENERATOR_PLATFORM') != 'x64'):
        raise RuntimeError('Windows SDK requires MSVC x64 /MD')
    if not windows and cache.get('CMAKE_BUILD_TYPE') != 'Release':
        raise RuntimeError('Linux SDK requires Release')
    compiler_files = list((build / 'CMakeFiles').glob('*/CMakeCCompiler.cmake'))
    if len(compiler_files) != 1:
        raise RuntimeError('Cannot identify compiler')
    compiler = compiler_files[0].read_text()
    def field(key):
        match = re.search(rf'set\({key} "([^"\n]*)"\)', compiler)
        if not match:
            raise RuntimeError('Missing compiler field: ' + key)
        return match.group(1)
    if field('CMAKE_C_SIZEOF_DATA_PTR') != '8' or field('CMAKE_C_COMPILER_ID') != ('MSVC' if windows else 'GNU'):
        raise RuntimeError('Expected x64 MSVC/GCC SDK')
    name = f'NumForge-{args.version}-sdk-{args.platform}'
    extension = '.zip' if windows else '.tar.gz'
    archive = output / (name + extension)
    if output.exists():
        raise RuntimeError('Use a fresh SDK output directory')
    output.mkdir(parents=True)
    installed = output / 'installed'
    run(['cmake', '--install', build, '--config', 'Release', '--prefix', installed])
    package = output / name
    package.mkdir()
    # Allowlist: no application internals, test files, screenshots or build debris.
    shutil.copytree(installed / 'include', package / 'include')
    shutil.copytree(installed / 'lib', package / 'lib')
    library = 'numforge.lib' if windows else 'libnumforge.a'
    expected = {'lib/' + library} | {'include/numforge/' + p.name for p in (source / 'include/numforge').glob('*.h')}
    expected |= {'lib/cmake/NumForge/' + filename for filename in ['NumForgeConfig.cmake', 'NumForgeConfigVersion.cmake', 'NumForgeTargets.cmake', 'NumForgeTargets-release.cmake']}
    actual = {p.relative_to(package).as_posix() for p in package.rglob('*') if p.is_file()}
    if actual != expected:
        raise RuntimeError('Unexpected installed SDK files: ' + str(actual.symmetric_difference(expected)))
    shutil.copy2(source / 'LICENSE', package / 'LICENSE')
    shutil.copytree(support / 'examples/sdk', package / 'example')
    readme = (support / 'docs/guides/SDK.md').read_text(encoding='utf-8').replace('/blob/v2.2.0/', f'/blob/v{args.version}/')
    (package / 'README.md').write_text(readme, encoding='utf-8', newline='\n')
    info = {'version': args.version, 'source_commit': args.commit, 'platform': args.platform,
            'compiler': field('CMAKE_C_COMPILER_ID'), 'compiler_version': field('CMAKE_C_COMPILER_VERSION'),
            'configuration': 'Release', 'library': 'static', 'runtime': '/MD (MultiThreadedDLL)' if windows else 'glibc',
            'architecture': 'x64', 'lto': False}
    if not windows:
        info['build_glibc'] = subprocess.check_output(['getconf', 'GNU_LIBC_VERSION'], text=True).strip()
    (package / 'BUILDINFO.json').write_text(json.dumps(info, indent=2) + '\n', encoding='utf-8', newline='\n')
    if windows:
        with zipfile.ZipFile(archive, 'w', zipfile.ZIP_DEFLATED) as bundle:
            for path in sorted(package.rglob('*')):
                if path.is_file():
                    bundle.write(path, path.relative_to(output))
    else:
        with tarfile.open(archive, 'w:gz') as bundle:
            bundle.add(package, arcname=name)
    relocated = output / 'relocated'
    if windows:
        with zipfile.ZipFile(archive) as bundle:
            bundle.extractall(relocated)
    else:
        with tarfile.open(archive) as bundle:
            bundle.extractall(relocated, filter='data')
    prefix = relocated / name
    # Remove staging copies: consumers must resolve the extracted archive alone.
    shutil.rmtree(installed)
    shutil.rmtree(package)
    for label, project, extra in [('example', prefix / 'example', []),
                                  ('consumer', source / 'tests/package_consumer', ['-DNUMFORGE_TEST_CPP=ON'])]:
        consumer_build = output / ('verify-' + label)
        command = ['cmake', '-S', project, '-B', consumer_build, '-DCMAKE_BUILD_TYPE=Release',
                   '-DCMAKE_PREFIX_PATH=' + str(prefix), '-DNumForge_DIR=' + str(prefix / 'lib/cmake/NumForge')]
        if windows:
            command += ['-G', cache['CMAKE_GENERATOR'], '-A', 'x64', '-DCMAKE_MSVC_RUNTIME_LIBRARY=MultiThreadedDLL']
        run(command + extra)
        run(['cmake', '--build', consumer_build, '--config', 'Release', '--parallel', '2'])
        run(['ctest', '--test-dir', consumer_build, '-C', 'Release', '--output-on-failure', '--no-tests=error', '--timeout', '60'])
    digest = hashlib.sha256(archive.read_bytes()).hexdigest()
    Path(str(archive) + '.sha256').write_text(f'{digest}  {archive.name}\n', encoding='ascii', newline='\n')
    print('Created and verified ' + archive.name + '\nSHA256: ' + digest)
    print('Build verification: ' + json.dumps(info, sort_keys=True))


if __name__ == '__main__':
    main()
