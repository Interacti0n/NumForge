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
import sqlite3
from contextlib import closing
import subprocess
import tempfile
import urllib.request

import compose
from compose import load_pilot_environment as load_environment

ROOT = compose.ROOT
STATE = ROOT / ".local/ops"


def execute(args, **kwargs):
    options = {"creationflags": subprocess.CREATE_NO_WINDOW} if os.name == "nt" else {}
    kwargs.setdefault("stdout", subprocess.PIPE)
    kwargs.setdefault("stderr", subprocess.PIPE)
    return subprocess.run([str(a) for a in args], check=True, **options, **kwargs)


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


def verify_monitor(path):
    """Open a standalone snapshot read-only; never the running monitor's file."""
    with closing(sqlite3.connect(path.resolve().as_uri() + "?mode=ro", uri=True)) as database:
        if database.execute("PRAGMA integrity_check").fetchall() != [("ok",)]:
            raise ValueError("Monitor snapshot failed integrity check.")
        # These tables distinguish a Kuma backup from an unrelated valid SQLite file.
        counts = {table: database.execute('SELECT count(*) FROM "' + table + '"').fetchone()[0]
                  for table in ("user", "monitor", "heartbeat")}
    return counts


def snapshot_monitor(target):
    """VACUUM INTO includes committed WAL writes without stopping monitoring."""
    remote = "/tmp/numforge-backup-" + secrets.token_hex(16) + ".sqlite"
    command = ["docker", "compose", "exec", "-T", "uptime", "node", "-e"]
    program = "const S=require('@louislam/sqlite3');const d=new S.Database('data/kuma.db');" \
        "d.configure('busyTimeout',30000);d.exec(\"VACUUM INTO '" + remote + "'\",e=>{" \
        "if(e){console.error(e.message);process.exitCode=1;}d.close();});"
    try:
        execute([*command, program], cwd=ROOT)
        with target.open("xb") as output:
            execute([*command, "process.stdout.write(require('fs').readFileSync(" + json.dumps(remote) + "))"],
                    cwd=ROOT, stdout=output)
        verify_monitor(target)
    finally:
        execute([*command, "require('fs').rmSync(" + json.dumps(remote) + ",{force:true})"], cwd=ROOT)


def save_status(status):
    temporary = STATE / "backup-status.pending.json"
    temporary.write_text(json.dumps(status, indent=2), encoding="utf-8")
    temporary.replace(STATE / "backup-status.json")


def run_backup():
    # A busy lock belongs to another job; do not overwrite its status or signal
    # a failed backup when this invocation has not started one.
    with lock():
        try:
            backup()
        except Exception as error:
            path = STATE / "backup-status.json"
            try:
                previous = json.loads(path.read_text(encoding="utf-8")) if path.exists() else {}
            except (OSError, ValueError):
                previous = {}
            if not isinstance(previous, dict):
                previous = {}
            previous.update(ok=False, last_attempt=datetime.now(timezone.utc).isoformat(),
                            failure_class=type(error).__name__)
            save_status(previous)
            notify(False)
            raise


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
    monitor_pending = source / "monitor.pending.sqlite"
    monitor_pending.unlink(missing_ok=True)
    snapshot_monitor(monitor_pending)
    monitor_pending.replace(source / "monitor.sqlite")
    bundle = source / "source.pending.bundle"
    bundle.unlink(missing_ok=True)
    execute(["git", "-C", ROOT, "bundle", "create", bundle, "--all"])
    bundle.replace(source / "source.bundle")
    credentials = ["admin.json", "migrate.json", "app.json", "admin-password", "gateway-token", "pilot.env", "pilot-build.yaml", "pilot-runtime.json", "gateway-root.crt"]
    for name in credentials:
        path = ROOT / ".local/compose" / name
        if path.is_file():
            shutil.copyfile(path, source / name)
        else:
            (source / name).unlink(missing_ok=True)
    for name in ("monitor-admin.json", "db-inspect.json", "pgpass.conf"):
        path = STATE / name
        if path.is_file():
            shutil.copyfile(path, source / name)
        else:
            (source / name).unlink(missing_ok=True)
    commit = execute(["git", "-C", ROOT, "rev-parse", "HEAD"]).stdout.decode().strip()
    (source / "manifest.json").write_text(json.dumps({"source_commit": commit,
        "format_version": 2,
        "scope": "logical database dump, consistent monitor SQLite, committed Git history, private pilot/ops configuration",
        "monitor": verify_monitor(source / "monitor.sqlite"),
        "not_included": ["Docker images", "gateway private CA volume", "uncommitted source", "restic recovery password"]}, indent=2))
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
        verify_monitor(restored / "monitor.sqlite")
    restic(settings, "check")
    # Retention affects only this job's snapshots and follows successful restore.
    restic(settings, "forget", "--tag", "numforge-pilot", "--keep-daily", "7", "--keep-weekly", "4", "--keep-monthly", "6", "--prune")
    now = datetime.now(timezone.utc).isoformat()
    save_status({"ok": True, "snapshot": snapshot, "last_success": now, "last_attempt": now,
        "restore_verified": True, "format_version": 2, "monitor_restore_verified": True})
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
        run_backup()
