"""Capture an encrypted recovery kit; never replace live containers or volumes."""
import argparse
from datetime import datetime, timezone
import hashlib
import json
from pathlib import Path
import shutil
import tarfile
import tempfile

import ops_backup as backup

SERVICES = ("web", "accounts", "db", "gateway", "uptime")


def digest(path):
    with path.open("rb") as source:
        return hashlib.file_digest(source, "sha256").hexdigest()


def verify_files(root, manifest):
    for name, expected in manifest["sha256"].items():
        # A restored manifest may never redirect the verifier outside its kit.
        relative = Path(name)
        if relative.is_absolute() or ".." in relative.parts or "\\" in name or ":" in name:
            raise ValueError("Unsafe recovery manifest path.")
        path = root / relative
        if not path.is_file() or path.is_symlink() or digest(path) != expected:
            raise ValueError("Recovery kit bytes differ: " + name)
    backup.verify_monitor(root / "daily/monitor.sqlite")
    with tarfile.open(root / "gateway-pki.tar") as archive:
        names = {member.name for member in archive.getmembers() if member.isfile()}
        if not {"caddy/pki/authorities/local/root.crt", "caddy/pki/authorities/local/root.key"} <= names:
            raise ValueError("Gateway private CA is missing from recovery kit.")


def capture():
    backup.load_environment()
    settings = backup.config()
    with backup.lock():
        backup.backup()
        daily_status = json.loads((backup.STATE / "backup-status.json").read_text())
        with tempfile.TemporaryDirectory(prefix="recovery-kit-", dir=backup.STATE) as temporary:
            root = Path(temporary)
            shutil.copytree(backup.STATE / "backup-source", root / "daily")
            profile = json.loads((root / "daily/pilot-runtime.json").read_text())
            images = {}
            for service in SERVICES:
                result = backup.execute(["docker", "compose", "ps", "-q", service], cwd=backup.ROOT)
                container = json.loads(backup.execute(["docker", "inspect", result.stdout.decode().strip()]).stdout)[0]
                if container["State"].get("Health", {}).get("Status") != "healthy":
                    raise ValueError("Recovery capture requires healthy service: " + service)
                images[service] = container["Image"]
            with (root / "images.tar").open("xb") as output:
                backup.execute(["docker", "image", "save", *sorted(set(images.values()))], stdout=output)
            with (root / "gateway-pki.tar").open("xb") as output:
                backup.execute(["docker", "compose", "exec", "-T", "gateway", "tar", "-C", "/data", "-cf", "-", "caddy/pki"],
                               cwd=backup.ROOT, stdout=output)
            assets = root / "assets"
            assets.mkdir()
            bindings = {}
            for service, config in profile["services"].items():
                for mount in config.get("volumes", []):
                    if mount["type"] == "bind":
                        original = Path(mount["source"])
                        if not original.is_file():
                            raise ValueError("Recovery capture supports file bind mounts only.")
                        name = service + "-" + original.name
                        shutil.copyfile(original, assets / name)
                        bindings[service + ":" + mount["target"]] = "assets/" + name
            manifest = {"format_version": 1, "captured_at": datetime.now(timezone.utc).isoformat(),
                        "daily_snapshot": daily_status["snapshot"], "images": images, "bindings": bindings,
                        "limits": ["requires external restic password and tools", "requires host path rebinding",
                                   "gateway PKI copied online; service data snapshots are not cross-service atomic",
                                   "monitor SQLite only; monitor attachments are outside daily backup"],
                        "sha256": {p.relative_to(root).as_posix(): digest(p) for p in root.rglob("*") if p.is_file()}}
            (root / "kit.json").write_text(json.dumps(manifest, indent=2), encoding="utf-8")
            verify_files(root, manifest)
            events = backup.restic(settings, "backup", root, "--tag", "numforge-recovery-kit", "--json").decode().splitlines()
            snapshot = next(json.loads(line)["snapshot_id"] for line in reversed(events)
                            if json.loads(line).get("message_type") == "summary")
            with tempfile.TemporaryDirectory(prefix="kit-restore-", dir=backup.STATE) as restored:
                backup.restic(settings, "restore", snapshot, "--target", restored)
                manifests = list(Path(restored).rglob("kit.json"))
                if len(manifests) != 1 or manifests[0].read_bytes() != (root / "kit.json").read_bytes():
                    raise ValueError("Unexpected recovery kit restore layout or manifest.")
                verify_files(manifests[0].parent, manifest)
            backup.restic(settings, "check")
            # No pruning here: maintenance/recovery points remain until reviewed.
            status = {"ok": True, "snapshot": snapshot, "captured_at": manifest["captured_at"],
                      "daily_snapshot": daily_status["snapshot"], "images": images, "byte_restore_verified": True,
                      "clean_host_recovery_verified": False}
            (backup.STATE / "recovery-kit-status.json").write_text(json.dumps(status, indent=2), encoding="utf-8")
            print("Encrypted recovery kit and restored bytes verified:", snapshot)


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("action", choices=("capture", "verify"))
    parser.add_argument("--directory", type=Path)
    args = parser.parse_args()
    if args.action == "capture":
        capture()
    else:
        if not args.directory:
            parser.error("verify requires --directory")
        verify_files(args.directory, json.loads((args.directory / "kit.json").read_text()))
        print("Recovery kit files verified; this does not certify a fresh-host startup.")
