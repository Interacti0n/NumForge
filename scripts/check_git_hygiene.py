"""Reject private/generated tracked files and missing literal build inputs."""
import fnmatch
from pathlib import Path, PurePosixPath
import re
import shlex
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[1]
PRIVATE_ROOTS = {'.local', '.local-backups', '.aws', '.codex-remote-attachments', 'info', 'build', 'stage', 'dist'}
GENERATED_PARTS = {'node_modules', '__pycache__', '.venv', 'venv', '.pytest_cache', '.mypy_cache', '.ruff_cache', 'CMakeFiles'}
GENERATED_SUFFIXES = {'.exe', '.o', '.obj', '.pdb', '.ilk', '.dll', '.so', '.dylib', '.a', '.lib', '.pyc', '.pyo'}
ENV_TEMPLATES = {'.env.example', '.env.sample', '.env.template'}
SECRET_PATTERNS = (
    re.compile(rb'(?m)^\s*-----BEGIN (?:RSA |EC |DSA |OPENSSH |ENCRYPTED )?PRIVATE KEY-----\s*$'),
    re.compile(rb'gh[pousr]_[A-Za-z0-9]{36,}'),
    re.compile(rb'AKIA[0-9A-Z]{16}'),
)


def audit(root, tracked):
    tracked = set(tracked)
    errors = []
    for name in sorted(tracked):
        relative = PurePosixPath(name)
        if relative.parts[0] in PRIVATE_ROOTS or relative.parts[0].startswith('build-') \
                or set(relative.parts) & GENERATED_PARTS or relative.suffix.lower() in GENERATED_SUFFIXES \
                or re.search(r'\.so(?:\.\d+)+$', relative.name) \
                or ((relative.name == '.env' or relative.name.startswith('.env.')) and relative.name not in ENV_TEMPLATES) \
                or name in ('AGENTS.md', 'CMakeUserPresets.json'):
            errors.append('Private/generated tracked path: ' + name)
        path = root / name
        if not path.is_file():
            errors.append('Missing tracked file: ' + name)
            continue
        if path.stat().st_size > 5 * 1024 * 1024:
            errors.append('Tracked file exceeds 5 MiB; review its source/fixture purpose: ' + name)
            continue
        data = path.read_bytes()
        if b'\0' not in data and any(pattern.search(data) for pattern in SECRET_PATTERNS):
            # Never print matched credentials or key material.
            errors.append('Possible private key/credential in tracked file: ' + name)

    def present(source):
        source = source.removeprefix('./').rstrip('/')
        return source == '.' or any(fnmatch.fnmatchcase(name, source) or name.startswith(source + '/') for name in tracked)

    for name in sorted(tracked):
        path = root / name
        if not path.is_file() or path.stat().st_size > 5 * 1024 * 1024:
            continue
        if name.lower().endswith('.dockerfile'):
            text = path.read_text(encoding='utf-8').replace('\\\n', ' ')
            for line in text.splitlines():
                if not line.lstrip().upper().startswith('COPY ') or '--from=' in line:
                    continue
                parts = shlex.split(line)
                for source in parts[1:-1]:
                    if source.startswith('--'):
                        continue
                    if not present(source):
                        errors.append('Docker COPY input is not tracked: ' + name + ' -> ' + source)
        if name == 'apps/web/assets/CMakeLists.txt':
            for source in re.findall(r'(?m)^\s*(apps/web/assets/[^\s)]+|LICENSE)\s*$', path.read_text(encoding='utf-8')):
                if source not in tracked:
                    errors.append('Web build asset is not tracked: ' + source)
    return errors


def main():
    tracked = subprocess.check_output(['git', 'ls-files', '-z'], cwd=ROOT).decode('utf-8').split('\0')[:-1]
    errors = audit(ROOT, tracked)
    ignored = subprocess.check_output(['git', 'ls-files', '-ci', '--exclude-standard', '-z'], cwd=ROOT).decode('utf-8').split('\0')[:-1]
    errors.extend('Tracked file also matches ignore rules: ' + name for name in ignored)
    if errors:
        print('\n'.join(errors), file=sys.stderr)
        return 1
    print(f'Git hygiene verified: {len(tracked)} tracked files; private/generated paths absent; literal Docker/web inputs tracked.')
    return 0


if __name__ == '__main__':
    sys.exit(main())
