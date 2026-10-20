"""Build local MkDocs guides and Doxygen declarations without changing sources."""
import argparse
import importlib.util
from pathlib import Path
import re
import shutil
import subprocess
import sys
from urllib.parse import unquote

ROOT = Path(__file__).resolve().parents[1]
EXTRA_PAGES = ('README.md', 'CONTRIBUTING.md', 'CHANGELOG.md', 'tests/README.md',
               'benchmarks/README.md', 'bindings/python/README.md', 'apps/web/assets/README.md')
LINK = re.compile(r'!?\[[^\]\n]*\]\(([^\s)]+)\)')
IMAGE = re.compile(r'<img\b[^>]*\bsrc=["\']([^"\']+)["\']', re.I)


def source_pages(root):
    return sorted(set((root / 'docs').rglob('*.md')) | {root / name for name in EXTRA_PAGES})


def clean_generated(root, directory):
    """Only the three dedicated output children may be recursively replaced."""
    base = root.resolve() / 'build' / 'docs'
    if not base.resolve().is_relative_to(root.resolve()):
        raise ValueError('Documentation output must remain in the checkout')
    if (directory.is_symlink() or directory.resolve().parent != base.resolve()
            or directory.name not in ('content', 'c', 'site')):
        raise ValueError('Refusing to replace a non-documentation output path')
    if directory.exists():
        shutil.rmtree(directory)
    directory.mkdir(parents=True)


def prepare_sources(root, destination):
    """Keep repository-relative links; copy only linked local supporting files."""
    pages = source_pages(root)
    resources = {root / 'LICENSE'}
    for source in pages:
        text = source.read_text(encoding='utf-8')
        for pattern in (LINK, IMAGE):
            for match in pattern.finditer(text):
                url = match[1]
                if re.match(r'^[A-Za-z][A-Za-z0-9+.-]*:', url) or url.startswith('#'):
                    continue
                target = (source.parent / unquote(url.split('#', 1)[0])).resolve()
                if not target.is_relative_to(root.resolve()):
                    raise ValueError(f'{source}: link escapes the checkout: {url}')
                if not target.exists():
                    raise ValueError(f'{source}: missing linked input: {url}')
                if target.is_file() and target.suffix.lower() != '.md':
                    resources.add(target)
                elif target.is_dir():
                    # Repository directory browsing remains a source link.
                    relative = target.relative_to(root).as_posix()
                    text = text.replace('](' + url + ')',
                                        '](https://github.com/Interacti0n/NumForge/tree/main/' + relative + ')')
        output = destination / source.relative_to(root)
        output.parent.mkdir(parents=True, exist_ok=True)
        output.write_text(text, encoding='utf-8', newline='\n')
    for resource in resources:
        output = destination / resource.relative_to(root)
        output.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(resource, output)
    return len(pages)


def build(root, doxygen):
    outputs = root / 'build' / 'docs'
    for name in ('content', 'c', 'site'):
        clean_generated(root, outputs / name)
    subprocess.run([doxygen, str(root / 'Doxyfile')], cwd=root, check=True)
    count = prepare_sources(root, outputs / 'content')
    shutil.copytree(outputs / 'c' / 'html', outputs / 'content' / 'c-api')
    subprocess.run([sys.executable, '-m', 'mkdocs', 'build', '--strict',
                    '--config-file', str(root / 'mkdocs.yml')], cwd=root, check=True)
    print(f'Built {count} Markdown pages and public C declarations: {outputs / "site" / "index.html"}')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--doxygen', default='doxygen', help='Doxygen executable (default: PATH)')
    args = parser.parse_args()
    doxygen = shutil.which(args.doxygen)
    if doxygen is None:
        parser.error('Doxygen is unavailable; install it or supply --doxygen /path/to/doxygen')
    if importlib.util.find_spec('mkdocs') is None:
        parser.error('MkDocs is unavailable; install docs/requirements.txt in a documentation environment')
    build(ROOT, doxygen)


if __name__ == '__main__':
    main()
