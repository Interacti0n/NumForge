import hashlib
import importlib.util
import io
from pathlib import Path
import stat
import tempfile
import unittest
import zipfile

ROOT = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location('jwt_source', ROOT / 'deploy/prepare_monitor_jwt.py')
source = importlib.util.module_from_spec(spec)
spec.loader.exec_module(source)


def wheel(extra=None):
    output = io.BytesIO()
    with zipfile.ZipFile(output, 'w') as archive:
        archive.writestr('jwt/__init__.py', '__version__ = "2.15.1"\n')
        archive.writestr('pyjwt-2.15.1.dist-info/METADATA', 'Name: PyJWT\nVersion: 2.15.1\n')
        if extra:
            archive.writestr(*extra)
    data = output.getvalue()
    return data, {'version': '2.15.1', 'size': len(data), 'sha256': hashlib.sha256(data).hexdigest()}


class SourceChecks(unittest.TestCase):
    def test_integrity_failure_writes_nothing(self):
        data, manifest = wheel()
        with tempfile.TemporaryDirectory(dir=ROOT / 'build') as temporary:
            output = Path(temporary) / 'out'
            with self.assertRaisesRegex(ValueError, 'integrity'):
                source.stage(data + b'x', manifest, output)
            self.assertFalse(output.exists())

    def test_valid_wheel_preserves_source_and_metadata(self):
        data, manifest = wheel()
        with tempfile.TemporaryDirectory(dir=ROOT / 'build') as temporary:
            output = Path(temporary) / 'out'
            receipt = source.stage(data, manifest, output)
            self.assertEqual(receipt['upstream'], manifest)
            self.assertEqual(len(receipt['files']), 2)
            self.assertEqual((output / 'jwt/__init__.py').read_bytes(), b'__version__ = "2.15.1"\n')
            with self.assertRaisesRegex(ValueError, 'must not exist'):
                source.stage(data, manifest, output)

    def test_traversal_and_symlinks_are_rejected_before_extraction(self):
        link = zipfile.ZipInfo('jwt/link')
        link.create_system = 3
        link.external_attr = (stat.S_IFLNK | 0o777) << 16
        backslash = zipfile.ZipInfo('placeholder')
        # ZipInfo's constructor normalizes the Windows separator; set the raw
        # member name afterwards to exercise a genuinely malformed archive.
        backslash.filename = 'jwt\\outside'
        for extra in (('../outside', b'x'), ('jwt/../../outside', b'x'), (backslash, b'x'), (link, b'outside')):
            with self.subTest(extra=str(extra[0])), tempfile.TemporaryDirectory(dir=ROOT / 'build') as temporary:
                data, manifest = wheel(extra)
                output = Path(temporary) / 'out'
                with self.assertRaisesRegex(ValueError, 'Unsafe'):
                    source.stage(data, manifest, output)
                self.assertFalse(output.exists())

    def test_expanded_size_limit_is_checked_before_extraction(self):
        data, manifest = wheel(('jwt/oversized', b'x' * (2 * 1024 * 1024)))
        with tempfile.TemporaryDirectory(dir=ROOT / 'build') as temporary:
            output = Path(temporary) / 'out'
            with self.assertRaisesRegex(ValueError, 'limits'):
                source.stage(data, manifest, output)
            self.assertFalse(output.exists())


if __name__ == '__main__':
    (ROOT / 'build').mkdir(exist_ok=True)
    unittest.main()
