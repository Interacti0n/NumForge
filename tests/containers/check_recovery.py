"""Start a restored kit in fresh isolated volumes, with no external network."""
import argparse
import copy
import json
import os
from pathlib import Path
import secrets
import subprocess
import sys
import time

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "scripts"))
import compose
import ops_recovery as recovery


def profile_for_kit(kit, manifest, project):
    if not project.startswith("numforge-test-recovery-"):
        raise ValueError("Recovery rehearsal requires a disposable project name.")
    profile = copy.deepcopy(json.loads((kit / "daily/pilot-runtime.json").read_text()))
    profile["name"] = project
    profile["networks"] = {"default": {"internal": True}}
    profile["volumes"] = {name: {"name": project + "_" + name} for name in profile["volumes"]}
    for secret in profile["secrets"].values():
        # Original host paths are Windows paths even on a Linux recovery host.
        basename = secret["file"].replace("\\", "/").rsplit("/", 1)[-1]
        secret["file"] = str(kit / "daily" / basename)
        if not Path(secret["file"]).is_file():
            raise ValueError("Missing recovery secret.")
    for name, service in profile["services"].items():
        service.pop("build", None)
        service.pop("container_name", None)
        service.pop("ports", None)
        service["restart"] = "no"
        service["image"] = manifest["images"]["accounts" if name == "setup" else name]
        for mount in service.get("volumes", []):
            if mount["type"] == "bind":
                mount["source"] = str(kit / manifest["bindings"][name + ":" + mount["target"]])
    return profile


def rehearse(kit, output):
    kit = kit.resolve()
    manifest = json.loads((kit / "kit.json").read_text())
    recovery.verify_files(kit, manifest)
    project = "numforge-test-recovery-" + secrets.token_hex(6)
    output.mkdir(parents=True, exist_ok=False)
    compose.private_directory(output)
    profile = profile_for_kit(kit, manifest, project)
    config = output / "compose.json"
    config.write_text(json.dumps(profile, indent=2), encoding="utf-8")
    # Explicit file/project override every inherited live Compose setting.
    command = ["docker", "compose", "--project-name", project, "--file", str(config)]
    environment = {k: v for k, v in os.environ.items() if not k.startswith("COMPOSE_")}

    def run(*args, **kwargs):
        return recovery.backup.execute([*command, *args], env=environment, **kwargs).stdout

    def wait(service):
        for _ in range(120):
            identity = run("ps", "-q", service).decode().strip()
            if identity:
                state = json.loads(recovery.backup.execute(["docker", "inspect", identity]).stdout)[0]["State"]
                if state.get("Health", {}).get("Status") == "healthy":
                    return
                if not state["Running"]:
                    raise RuntimeError("Restored service stopped: " + service)
            time.sleep(1)
        raise TimeoutError("Restored service did not become healthy: " + service)

    result = {"project": project, "ok": False, "resources_absent": False,
              "scope": "same-engine isolated recovery, not a clean Windows host", "images": manifest["images"]}
    try:
        # Never create resources if a required immutable image is unavailable.
        for image in set(manifest["images"].values()):
            recovery.backup.execute(["docker", "image", "inspect", image])
        with (kit / "gateway-pki.tar").open("rb") as archive:
            run("run", "--rm", "--no-deps", "-T", "--entrypoint", "tar", "gateway", "-C", "/data", "-xf", "-", stdin=archive)
        with (kit / "daily/monitor.sqlite").open("rb") as database:
            run("run", "--rm", "--no-deps", "-T", "--user", "0:0", "--entrypoint", "sh", "uptime", "-c",
                "cat > /app/data/kuma.db && chown 1000:1000 /app/data/kuma.db", stdin=database)
        run("up", "--detach", "--no-build", "--pull", "never", "db")
        wait("db")
        run("run", "--rm", "--no-deps", "-T", "--entrypoint", "python", "setup", "-c",
            "import sys;sys.path.insert(0,'/support');import accounts_db as d;"
            "d.provision({n:d.read_config('/run/secrets/'+n) for n in ('admin','migrate','app')})")
        run("exec", "-T", "db", "psql", "-U", "nf_local_admin", "-d", "postgres", "-v", "ON_ERROR_STOP=1",
            "-c", "CREATE ROLE nf_inspect NOLOGIN NOSUPERUSER NOCREATEDB NOCREATEROLE NOREPLICATION;")
        with (kit / "daily/accounts.dump").open("rb") as archive:
            run("exec", "-T", "db", "pg_restore", "-U", "nf_local_admin", "-d", "numforge_accounts",
                "--single-transaction", "--exit-on-error", stdin=archive)
        run("up", "--detach", "--no-build", "--pull", "never")
        for service in recovery.SERVICES:
            wait(service)
        result["account_tables"] = int(run("exec", "-T", "db", "psql", "-U", "nf_local_admin", "-d", "numforge_accounts",
            "-At", "-c", "SELECT count(*) FROM information_schema.tables WHERE table_schema='public'").decode().strip())
        restored_monitor = run("exec", "-T", "uptime", "node", "-e",
            "const S=require('@louislam/sqlite3');const d=new S.Database('data/kuma.db',S.OPEN_READONLY);"
            "d.get('PRAGMA integrity_check',(e,r)=>{if(e||Object.values(r)[0]!=='ok')process.exitCode=1;d.close();});")
        ca = run("exec", "-T", "gateway", "cat", "/data/caddy/pki/authorities/local/root.crt")
        import tarfile
        with tarfile.open(kit / "gateway-pki.tar") as archive:
            if ca != archive.extractfile("caddy/pki/authorities/local/root.crt").read():
                raise ValueError("Recovery changed the gateway CA.")
        run("exec", "-T", "gateway", "wget", "-q", "--no-check-certificate", "-O", "/dev/null", "https://localhost:8443/")
        result.update(ok=True, five_healthy=True, monitor_integrity=True, gateway_ca_preserved=True)
    finally:
        # Down reads only the generated disposable profile; it has no external
        # volumes, host data-directory mounts, live names or published ports.
        run("down", "--volumes", "--remove-orphans", "--timeout", "10")
        for resource, arguments in (("containers", ["ps", "-a", "-q"]), ("volumes", ["volume", "ls", "-q"]),
                                    ("networks", ["network", "ls", "-q"])):
            if recovery.backup.execute(["docker", *arguments, "--filter", "label=com.docker.compose.project=" + project]).stdout.strip():
                raise RuntimeError("Recovery fixture left " + resource)
        result["resources_absent"] = True
        (output / "result.json").write_text(json.dumps(result, indent=2), encoding="utf-8")
    print("Isolated restored stack verified; disposable resources removed.")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--kit", required=True, type=Path)
    parser.add_argument("--output", required=True, type=Path)
    args = parser.parse_args()
    rehearse(args.kit, args.output)
