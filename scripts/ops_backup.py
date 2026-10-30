"""Encrypted pilot backups; no live PostgreSQL data-directory copying."""
import argparse
from contextlib import contextmanager
from datetime import datetime, timezone
import hashlib
import json
import os
from pathlib import Path
import secrets
import shutil
import subprocess
import tempfile
import urllib.request

import compose
from compose import load_pilot_environment as load_environment

ROOT = compose.ROOT
STATE = ROOT / ".local/ops"


def execute(args, **kwargs):
    options = {"creationflags": subprocess.CREATE_NO_WINDOW} if os.name == "nt" else {}
    return subprocess.run([str(a) for a in args], check=True, capture_output=True, **options, **kwargs)


def config():
    return json.loads((STATE / "backup.json").read_text(encoding="utf-8"))


def restic(settings, *args):
    return execute([settings["restic"], "--repo", settings["repository"], "--password-file",
                    settings["password_file"], *args], env=os.environ | {"GOMAXPROCS": "2"}).stdout


@contextmanager
def lock():
    with (STATE / "backup.lock").open("a+b") as handle:
        handle.write(b"\0")
        handle.flush()
        handle.seek(0)
        if os.name == "nt":
            import msvcrt
            msvcrt.locking(handle.fileno(), msvcrt.LK_NBLCK, 1)
        else:
            import fcntl
            fcntl.flock(handle, fcntl.LOCK_EX | fcntl.LOCK_NB)
        try:
            yield
        finally:
            if os.name == "nt":
                handle.seek(0)
                msvcrt.locking(handle.fileno(), msvcrt.LK_UNLCK, 1)


def initialize(repository):
    compose.private_directory(STATE)
    if (STATE / "backup.json").exists():
        raise ValueError("Backup configuration exists; initialization will not replace keys.")
    repository = repository.resolve()
    if repository.exists() and any(repository.iterdir()):
        raise ValueError("Choose a new empty repository; existing data will not be replaced.")
    password = STATE / "restic.password"
    with password.open("x", encoding="utf-8") as output:
        output.write(secrets.token_urlsafe(48))
    password.chmod(0o600)
    settings = {"repository": str(repository), "password_file": str(password),
        "restic": str(ROOT / ".local/tools/bin" / ("restic.exe" if os.name == "nt" else "restic"))}
    restic(settings, "init")
    (STATE / "backup.json").write_text(json.dumps(settings, indent=2), encoding="utf-8")
    print("Encrypted repository initialized. Preserve the recovery password separately.")


def verify_database(dump):
    name = "nf_backup_verify_" + secrets.token_hex(8)
    compose.docker("exec", "-T", "db", "createdb", "-U", "nf_local_admin", "-O", "nf_migrate", name)
    try:
        with dump.open("rb") as archive:
            compose.docker("exec", "-T", "db", "pg_restore", "-U", "nf_local_admin", "-d", name,
                           "--single-transaction", "--exit-on-error", stdin=archive)
        result = compose.docker("exec", "-T", "db", "psql", "-U", "nf_local_admin", "-d", name,
                                "-At", "-v", "ON_ERROR_STOP=1", "-c", "SELECT count(*) FROM django_migrations")
        if int(result.stdout.strip()) == 0:
            raise ValueError("Restored database has no migration ledger.")
    finally:
        compose.docker("exec", "-T", "db", "dropdb", "-U", "nf_local_admin", "--force", name)


def notify(success):
    path = STATE / "monitor-admin.json"
    try:
        if not path.exists():
            return
        token = json.loads(path.read_text())["backupPushToken"]
        with urllib.request.urlopen("http://127.0.0.1:18880/api/push/"+token+"?status="+("up" if success else "down")+"&msg=Backup", timeout=5):
            pass
    except (OSError, ValueError, KeyError):
        print("Local backup monitor could not be updated.")


def backup():
    settings = config()
    # An unplugged external disk is a failure, never a reason to create a new repo.
    if not (Path(settings["repository"]) / "config").is_file():
        raise ValueError("Backup repository is unavailable; attach the configured disk.")
    load_environment()
    source = STATE / "backup-source"
    source.mkdir(exist_ok=True)
    pending = source / "accounts.pending.dump"
    pending.unlink(missing_ok=True)
    compose.backup(pending)
    pending.replace(source / "accounts.dump")
    bundle = source / "source.pending.bundle"
    bundle.unlink(missing_ok=True)
    execute(["git", "-C", ROOT, "bundle", "create", bundle, "--all"])
    bundle.replace(source / "source.bundle")
    credentials = ["admin.json", "migrate.json", "app.json", "admin-password", "gateway-token", "pilot.env", "pilot-build.yaml"]
    for name in credentials:
        path = ROOT / ".local/compose" / name
        if path.is_file():
            shutil.copyfile(path, source / name)
    for name in ("monitor-admin.json", "db-inspect.json", "pgpass.conf"):
        path = STATE / name
        if path.is_file():
            shutil.copyfile(path, source / name)
    commit = execute(["git", "-C", ROOT, "rev-parse", "HEAD"]).stdout.decode().strip()
    (source / "manifest.json").write_text(json.dumps({"source_commit": commit,
        "scope": "logical database dump, committed Git history, private pilot/ops configuration"}, indent=2))
    expected = {p.name: hashlib.sha256(p.read_bytes()).hexdigest() for p in source.iterdir() if p.is_file()}
    events = restic(settings, "backup", source, "--tag", "numforge-pilot", "--json").decode().splitlines()
    summary = next(json.loads(line) for line in reversed(events) if json.loads(line).get("message_type") == "summary")
    snapshot = summary["snapshot_id"]
    with tempfile.TemporaryDirectory(prefix="restore-", dir=STATE) as temporary:
        restic(settings, "restore", snapshot, "--target", temporary)
        roots = list(Path(temporary).rglob("accounts.dump"))
        if len(roots) != 1:
            raise ValueError("Unexpected restore layout.")
        restored = roots[0].parent
        for name, digest in expected.items():
            if hashlib.sha256((restored / name).read_bytes()).hexdigest() != digest:
                raise ValueError("Restored bytes differ: " + name)
        verify_database(roots[0])
    restic(settings, "check")
    # Retention affects only this job's snapshots and follows successful restore.
    restic(settings, "forget", "--tag", "numforge-pilot", "--keep-daily", "7", "--keep-weekly", "4", "--keep-monthly", "6", "--prune")
    (STATE / "backup-status.json").write_text(json.dumps({"ok": True, "snapshot": snapshot,
        "last_success": datetime.now(timezone.utc).isoformat(), "restore_verified": True}, indent=2))
    notify(True)
    print("Encrypted backup and byte-for-byte/database restore verified:", snapshot)


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("action", choices=("init", "run", "check"))
    parser.add_argument("--repository", type=Path)
    args = parser.parse_args()
    if args.action == "init":
        if not args.repository:
            parser.error("init requires --repository")
        initialize(args.repository)
    elif args.action == "check":
        print(restic(config(), "check").decode())
    else:
        try:
            with lock():
                backup()
        except Exception:
            notify(False)
            raise
