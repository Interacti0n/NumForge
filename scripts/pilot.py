"""Windows account-pilot recovery, pinned origin TLS and daily expired-session cleanup."""
import argparse
from datetime import datetime, timezone
import json
import os
from pathlib import Path
import re
import ssl
import subprocess
import time
from urllib.parse import urlsplit
import urllib.request

import compose

ROOT = compose.ROOT
STATE = compose.STATE
QUICK_ORIGIN = re.compile(r"https://[a-z0-9]+(?:-[a-z0-9]+)*\.trycloudflare\.com")


def read_json(path):
    return json.loads(path.read_text(encoding="utf-8-sig")) if path.exists() else {}


def save_json(path, data):
    temporary = path.with_suffix(".pending.json")
    temporary.write_text(json.dumps(data, indent=2), encoding="utf-8")
    temporary.replace(path)


def execute(args, **options):
    return subprocess.run([str(a) for a in args], check=True, capture_output=True,
        creationflags=subprocess.CREATE_NO_WINDOW if os.name == "nt" else 0, timeout=20, **options)


def identity(pid):
    # Path plus creation time protects against stale/reused PIDs. Never scan/kill by name.
    shell = Path(os.environ["SystemRoot"]) / "System32/WindowsPowerShell/v1.0/powershell.exe"
    script = f"$p=Get-Process -Id {int(pid)} -ErrorAction SilentlyContinue; if($p){{@{{Id=$p.Id;Path=$p.Path;StartTime=$p.StartTime.ToUniversalTime().ToString('o')}}|ConvertTo-Json -Compress}}; exit 0"
    return json.loads(execute([shell, "-NoProfile", "-NonInteractive", "-Command", script]).stdout or b"null")


def owned(entry):
    current = identity(entry["Id"]) if entry.get("Id") else None
    return bool(current and all(current.get(key) == entry.get(key) for key in ("Id", "Path", "StartTime")))


def check(base, context=None):
    opener = urllib.request.build_opener(urllib.request.ProxyHandler({}), urllib.request.HTTPSHandler(context=context))
    request = urllib.request.Request(base + "/api/evaluate?precision=10", data=b"2+2",
        headers={"Origin": base, "Content-Type": "text/plain"})
    with opener.open(request, timeout=10) as response:
        if json.load(response).get("result") != "4":
            raise ValueError("Pilot calculation failed")
    with opener.open(base + "/api/account/session", timeout=10) as response:
        if not json.load(response).get("csrf_token"):
            raise ValueError("Pilot session API failed")


def sync_origin(origin):
    if not QUICK_ORIGIN.fullmatch(origin):
        raise ValueError("Use one exact HTTPS Quick Tunnel origin")
    path = STATE / "pilot.env"
    lines = path.read_text(encoding="utf-8-sig").splitlines()
    updated = ["NF_ACCOUNT_PUBLIC_ORIGIN=" + origin if line.startswith("NF_ACCOUNT_PUBLIC_ORIGIN=") else line for line in lines]
    if not any(line.startswith("NF_ACCOUNT_PUBLIC_ORIGIN=") for line in lines):
        updated.append("NF_ACCOUNT_PUBLIC_ORIGIN=" + origin)
    temporary = path.with_suffix(".pending.env")
    temporary.write_text("\n".join(updated) + "\n", encoding="utf-8")
    temporary.replace(path)
    compose.load_pilot_environment()
    # No build or migration: keep the deployed pilot image and its data.
    compose.docker("up", "--no-build", "--no-deps", "--wait", "--wait-timeout", "90", "accounts", "gateway")
    monitor_path = ROOT / ".local/ops/monitor-admin.json"
    if monitor_path.exists():
        settings = read_json(monitor_path)
        for monitor in settings["monitors"]:
            url = urlsplit(monitor.get("url", ""))
            if QUICK_ORIGIN.fullmatch(f"{url.scheme}://{url.netloc}"):
                monitor["url"] = origin + monitor["url"][len(f"{url.scheme}://{url.netloc}"):]
                headers = json.loads(monitor.get("headers") or "{}")
                if "Origin" in headers:
                    headers["Origin"] = origin
                    monitor["headers"] = json.dumps(headers)
        monitor_path.write_text(json.dumps(settings, indent=2), encoding="utf-8")
        compose.docker("up", "--no-build", "--no-deps", "--wait", "uptime")
        compose.docker("exec", "-T", "uptime", "node", "/opt/setup_monitor.cjs",
                       input=json.dumps(settings), encoding="utf-8")


def cleanup():
    path = STATE / "pilot-maintenance.json"
    today = datetime.now(timezone.utc).date().isoformat()
    if read_json(path).get("last_session_cleanup") != today:
        compose.docker("exec", "-T", "accounts", "python", "manage.py", "clearsessions")
        save_json(path, {"last_session_cleanup": today})


def tunnel():
    path = STATE / "pilot-tunnel.json"
    entry = read_json(path)
    if owned(entry) and entry.get("Url"):
        return entry
    executable = ROOT / ".local/tools/bin/cloudflared.exe"
    log = STATE / "pilot-cloudflared.log"
    if not owned(entry):
        with log.open("wb") as output:
            process = subprocess.Popen([str(executable), "tunnel", "--no-autoupdate", "--url",
                "https://localhost:" + os.environ["NUMFORGE_GATEWAY_PORT"], "--http-host-header", "localhost",
                "--origin-server-name", "localhost", "--origin-ca-pool", str(STATE / "gateway-root.crt")],
                cwd=ROOT, stdin=subprocess.DEVNULL, stdout=output, stderr=subprocess.STDOUT,
                creationflags=subprocess.CREATE_NO_WINDOW if os.name == "nt" else 0)
        entry = identity(process.pid)
        if not entry:
            raise RuntimeError("cloudflared exited during startup")
        entry.update(Origin="https://localhost:" + os.environ["NUMFORGE_GATEWAY_PORT"])
        save_json(path, entry)
    # Persist identity before waiting: an interrupted check can adopt its own process.
    deadline = time.monotonic() + 45
    while time.monotonic() < deadline:
        matches = re.findall(r"https://[a-z0-9]+(?:-[a-z0-9]+)*\.trycloudflare\.com", log.read_text(encoding="utf-8", errors="replace"))
        if matches:
            entry["Url"] = matches[-1]
            save_json(path, entry)
            return entry
        time.sleep(1)
    raise RuntimeError("Quick Tunnel URL not ready; recovery will retry")


def ensure():
    compose.load_pilot_environment()
    try:
        execute(["docker", "info", "--format", "{{.ServerVersion}}"])
    except (subprocess.CalledProcessError, subprocess.TimeoutExpired):
        execute(["docker", "desktop", "start", "--detach"])
        raise RuntimeError("Docker is starting; recovery will retry")
    local = "https://localhost:" + os.environ["NUMFORGE_GATEWAY_PORT"]
    certificate = STATE / "gateway-root.crt"
    try:
        check(local, ssl.create_default_context(cafile=str(certificate)))
    except (OSError, ValueError):
        compose.docker("up", "--no-build", "--wait", "--wait-timeout", "90")
        compose.docker("cp", "gateway:/data/caddy/pki/authorities/local/root.crt", str(certificate))
        check(local, ssl.create_default_context(cafile=str(certificate)))
    entry = tunnel()
    status_path = STATE / "pilot-status.json"
    if read_json(status_path).get("configured_origin") != entry["Url"]:
        sync_origin(entry["Url"])
    check(entry["Url"])
    cleanup()
    save_json(status_path, {"ok": True, "url": entry["Url"], "configured_origin": entry["Url"],
        "last_checked": datetime.now(timezone.utc).isoformat()})
    (STATE / "pilot-url.txt").write_text(entry["Url"] + "\n", encoding="utf-8")
    print("Pilot verified:", entry["Url"])


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("action", choices=("ensure", "status"))
    args = parser.parse_args()
    if args.action == "status":
        print(json.dumps(read_json(STATE / "pilot-status.json"), indent=2))
    elif os.name != "nt":
        parser.error("Automatic desktop/tunnel recovery is a Windows host helper")
    else:
        # The PowerShell entry point owns an exclusive file lock for the full run.
        try:
            ensure()
        except Exception as error:
            path = STATE / "pilot-status.json"
            status = read_json(path)
            status.update(ok=False, last_checked=datetime.now(timezone.utc).isoformat(), error=type(error).__name__)
            save_json(path, status)
            raise
