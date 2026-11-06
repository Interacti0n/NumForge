"""Stage an unmodified, hash-pinned upstream PyJWT wheel without running pip."""
import hashlib
import io
import json
from pathlib import Path, PurePosixPath
import stat
import urllib.request
import zipfile


def stage(data, manifest, output):
    if len(data) != manifest['size'] or hashlib.sha256(data).hexdigest() != manifest['sha256']:
        raise ValueError('PyJWT archive integrity mismatch')
    with zipfile.ZipFile(io.BytesIO(data)) as archive:
        members = archive.infolist()
        if len(members) > 128 or sum(m.file_size for m in members) > 2 * 1024 * 1024:
            raise ValueError('PyJWT archive exceeds extraction limits')
        names = set()
        for member in members:
            path = PurePosixPath(member.filename)
            if path.is_absolute() or '..' in path.parts or '\\' in member.orig_filename or ':' in member.filename \
                    or not path.parts or path.parts[0] not in ('jwt', 'pyjwt-' + manifest['version'] + '.dist-info') \
                    or stat.S_ISLNK(member.external_attr >> 16) or member.filename in names:
                raise ValueError('Unsafe PyJWT archive member')
            names.add(member.filename)
        if output.exists():
            raise ValueError('PyJWT staging destination must not exist')
        output.mkdir(parents=True)
        for member in members:
            target = output.joinpath(*PurePosixPath(member.filename).parts)
            if member.is_dir():
                target.mkdir(parents=True, exist_ok=True)
            else:
                target.parent.mkdir(parents=True, exist_ok=True)
                target.write_bytes(archive.read(member))
        files = {p.relative_to(output).as_posix(): hashlib.sha256(p.read_bytes()).hexdigest()
                 for p in output.rglob('*') if p.is_file()}
        if 'jwt/__init__.py' not in files or 'pyjwt-' + manifest['version'] + '.dist-info/METADATA' not in files:
            raise ValueError('PyJWT module or metadata missing')
        return {'upstream': manifest, 'installation': '/usr/local/lib/python3.11/dist-packages', 'files': files,
                'scope': 'Upstream default import; Debian package and its historical redirect repair retained unchanged.'}


if __name__ == '__main__':
    manifest = json.loads(Path('/build/monitor-jwt.json').read_text())
    with urllib.request.urlopen(manifest['url'], timeout=30) as response:
        data = response.read(1024 * 1024 + 1)
    receipt = stage(data, manifest, Path('/out'))
    Path('/receipt.json').write_text(json.dumps(receipt, indent=2), encoding='utf-8')
