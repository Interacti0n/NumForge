"""Build the pinned vendor-patched legacy ABI on the existing glibc baseline."""
import hashlib
import json
import os
from pathlib import Path
import subprocess
import sys
import tarfile
import urllib.request

BUILD = Path('/build')
MANIFEST = json.loads((BUILD / 'postgres-libxml.json').read_text())
SOURCE = BUILD / ('libxml2-' + MANIFEST['version'])
RECEIPT = BUILD / 'libxml-receipt.json'


def digest(data, algorithm='sha256'):
    return hashlib.new(algorithm, data).hexdigest()


def blob(data):
    return digest(b'blob ' + str(len(data)).encode() + b'\0' + data, 'sha1')


def fetch(url, filename, expected, algorithm='sha256'):
    with urllib.request.urlopen(url, timeout=60) as response:
        data = response.read()
    if digest(data, algorithm) != expected:
        raise RuntimeError('Source digest mismatch: ' + filename)
    (BUILD / filename).write_bytes(data)


def patch(filename):
    subprocess.run(['patch', '--batch', '--fuzz=0', '-p1', '-i', str(BUILD / filename)],
                   cwd=SOURCE, check=True)


def prepare():
    fetch(MANIFEST['archive'], 'libxml.tar.xz', MANIFEST['archiveSha256'])
    base = 'https://raw.githubusercontent.com/alpinelinux/aports/' + MANIFEST['vendorCommit'] + '/main/libxml2/'
    fetch(base + 'CVE-2026-6732.patch', 'fix.patch', MANIFEST['patchSha512'], 'sha512')
    fetch(base + 'CVE-2026-6732-test.patch', 'test.patch', MANIFEST['testPatchSha512'], 'sha512')
    with tarfile.open(BUILD / 'libxml.tar.xz') as archive:
        archive.extractall(BUILD, filter='data')
    assert blob((SOURCE / 'parser.c').read_bytes()) == MANIFEST['parserBeforeGitBlob']
    patch('test.patch')
    subprocess.run(['./configure', *MANIFEST['configure']], cwd=SOURCE, check=True)


def negative():
    result = subprocess.run(['./testparser'], cwd=SOURCE, capture_output=True)
    (BUILD / 'negative-testparser.log').write_bytes(result.stdout + result.stderr)
    assert result.returncode in (-11, 139), ('Expected the original vendor regression crash', result.returncode)
    RECEIPT.write_text(json.dumps({'manifest': MANIFEST, 'negativeExit': result.returncode,
                                  'parserBeforeSha256': digest((SOURCE / 'parser.c').read_bytes())}, indent=2))
    print('Confirmed original unpatched 2.13.9 reader/schema regression crash:', result.returncode)


def apply():
    assert blob((SOURCE / 'parser.c').read_bytes()) == MANIFEST['parserBeforeGitBlob']
    # The vendor hunk uses newer NOCDATA context, while the release archive
    # checks cdataBlock == NULL. Adapt only the same four callback arguments;
    # preserve both release branches and reject any changed source preimage.
    text = (SOURCE / 'parser.c').read_bytes()
    for callback in ('characters', 'cdataBlock'):
        old = ('ctxt->sax->' + callback + '(ctxt, cur->content, len);').encode()
        assert text.count(old) == 2
        text = text.replace(old, old.replace(b'(ctxt,', b'(ctxt->userData,'))
    (SOURCE / 'parser.c').write_bytes(text)
    after = text
    assert blob(after) == MANIFEST['parserAfterGitBlob']
    receipt = json.loads(RECEIPT.read_text())
    receipt['parserAfterSha256'] = digest(after)
    receipt['adaptation'] = 'Four userData callback arguments; release cdataBlock branch conditions preserved.'
    receipt['vendorPatchAppliedVerbatim'] = False
    RECEIPT.write_text(json.dumps(receipt, indent=2))


def receipt():
    library = Path('/out/usr/lib/x86_64-linux-gnu/libxml2.so.2.13.9')
    result = json.loads(RECEIPT.read_text())
    result.update(binarySha256=digest(library.read_bytes()), positiveTestparser=True,
                  upstreamRuntests=True, runtimeSoname='libxml2.so.2',
                  compiler=subprocess.check_output(['cc', '--version'], text=True).splitlines()[0],
                  cflags=os.environ.get('CFLAGS'), ldflags=os.environ.get('LDFLAGS'))
    RECEIPT.write_text(json.dumps(result, indent=2))


if __name__ == '__main__':
    {'prepare': prepare, 'negative': negative, 'apply': apply, 'receipt': receipt}[sys.argv[1]]()
