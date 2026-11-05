"""Supply-chain guards operate on independently constructed archive bytes."""
import importlib.util
import io
import gzip
from pathlib import Path
import tarfile
import tempfile
import unittest
from unittest.mock import patch

ROOT = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location('binding_source', ROOT / 'deploy/prepare_monitor_binding.py')
source = importlib.util.module_from_spec(spec)
spec.loader.exec_module(source)


def archive(name, link=None):
    output = io.BytesIO()
    with tarfile.open(fileobj=output, mode='w:gz') as tar:
        entry = tarfile.TarInfo(name)
        if link:
            entry.type = tarfile.SYMTYPE
            entry.linkname = link
            tar.addfile(entry)
        else:
            entry.size = 7
            tar.addfile(entry, io.BytesIO(b'fixture'))
    return output.getvalue()


class Guards(unittest.TestCase):
    def test_bad_registry_integrity_is_rejected(self):
        with patch.object(source.urllib.request, 'urlopen', return_value=io.BytesIO(b'tampered')):
            with self.assertRaisesRegex(RuntimeError, 'integrity mismatch'):
                source.download('https://registry.npmjs.org/fixture', 'sha512-invalid')

    def test_valid_regular_file_is_extracted_under_target(self):
        (ROOT / 'build').mkdir(exist_ok=True)
        with tempfile.TemporaryDirectory(dir=ROOT / 'build') as temporary:
            source.unpack(archive('package/src/fixture.txt'), Path(temporary))
            self.assertEqual((Path(temporary) / 'src/fixture.txt').read_bytes(), b'fixture')

    def test_parent_traversal_is_rejected(self):
        with self.assertRaisesRegex(RuntimeError, 'Unexpected package archive member'):
            source.unpack(archive('package/../../outside'), ROOT / 'build')

    def test_archive_symlink_is_rejected(self):
        with self.assertRaisesRegex(RuntimeError, 'Unexpected package archive member'):
            source.unpack(archive('package/escape', '../../outside'), ROOT / 'build')

    def test_oversized_member_is_rejected_before_reading_its_body(self):
        entry = tarfile.TarInfo('package/large')
        entry.size = 32 * 1024 * 1024 + 1
        data = gzip.compress(entry.tobuf() + bytes(1024))
        with self.assertRaisesRegex(RuntimeError, 'Expanded source archive exceeds bound'):
            source.unpack(data, ROOT / 'build')


if __name__ == '__main__':
    unittest.main()
