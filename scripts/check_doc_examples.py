"""Execute maintained Python/C documentation examples against installed packages."""
import argparse
from contextlib import redirect_stdout
from io import StringIO
from pathlib import Path
import re
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[1]
PYTHON_PAGES = ('README.md', 'docs/guides/PYTHON.md', 'docs/reference/MATRIX_API.md',
                'docs/reference/LINEAR_SYSTEM_API.md', 'docs/reference/STATISTICS_API.md',
                'bindings/python/README.md')
C_PAGES = (('docs/guides/LIBRARY_GUIDE.md', 'NumForge::numforge', '1[.]4142135624'),
           ('docs/reference/ENGINE_API.md', 'NumForge::engine', 'exact = no'))


def blocks(page, language):
    return re.findall(r'^```' + language + r'\s*\n(.*?)^```',
                      (ROOT / page).read_text(encoding='utf-8'), re.M | re.S)


def python_examples(site):
    sys.path.insert(0, str(site))
    import numforge
    from numforge import _native
    for module in (numforge, _native):
        if not Path(module.__file__).resolve().is_relative_to(site):
            raise RuntimeError('Documentation examples must use the requested installed package')
    count = 0
    for page in PYTHON_PAGES:
        namespace = {'__name__': '__main__'}
        for block in blocks(page, 'python'):
            with redirect_stdout(StringIO()):
                exec(compile(block, page, 'exec'), namespace)
            count += 1
    print(f'{count} Python documentation examples passed against {site}')


def c_examples(prefix, output):
    output.mkdir(parents=True, exist_ok=False)
    source = output / 'source'
    source.mkdir()
    cmake = ['cmake_minimum_required(VERSION 3.20)', 'project(DocumentationExamples LANGUAGES C)',
             'set(CMAKE_C_STANDARD 17)', 'set(CMAKE_C_STANDARD_REQUIRED ON)',
             'find_package(NumForge CONFIG REQUIRED)', 'enable_testing()']
    for index, (page, target, expected) in enumerate(C_PAGES):
        programs = [block for block in blocks(page, 'c') if re.search(r'\bint\s+main\s*\(', block)]
        if len(programs) != 1:
            raise RuntimeError(f'{page}: expected one maintained complete C example')
        name = f'doc_example_{index}'
        (source / (name + '.c')).write_text(programs[0], encoding='utf-8')
        cmake += [f'add_executable({name} {name}.c)', f'target_link_libraries({name} PRIVATE {target})',
                  f'add_test(NAME {name} COMMAND {name})',
                  f'set_tests_properties({name} PROPERTIES PASS_REGULAR_EXPRESSION "{expected}")',
                  f'if(MSVC)\n  target_compile_options({name} PRIVATE /W4 /WX /utf-8)\nelse()\n'
                  f'  target_compile_options({name} PRIVATE -Wall -Wextra -Wpedantic -Werror)\nendif()']
    (source / 'CMakeLists.txt').write_text('\n'.join(cmake) + '\n', encoding='utf-8')
    build = output / 'build'
    for command in ([ 'cmake', '-S', str(source), '-B', str(build), '-DCMAKE_BUILD_TYPE=Release',
                      '-DCMAKE_PREFIX_PATH=' + str(prefix)],
                    ['cmake', '--build', str(build), '--config', 'Release', '--parallel', '2'],
                    ['ctest', '--test-dir', str(build), '-C', 'Release', '--output-on-failure']):
        subprocess.run(command, check=True)
    print(f'{len(C_PAGES)} C documentation examples compiled and passed against {prefix}')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--python-site', type=Path, help='Isolated installed Python package directory')
    parser.add_argument('--prefix', type=Path, help='Installed C SDK prefix, including NumForge::engine')
    parser.add_argument('--output', type=Path, help='Fresh directory for C example source/build outputs')
    args = parser.parse_args()
    if not args.python_site and not args.prefix:
        parser.error('Supply --python-site and/or --prefix')
    if args.prefix and not args.output:
        parser.error('--prefix requires a fresh --output directory')
    if args.python_site:
        python_examples(args.python_site.resolve())
    if args.prefix:
        c_examples(args.prefix.resolve(), args.output.resolve())


if __name__ == '__main__':
    main()
