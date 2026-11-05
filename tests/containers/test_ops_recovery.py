"""Recovery fixtures must not reuse pilot volumes, ports, builds or host paths."""
import json
from pathlib import Path
import sys
import tempfile
import unittest
from unittest.mock import patch

sys.path.insert(0, str(Path(__file__).resolve().parents[2] / "scripts"))
import ops_recovery as recovery
from check_recovery import profile_for_kit


class RecoverySafety(unittest.TestCase):
    def test_rebinding_removes_live_resource_names_ports_and_build_paths(self):
        with tempfile.TemporaryDirectory(dir=recovery.backup.ROOT / "build") as temporary:
            kit = Path(temporary)
            (kit / "daily").mkdir()
            (kit / "daily/app.json").touch()
            profile = {"name": "numforge", "networks": {"default": {"name": "numforge_default"}},
                "volumes": {"accounts-data": {"name": "numforge_accounts-data", "external": True}},
                "secrets": {"app": {"file": "C:\\old\\app.json"}},
                "services": {"accounts": {"image": "mutable", "build": "old/source", "container_name": "numforge-accounts-1",
                    "ports": ["55433:5432"], "volumes": [{"type": "bind", "source": "C:\\old\\script.py", "target": "/support/script.py"}]}}}
            (kit / "daily/pilot-runtime.json").write_text(json.dumps(profile))
            manifest = {"images": {"accounts": "sha256:fixture"}, "bindings": {"accounts:/support/script.py": "assets/script.py"}}
            result = profile_for_kit(kit, manifest, "numforge-test-recovery-fixture")
            self.assertEqual(result["volumes"]["accounts-data"], {"name": "numforge-test-recovery-fixture_accounts-data"})
            self.assertEqual(result["networks"], {"default": {"internal": True}})
            service = result["services"]["accounts"]
            for field in ("build", "container_name", "ports"):
                self.assertNotIn(field, service)
            self.assertEqual(service["image"], "sha256:fixture")
            self.assertEqual(result["secrets"]["app"]["file"], str(kit / "daily/app.json"))
            self.assertEqual(service["volumes"][0]["source"], str(kit / "assets/script.py"))
            with self.assertRaises(ValueError):
                profile_for_kit(kit, manifest, "numforge")

    def test_unsafe_manifest_paths_are_rejected_before_access(self):
        for name in ("../outside", "C:/private", "safe\\..\\outside", "/absolute"):
            with self.subTest(name=name), patch.object(recovery, "digest") as digest:
                with self.assertRaises(ValueError):
                    recovery.verify_files(Path("unused"), {"sha256": {name: "ignored"}})
                digest.assert_not_called()

    def test_missing_or_corrupted_files_fail_before_database_verification(self):
        with tempfile.TemporaryDirectory(dir=recovery.backup.ROOT / "build") as temporary:
            root = Path(temporary)
            (root / "images.tar").write_bytes(b"changed")
            for name in ("missing", "images.tar"):
                with patch.object(recovery.backup, "verify_monitor") as verify:
                    with self.assertRaises(ValueError):
                        recovery.verify_files(root, {"sha256": {name: "wrong"}})
                    verify.assert_not_called()


if __name__ == "__main__":
    unittest.main()
