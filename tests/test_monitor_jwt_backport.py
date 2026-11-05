"""The security patch must refuse an unknown distribution source unchanged."""
import importlib.util
from pathlib import Path
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location('monitor_jwt_backport', ROOT / 'deploy/backport_monitor_jwt.py')
backport = importlib.util.module_from_spec(spec)
spec.loader.exec_module(backport)


class BackportChecks(unittest.TestCase):
    def test_unknown_source_is_rejected_without_writing_a_receipt(self):
        (ROOT / 'build').mkdir(exist_ok=True)
        with tempfile.TemporaryDirectory(dir=ROOT / 'build') as temporary:
            source = Path(temporary) / 'jwks_client.py'
            receipt = Path(temporary) / 'receipt.json'
            original = b'class PyJWKClient: pass\n'
            source.write_bytes(original)
            with self.assertRaisesRegex(RuntimeError, 'source preimage'):
                backport.patch(source, receipt)
            self.assertEqual(source.read_bytes(), original)
            self.assertFalse(receipt.exists())


if __name__ == '__main__':
    unittest.main()
