"""Stage pinned upstream XML parser and a private, complete JS dependency closure."""
import base64
import hashlib
import io
import json
from pathlib import Path, PurePosixPath
import tarfile
import urllib.request


def extract(data, integrity, target):
    if 'sha512-' + base64.b64encode(hashlib.sha512(data).digest()).decode() != integrity:
        raise ValueError('XML package archive integrity mismatch')
    with tarfile.open(fileobj=io.BytesIO(data), mode='r|gz') as archive:
        count = size = 0
        for member in archive:
            count += 1
            size += member.size
            relative = PurePosixPath(member.name)
            if count > 2048 or size > 16 * 1024 * 1024 or member.size > 8 * 1024 * 1024:
                raise ValueError('XML package extraction limit exceeded')
            if not relative.parts or relative.parts[0] != 'package' or '..' in relative.parts \
                    or '\\' in member.name or ':' in member.name or not (member.isdir() or member.isfile()):
                raise ValueError('Unsafe XML package archive member')
            path = target.joinpath(*relative.parts[1:])
            if member.isdir():
                path.mkdir(parents=True, exist_ok=True)
            else:
                path.parent.mkdir(parents=True, exist_ok=True)
                with path.open('xb') as output:
                    output.write(archive.extractfile(member).read())
                # npm archives may carry owner-only author permissions. Match
                # package-manager installation: readable by the runtime UID,
                # executable only when the archive marks a file executable.
                path.chmod(0o755 if member.mode & 0o111 else 0o644)


if __name__ == '__main__':
    manifest = json.loads(Path('/build/monitor-xml.json').read_text())
    for package in manifest['packages']:
        with urllib.request.urlopen(package['url'], timeout=30) as response:
            data = response.read(4 * 1024 * 1024 + 1)
        if len(data) > 4 * 1024 * 1024:
            raise ValueError('XML package download limit exceeded')
        target = Path('/out') if package['name'] == 'fast-xml-parser' else Path('/out/node_modules') / package['name']
        extract(data, package['integrity'], target)
        installed = json.loads((target / 'package.json').read_text())
        if installed['name'] != package['name'] or installed['version'] != package['version'] \
                or installed.get('dependencies', {}) != package['dependencies']:
            raise ValueError('Unexpected XML package identity or dependencies')
