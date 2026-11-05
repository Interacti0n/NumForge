"""Backup failure paths must never replace data or restore over a live database."""
from pathlib import Path
from contextlib import closing
import json
import shutil
import sqlite3
import subprocess
import sys
import tempfile
import unittest
from unittest.mock import patch

sys.path.insert(0, str(Path(__file__).resolve().parents[2] / "scripts"))
import ops_backup as backup


def monitor_fixture(path):
    with closing(sqlite3.connect(path)) as database, database:
        for table in ("user", "monitor", "heartbeat"):
            database.execute('CREATE TABLE "' + table + '" (id INTEGER PRIMARY KEY, value TEXT)')
            database.execute('INSERT INTO "' + table + '" VALUES(1,?)', ("private fixture",))


class BackupSafety(unittest.TestCase):
    def test_restore_failure_prevents_retention(self):
        for failure in ("corrupt-bytes", "database-error"):
            with self.subTest(failure=failure), tempfile.TemporaryDirectory(dir=backup.ROOT / "build") as temporary:
                root = Path(temporary)
                repository = root / "repository"
                repository.mkdir()
                (repository / "config").touch()
                commands = []

                def execute(args):
                    if "bundle" in args:
                        Path(args[-2]).write_bytes(b"git bundle fixture")
                    return subprocess.CompletedProcess(args, 0, stdout=b"fixture-commit\n")

                def restic(settings, *args):
                    commands.append(args[0])
                    if args[0] == "backup":
                        return json.dumps({"message_type": "summary", "snapshot_id": "fixture"}).encode()
                    if args[0] == "restore":
                        restored = Path(args[-1]) / "restored"
                        shutil.copytree(root / "backup-source", restored)
                        if failure == "corrupt-bytes":
                            (restored / "accounts.dump").write_bytes(b"corrupted")
                    return b""

                with patch.object(backup, "ROOT", root), patch.object(backup, "STATE", root), \
                     patch.object(backup, "config", return_value={"repository": str(repository)}), \
                     patch.object(backup, "load_environment"), patch.object(backup, "execute", side_effect=execute), \
                     patch.object(backup, "snapshot_monitor", side_effect=monitor_fixture), \
                     patch.object(backup.compose, "backup", side_effect=lambda path: path.write_bytes(b"valid dump")), \
                     patch.object(backup, "restic", side_effect=restic), \
                     patch.object(backup, "verify_database", side_effect=RuntimeError("restore rejected")):
                    with self.assertRaises((ValueError, RuntimeError)):
                        backup.backup()
                self.assertIn("restore", commands)
                self.assertNotIn("forget", commands)
                self.assertFalse((root / "backup-status.json").exists())

    def test_monitor_restore_failure_prevents_retention_and_success(self):
        with tempfile.TemporaryDirectory(dir=backup.ROOT / "build") as temporary:
            root = Path(temporary)
            (root / "repository").mkdir()
            (root / "repository/config").touch()
            commands = []

            def execute(args):
                if "bundle" in args:
                    Path(args[-2]).write_bytes(b"bundle")
                return subprocess.CompletedProcess(args, 0, stdout=b"commit")

            def restic(settings, *args):
                commands.append(args[0])
                if args[0] == "backup":
                    return b'{"message_type":"summary","snapshot_id":"fixture"}'
                if args[0] == "restore":
                    shutil.copytree(root / "backup-source", Path(args[-1]) / "restored")
                return b""

            original = backup.verify_monitor
            def verify(path):
                if path.parent.name == "restored":
                    raise ValueError("Monitor restore rejected")
                return original(path)

            with patch.object(backup, "ROOT", root), patch.object(backup, "STATE", root), \
                 patch.object(backup, "config", return_value={"repository": str(root / "repository")}), \
                 patch.object(backup, "load_environment"), patch.object(backup, "execute", side_effect=execute), \
                 patch.object(backup.compose, "backup", side_effect=lambda p: p.write_bytes(b"dump")), \
                 patch.object(backup, "snapshot_monitor", side_effect=monitor_fixture), \
                 patch.object(backup, "restic", side_effect=restic), \
                 patch.object(backup, "verify_database"), patch.object(backup, "verify_monitor", side_effect=verify), \
                 patch.object(backup, "notify") as notify:
                with self.assertRaisesRegex(ValueError, "Monitor restore rejected"):
                    backup.backup()
                notify.assert_not_called()
            self.assertNotIn("forget", commands)
            self.assertFalse((root / "backup-status.json").exists())

    def test_monitor_snapshot_cleanup_on_stream_failure(self):
        calls = []
        def execute(args, **kwargs):
            calls.append(args)
            if "readFileSync" in args[-1]:
                raise RuntimeError("stream failed")
        with tempfile.TemporaryDirectory(dir=backup.ROOT / "build") as temporary, \
             patch.object(backup, "execute", side_effect=execute):
            with self.assertRaisesRegex(RuntimeError, "stream failed"):
                backup.snapshot_monitor(Path(temporary) / "monitor.sqlite")
        self.assertIn("VACUUM INTO", calls[0][-1])
        self.assertIn("rmSync", calls[-1][-1])
        remote = calls[-1][-1].split('"')[1]
        self.assertRegex(remote, r"^/tmp/numforge-backup-[0-9a-f]{32}\.sqlite$")
        self.assertIn(remote, calls[0][-1])

    def test_monitor_validation_rejects_wrong_schema_and_corruption(self):
        with tempfile.TemporaryDirectory(dir=backup.ROOT / "build") as temporary:
            path = Path(temporary) / "monitor.sqlite"
            path.write_bytes(b"not a database")
            with self.assertRaises(sqlite3.DatabaseError):
                backup.verify_monitor(path)
            path.unlink()
            with closing(sqlite3.connect(path)) as database, database:
                database.execute("CREATE TABLE unrelated(id INTEGER)")
            with self.assertRaises(sqlite3.DatabaseError):
                backup.verify_monitor(path)
            path.unlink()
            monitor_fixture(path)
            self.assertEqual(backup.verify_monitor(path), {"user": 1, "monitor": 1, "heartbeat": 1})

    def test_nonempty_repository_is_never_initialized(self):
        with tempfile.TemporaryDirectory(dir=backup.ROOT / "build") as temporary:
            root = Path(temporary)
            repository = root / "existing"
            repository.mkdir()
            (repository / "keep").write_bytes(b"existing backup")
            with patch.object(backup, "STATE", root / "state"), patch.object(backup, "restic") as restic:
                with self.assertRaises(ValueError):
                    backup.initialize(repository)
                restic.assert_not_called()
            self.assertEqual((repository / "keep").read_bytes(), b"existing backup")
            self.assertFalse((root / "state/restic.password").exists())

    def test_missing_external_disk_fails_before_dump_or_new_repository(self):
        with tempfile.TemporaryDirectory(dir=backup.ROOT / "build") as temporary:
            repository = Path(temporary) / "unplugged"
            with patch.object(backup, "config", return_value={"repository": str(repository)}), \
                 patch.object(backup.compose, "backup") as dump, patch.object(backup, "restic") as restic:
                with self.assertRaises(ValueError):
                    backup.backup()
                dump.assert_not_called()
                restic.assert_not_called()
            self.assertFalse(repository.exists())

    def test_failed_restore_drops_only_its_disposable_database(self):
        calls = []

        def docker(*args, **kwargs):
            calls.append(args)
            if "pg_restore" in args:
                self.assertEqual(kwargs["stdin"].read(), b"test dump")
                raise RuntimeError("broken dump")

        with tempfile.TemporaryDirectory(dir=backup.ROOT / "build") as temporary:
            path = Path(temporary) / "dump"
            path.write_bytes(b"test dump")
            with patch.object(backup.compose, "docker", side_effect=docker):
                with self.assertRaisesRegex(RuntimeError, "broken dump"):
                    backup.verify_database(path)
        name = calls[0][-1]
        self.assertRegex(name, r"^nf_backup_verify_[0-9a-f]{16}$")
        self.assertEqual(calls[-1], ("exec", "-T", "db", "dropdb", "-U", "nf_local_admin", "--force", name))
        self.assertIn(name, calls[1])


if __name__ == "__main__":
    (backup.ROOT / "build").mkdir(exist_ok=True)
    unittest.main()
