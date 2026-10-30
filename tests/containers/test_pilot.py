"""Host recovery checks without launching Docker, a tunnel or desktop applications."""
import json
import os
from pathlib import Path
import subprocess
import sys
import tempfile
from types import SimpleNamespace
import unittest
from unittest.mock import patch

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "scripts"))
import pilot


class PilotChecks(unittest.TestCase):
    def test_reused_pid_is_not_owned(self):
        entry = {"Id": 42, "Path": "cloudflared.exe", "StartTime": "old"}
        with patch.object(pilot, "identity", return_value=entry | {"StartTime": "new"}):
            self.assertFalse(pilot.owned(entry))
        with patch.object(pilot, "identity", return_value=entry):
            self.assertTrue(pilot.owned(entry))

    def test_live_tunnel_is_preserved(self):
        entry = {"Id": 42, "Url": "https://existing.trycloudflare.com"}
        with patch.object(pilot, "read_json", return_value=entry), patch.object(pilot, "owned", return_value=True), \
             patch.object(pilot.subprocess, "Popen") as spawn:
            self.assertEqual(pilot.tunnel(), entry)
            spawn.assert_not_called()

    def test_origin_update_is_exact_and_keeps_private_settings(self):
        with tempfile.TemporaryDirectory(dir=ROOT / "build") as temporary:
            root = Path(temporary)
            state = root / ".local/compose"
            state.mkdir(parents=True)
            env = state / "pilot.env"
            env.write_text("COMPOSE_PROJECT_NAME=fixture\nNF_ACCOUNT_PUBLIC_ORIGIN=https://old.trycloudflare.com\n")
            monitor = root / ".local/ops/monitor-admin.json"
            monitor.parent.mkdir()
            config = {"password": "fixture-not-a-secret", "monitors": [
                {"url": "https://old.trycloudflare.com/api/evaluate?precision=10", "headers": '{"Origin":"https://old.trycloudflare.com","Content-Type":"text/plain"}'},
                {"url": "http://web:18876/api/evaluate"}]}
            monitor.write_text(json.dumps(config))
            with patch.object(pilot, "ROOT", root), patch.object(pilot, "STATE", state), \
                 patch.object(pilot.compose, "load_pilot_environment"), patch.object(pilot.compose, "docker") as docker:
                for invalid in ("http://new.trycloudflare.com", "https://new.trycloudflare.com/evil", "https://new.trycloudflare.com.evil.example", "https://user@new.trycloudflare.com", "https://*.trycloudflare.com"):
                    with self.assertRaises(ValueError):
                        pilot.sync_origin(invalid)
                docker.assert_not_called()
                self.assertIn("https://old.trycloudflare.com", env.read_text())
                pilot.sync_origin("https://new.trycloudflare.com")
                first = (env.read_bytes(), monitor.read_bytes())
                pilot.sync_origin("https://new.trycloudflare.com")
                self.assertEqual(first, (env.read_bytes(), monitor.read_bytes()))
                _, options = docker.call_args
                self.assertEqual(json.loads(options["input"])["password"], config["password"])
                self.assertEqual(options["encoding"], "utf-8")
            updated = json.loads(monitor.read_text())
            self.assertEqual(updated["password"], config["password"])
            self.assertEqual(updated["monitors"][1], config["monitors"][1])
            self.assertEqual(updated["monitors"][0]["url"], "https://new.trycloudflare.com/api/evaluate?precision=10")
            self.assertEqual(json.loads(updated["monitors"][0]["headers"])["Origin"], "https://new.trycloudflare.com")

    def test_cleanup_is_daily_and_failed_command_is_retried(self):
        with tempfile.TemporaryDirectory(dir=ROOT / "build") as temporary, patch.object(pilot, "STATE", Path(temporary)):
            with patch.object(pilot.compose, "docker", side_effect=RuntimeError("database unavailable")):
                with self.assertRaises(RuntimeError):
                    pilot.cleanup()
            self.assertFalse((pilot.STATE / "pilot-maintenance.json").exists())
            with patch.object(pilot.compose, "docker") as docker:
                pilot.cleanup()
                pilot.cleanup()
                docker.assert_called_once_with("exec", "-T", "accounts", "python", "manage.py", "clearsessions")

    def test_new_tunnel_pins_tls_and_persists_identity(self):
        with tempfile.TemporaryDirectory(dir=ROOT / "build") as temporary, patch.object(pilot, "STATE", Path(temporary)):
            def spawn(args, **kwargs):
                self.assertNotIn("--no-tls-verify", args)
                self.assertIn("--origin-ca-pool", args)
                self.assertIn("localhost", args)
                kwargs["stdout"].write(b"URL: https://replacement.trycloudflare.com\n")
                return SimpleNamespace(pid=42)

            with patch.dict(os.environ, NUMFORGE_GATEWAY_PORT="18878"), \
                 patch.object(pilot, "owned", return_value=False), patch.object(pilot, "identity", return_value={"Id": 42}), \
                 patch.object(pilot.subprocess, "Popen", side_effect=spawn):
                entry = pilot.tunnel()
            self.assertEqual(entry["Url"], "https://replacement.trycloudflare.com")
            self.assertEqual(pilot.read_json(pilot.STATE / "pilot-tunnel.json"), entry)


if __name__ == "__main__":
    (ROOT / "build").mkdir(exist_ok=True)
    unittest.main()
