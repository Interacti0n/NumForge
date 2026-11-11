import json
from pathlib import Path
import sys
import tempfile
import unittest
from unittest.mock import patch

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "scripts"))
from security_inventory import inventory
from scan_images import fixable_findings


class InventoryTests(unittest.TestCase):
    def test_strict_fix_gate_includes_low_medium_and_unknown(self):
        findings = [{"Severity": severity, "FixedVersion": "fixed"} for severity in ("LOW", "MEDIUM", "UNKNOWN")]
        report = {"Results": [{"Vulnerabilities": findings + [{"Severity": "CRITICAL"}]}]}
        self.assertEqual(fixable_findings(report), [])
        self.assertEqual(fixable_findings(report, all_severities=True), findings)

    def test_all_severities_and_manifest_identity_are_preserved(self):
        with tempfile.TemporaryDirectory(dir=ROOT / "build") as temporary:
            path = Path(temporary) / ("sha256_" + "a" * 64 + ".json")
            report = {"Metadata": {"ImageID": "different-config-id", "DiffIDs": ["layer"]},
                "Results": [{"Target": "test", "Type": "debian", "Vulnerabilities": [
                    {"PkgName": "package", "InstalledVersion": "1", "VulnerabilityID": severity,
                     "Severity": severity, "FixedVersion": "2" if severity == "LOW" else ""}
                    for severity in ("CRITICAL", "HIGH", "MEDIUM", "LOW", "UNKNOWN")]}]}
            path.write_text(json.dumps(report))
            with patch("security_inventory.subprocess.check_output", return_value=b'[{"RootFS":{"Layers":["layer"]}}]'):
                summary, rows = inventory([path], verify_images=True)
            self.assertEqual(len(rows), 5)
            self.assertEqual(summary["images"][0]["image"], "sha256:" + "a" * 64)
            self.assertEqual(summary["images"][0]["scanner_image_id"], "different-config-id")
            self.assertEqual(summary["images"][0]["listed_fixes"], {"LOW": 1})
            with patch("security_inventory.subprocess.check_output", return_value=b'[{"RootFS":{"Layers":["other"]}}]'):
                with self.assertRaisesRegex(ValueError, "does not match"):
                    inventory([path], verify_images=True)


if __name__ == "__main__":
    unittest.main()
