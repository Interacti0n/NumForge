"""Apply the upstream no-redirect JWKS fix to the exact Debian PyJWT source."""
import hashlib
import json
from pathlib import Path


def patch(source, receipt):
    before = source.read_bytes()
    digest = hashlib.sha256(before).hexdigest()
    if digest != 'd0b7b197e853626e74551a3452e15962fb43966818fd5d3c67c7ad37121cf5dd':
        raise RuntimeError('Unexpected Debian PyJWT 2.6.0-1+deb12u1 source preimage')
    text = before.decode('utf-8')
    text = text.replace('\n\nclass PyJWKClient:', '''

class _NoRedirectHandler(urllib.request.HTTPRedirectHandler):
    def redirect_request(
        self,
        req: urllib.request.Request,
        fp: Any,
        code: int,
        msg: str,
        headers: Any,
        newurl: str,
    ) -> None:
        return None


class PyJWKClient:''', 1)
    text = text.replace(
        '            with urllib.request.urlopen(self.uri) as response:',
        '            opener = urllib.request.build_opener(_NoRedirectHandler())\n'
        '            with opener.open(self.uri) as response:', 1)
    after = text.encode('utf-8')
    compile(text, str(source), 'exec')
    source.write_bytes(after)
    # Do not retain bytecode generated from the unpatched module.
    for cached in (source.parent / '__pycache__').glob('jwks_client.*.pyc'):
        cached.unlink()
    receipt.parent.mkdir(parents=True, exist_ok=True)
    receipt.write_text(json.dumps({
        'cve': 'CVE-2026-102267',
        'debianPackage': 'python3-jwt=2.6.0-1+deb12u1',
        'upstream': '0a795b8e1f6ef08f634aa7086fc41cc6d5ce3e56',
        'adaptation': 'Reject redirects using the existing 2.6 client API and error/cache handling.',
        'beforeSha256': digest,
        'afterSha256': hashlib.sha256(after).hexdigest(),
    }, indent=2), encoding='utf-8')


if __name__ == '__main__':
    patch(Path('/usr/lib/python3/dist-packages/jwt/jwks_client.py'),
          Path('/opt/numforge/system-jwt-backport.json'))
