"""Host-side safety checks; these do not require a Docker daemon."""
from pathlib import Path
import sys
import tempfile
import unittest
from unittest.mock import patch

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "scripts"))
import compose


class HelperChecks(unittest.TestCase):
    def test_existing_credentials_cannot_be_replaced(self):
        with tempfile.TemporaryDirectory(dir=ROOT / "build") as temporary:
            with patch.object(compose, "STATE", Path(temporary) / "secrets"):
                compose.init()
                before = {path.name: path.read_bytes() for path in compose.STATE.iterdir()}
                with self.assertRaises(ValueError):
                    compose.init()
                self.assertEqual(before, {path.name: path.read_bytes() for path in compose.STATE.iterdir()})

    def test_binary_backup_is_preserved_and_not_overwritten(self):
        content = bytes(range(256)) + b"\x00\r\nPGDMP"

        def write_dump(*arguments, **kwargs):
            kwargs["stdout"].write(content)

        with tempfile.TemporaryDirectory(dir=ROOT / "build") as temporary:
            destination = Path(temporary) / "backup.dump"
            with patch.object(compose, "docker", side_effect=write_dump):
                compose.backup(destination)
                with self.assertRaises(FileExistsError):
                    compose.backup(destination)
            self.assertEqual(destination.read_bytes(), content)

    def test_failed_backup_does_not_leave_a_partial_archive(self):
        def fail_dump(*arguments, **kwargs):
            kwargs["stdout"].write(b"incomplete")
            raise RuntimeError("simulated interrupted backup")

        with tempfile.TemporaryDirectory(dir=ROOT / "build") as temporary:
            destination = Path(temporary) / "backup.dump"
            with patch.object(compose, "docker", side_effect=fail_dump):
                with self.assertRaises(RuntimeError):
                    compose.backup(destination)
            self.assertFalse(destination.exists())


if __name__ == "__main__":
    (ROOT / "build").mkdir(exist_ok=True)
    unittest.main()
