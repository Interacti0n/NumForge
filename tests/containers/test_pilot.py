"""Host recovery checks without launching Docker, a tunnel or desktop applications."""
import json
import os
from pathlib import Path
import subprocess
import sys
import tempfile
from types import SimpleNamespace
import unittest
from urllib.error import URLError
from unittest.mock import patch

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "scripts"))
import pilot


class PilotChecks(unittest.TestCase):
    def test_failure_diagnostics_survive_later_success_status(self):
        with tempfile.TemporaryDirectory(dir=ROOT / "build") as temporary, patch.object(pilot, "STATE", Path(temporary)):
            pilot.save_json(pilot.STATE / "pilot-status.json", {"ok": True, "url": "https://old.trycloudflare.com"})
            error = subprocess.CalledProcessError(1, ["docker", "desktop", "start", "--detach"], stderr=b"backend failed")
            pilot.record_failure(error)
            status = pilot.read_json(pilot.STATE / "pilot-status.json")
            self.assertFalse(status["ok"])
            self.assertEqual(status["url"], "https://old.trycloudflare.com")
            failure = pilot.read_json(pilot.STATE / "pilot-last-failure.json")
            self.assertEqual(failure["command"], ["docker", "desktop", "start"])
            self.assertEqual(failure["stderr"], "backend failed")
            pilot.save_json(pilot.STATE / "pilot-status.json", {"ok": True})
            self.assertEqual(pilot.read_json(pilot.STATE / "pilot-last-failure.json"), failure)

    def test_docker_unavailable_requests_background_start_and_leaves_stack_untouched(self):
        with patch.object(pilot.compose, "load_pilot_environment"), \
             patch.object(pilot, "execute", side_effect=[subprocess.CalledProcessError(1, ["docker", "info"]), None]) as execute, \
             patch.object(pilot.compose, "docker") as compose_command, patch.object(pilot, "tunnel") as tunnel:
            with self.assertRaisesRegex(RuntimeError, "Docker is starting"):
                pilot.ensure()
            self.assertEqual(execute.call_args_list[1].args[0], ["docker", "desktop", "start", "--detach"])
            compose_command.assert_not_called()
            tunnel.assert_not_called()

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

    def test_retired_tunnel_requires_our_process_and_current_server_rejection(self):
        entry = {"Id":42,"Path":str(ROOT / ".local/tools/bin/cloudflared.exe"),"StartTime":"fixture",
                 "Url":"https://retired.trycloudflare.com","Origin":"https://localhost:18878"}
        with tempfile.TemporaryDirectory(dir=ROOT / "build") as temporary, \
             patch.object(pilot,"STATE",Path(temporary)), patch.dict(os.environ,NUMFORGE_GATEWAY_PORT="18878",SystemRoot="C:/Windows"), \
             patch.object(pilot,"owned",return_value=True) as owned, patch.object(pilot,"execute") as execute:
            log = pilot.STATE / "pilot-cloudflared.log"
            self.assertFalse(pilot.retire_missing_tunnel(entry))
            for text in ("", "connection timeout", "Unauthorized: Tunnel not found\nRegistered tunnel connection"):
                log.write_text(text)
                self.assertFalse(pilot.retire_missing_tunnel(entry))
            log.write_text("Registered tunnel connection\nUnauthorized: Tunnel not found")
            for change in ({"Path":"unrelated.exe"},{"Origin":"https://localhost:9999"},{"Url":"https://foreign.example"}):
                self.assertFalse(pilot.retire_missing_tunnel(entry | change))
            owned.return_value = False
            self.assertFalse(pilot.retire_missing_tunnel(entry))
            execute.assert_not_called()
            owned.return_value = True
            self.assertTrue(pilot.retire_missing_tunnel(entry))
            self.assertIn("StartTime",execute.call_args.args[0][-1])
            self.assertIn("Stop-Process -InputObject",execute.call_args.args[0][-1])

    def test_public_failure_rotates_only_an_explicitly_retired_tunnel(self):
        old,new = {"Url":"https://old.trycloudflare.com"},{"Url":"https://new.trycloudflare.com"}
        for retired in (False,True):
            with self.subTest(retired=retired), tempfile.TemporaryDirectory(dir=ROOT / "build") as temporary, \
                 patch.object(pilot,"STATE",Path(temporary)), patch.dict(os.environ,NUMFORGE_GATEWAY_PORT="18878"), \
                 patch.object(pilot.compose,"load_pilot_environment"), patch.object(pilot,"execute"), \
                 patch.object(pilot.ssl,"create_default_context"), patch.object(pilot,"cleanup"), \
                 patch.object(pilot,"tunnel",side_effect=[old,new]) as tunnel, patch.object(pilot,"sync_origin") as sync, \
                 patch.object(pilot,"retire_missing_tunnel",return_value=retired), \
                 patch.object(pilot,"check",side_effect=[None,URLError("DNS failed"),None]):
                pilot.save_json(pilot.STATE / "pilot-status.json",{"configured_origin":old["Url"]})
                if retired:
                    pilot.ensure()
                    self.assertEqual(tunnel.call_count,2)
                    sync.assert_called_once_with(new["Url"])
                    self.assertEqual(pilot.read_json(pilot.STATE / "pilot-status.json")["url"],new["Url"])
                else:
                    with self.assertRaises(URLError):pilot.ensure()
                    tunnel.assert_called_once();sync.assert_not_called()

    def test_healthy_public_check_never_retires_a_tunnel(self):
        entry = {"Url":"https://healthy.trycloudflare.com"}
        with tempfile.TemporaryDirectory(dir=ROOT / "build") as temporary, \
             patch.object(pilot,"STATE",Path(temporary)), patch.dict(os.environ,NUMFORGE_GATEWAY_PORT="18878"), \
             patch.object(pilot.compose,"load_pilot_environment"), patch.object(pilot,"execute"), \
             patch.object(pilot.ssl,"create_default_context"), patch.object(pilot,"cleanup"), \
             patch.object(pilot,"tunnel",return_value=entry), patch.object(pilot,"check"), \
             patch.object(pilot,"retire_missing_tunnel") as retire:
            pilot.save_json(pilot.STATE / "pilot-status.json",{"configured_origin":entry["Url"]})
            pilot.ensure()
            retire.assert_not_called()


if __name__ == "__main__":
    (ROOT / "build").mkdir(exist_ok=True)
    unittest.main()
