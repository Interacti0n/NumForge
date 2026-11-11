import base64
import gzip
import hashlib
import importlib.util
import io
import os
from pathlib import Path
import tarfile
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location('xml_source', ROOT / 'deploy/prepare_monitor_xml.py')
source = importlib.util.module_from_spec(spec)
spec.loader.exec_module(source)


def integrity(data):
    return 'sha512-' + base64.b64encode(hashlib.sha512(data).digest()).decode()


def archive(name, content=b'fixture', mode=0o600, link=False):
    output = io.BytesIO()
    with tarfile.open(fileobj=output, mode='w:gz') as stream:
        member = tarfile.TarInfo(name)
        member.mode = mode
        if link:
            member.type = tarfile.SYMTYPE
            member.linkname = '../../outside'
            stream.addfile(member)
        else:
            member.size = len(content)
            stream.addfile(member, io.BytesIO(content))
    return output.getvalue()


class XmlSourceChecks(unittest.TestCase):
    def test_integrity_is_checked_before_writing(self):
        data = archive('package/source.js')
        with tempfile.TemporaryDirectory(dir=ROOT / 'build') as temporary:
            target = Path(temporary) / 'out'
            with self.assertRaisesRegex(ValueError, 'integrity'):
                source.extract(data, integrity(data + b'x'), target)
            self.assertFalse(target.exists())

    def test_source_bytes_are_preserved_and_author_modes_normalized(self):
        for mode, expected in ((0o600, 0o644), (0o700, 0o755)):
            with self.subTest(mode=mode), tempfile.TemporaryDirectory(dir=ROOT / 'build') as temporary:
                target = Path(temporary) / 'out'
                data = archive('package/source.js', mode=mode)
                source.extract(data, integrity(data), target)
                self.assertEqual((target / 'source.js').read_bytes(), b'fixture')
                # Windows does not expose POSIX execute bits; the actual image
                # provenance test verifies exact installed modes as UID 1000.
                if os.name != 'nt':
                    self.assertEqual((target / 'source.js').stat().st_mode & 0o777, expected)

    def test_traversal_links_and_absolute_members_are_rejected(self):
        for name, link in (('package/../../outside', False), ('/outside', False), ('package/link', True)):
            with self.subTest(name=name), tempfile.TemporaryDirectory(dir=ROOT / 'build') as temporary:
                data = archive(name, link=link)
                with self.assertRaisesRegex(ValueError, 'Unsafe'):
                    source.extract(data, integrity(data), Path(temporary) / 'out')
                self.assertFalse((Path(temporary) / 'outside').exists())

    def test_oversized_member_is_rejected_before_reading_its_body(self):
        member = tarfile.TarInfo('package/oversized')
        member.size = 8 * 1024 * 1024 + 1
        data = gzip.compress(member.tobuf() + b'\0' * 1024)
        with tempfile.TemporaryDirectory(dir=ROOT / 'build') as temporary:
            with self.assertRaisesRegex(ValueError, 'limit'):
                source.extract(data, integrity(data), Path(temporary) / 'out')


if __name__ == '__main__':
    (ROOT / 'build').mkdir(exist_ok=True)
    unittest.main()
