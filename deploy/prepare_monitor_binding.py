"""Verify registry source archives, then prepare the maintained SQLite binding."""
import base64
import hashlib
import io
import json
from pathlib import Path
import tarfile
import urllib.request

ROOT = Path('/out/binding')


def download(url, expected, algorithm='sha512'):
    with urllib.request.urlopen(url, timeout=60) as response:
        data = response.read(16 * 1024 * 1024 + 1)
    if len(data) > 16 * 1024 * 1024:
        raise RuntimeError('Source archive exceeds bound')
    actual = hashlib.new(algorithm, data).digest()
    if algorithm == 'sha512':
        actual = 'sha512-' + base64.b64encode(actual).decode()
    else:
        actual = actual.hex()
    if actual != expected:
        raise RuntimeError('Source archive integrity mismatch')
    return data


def unpack(data, target):
    with tarfile.open(fileobj=io.BytesIO(data), mode='r|gz') as archive:
        total = 0
        for count, entry in enumerate(archive, 1):
            if count > 4096:
                raise RuntimeError('Too many source archive members')
            parts = Path(entry.name).parts
            if not parts or parts[0] != 'package' or '..' in parts or not (entry.isfile() or entry.isdir()):
                raise RuntimeError('Unexpected package archive member')
            total += entry.size
            if total > 32 * 1024 * 1024:
                raise RuntimeError('Expanded source archive exceeds bound')
            entry.name = str(Path(*parts[1:]))
            archive.extract(entry, target, filter='data')


if __name__ == '__main__':
    manifest = json.loads(Path('/build/monitor-binding.json').read_text())
    engine = json.loads(Path('/build/monitor-sqlite.json').read_text())
    unpack(download(manifest['url'], manifest['integrity']), ROOT)
    addon = manifest['addon']
    unpack(download(addon['url'], addon['integrity']), ROOT / 'node_modules/node-addon-api')
    package = json.loads((ROOT / 'package.json').read_text())
    assert package['name'] == manifest['name'] and package['version'] == manifest['version']
    assert json.loads((ROOT / 'node_modules/node-addon-api/package.json').read_text())['version'] == addon['version']
    archive = download(engine['url'], engine['archiveSha3'], 'sha3_256')
    with tarfile.open(fileobj=io.BytesIO(archive), mode='r:gz') as source:
        matches = [p for p in source.getmembers() if p.name == 'sqlite-autoconf-' + engine['archiveVersion'] + '/sqlite3.c']
        assert len(matches) == 1 and matches[0].isfile()
        assert hashlib.sha3_256(source.extractfile(matches[0]).read()).hexdigest() == engine['amalgamationSha3']
    config = ROOT / 'deps/common-sqlite.gypi'
    text = config.read_text()
    assert text.count("'sqlite_version%':'3390400'") == 1
    config.write_text(text.replace("'sqlite_version%':'3390400'", "'sqlite_version%':'" + engine['archiveVersion'] + "'"))
    (ROOT / 'deps/sqlite-autoconf-3390400.tar.gz').unlink()
    (ROOT / ('deps/sqlite-autoconf-' + engine['archiveVersion'] + '.tar.gz')).write_bytes(archive)
    print('Binding, header archive, official SQLite archive and amalgamation identities verified.')
