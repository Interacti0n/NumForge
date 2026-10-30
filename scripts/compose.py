"""Generate private Compose configuration and preserve binary PostgreSQL backups."""
import argparse
from datetime import datetime, timezone
import json
import os
from pathlib import Path
import secrets
import subprocess

ROOT = Path(__file__).resolve().parents[1]
STATE = ROOT / ".local/compose"


def docker(*arguments, **kwargs):
    options = {"creationflags": subprocess.CREATE_NO_WINDOW} if os.name == "nt" else {}
    if not kwargs.get("capture_output"):
        kwargs.setdefault("stdout", subprocess.PIPE)
        kwargs.setdefault("stderr", subprocess.PIPE)
    result = subprocess.run(["docker", "compose", *arguments], cwd=ROOT, **options, **kwargs)
    for output in (result.stdout, result.stderr):
        if output:
            print(output.decode("utf-8", errors="replace") if isinstance(output, bytes) else output, flush=True)
    result.check_returncode()
    return result


def private_directory(path):
    path.mkdir(parents=True, exist_ok=True)
    if os.name == "nt":
        identity = subprocess.check_output(["whoami"], text=True).strip()
        subprocess.run(["icacls", str(path), "/inheritance:r", "/grant:r", f"{identity}:(OI)(CI)F",
                        "*S-1-5-18:(OI)(CI)F", "*S-1-5-32-544:(OI)(CI)F"], check=True,
                       stdout=subprocess.DEVNULL, creationflags=subprocess.CREATE_NO_WINDOW)
    else:
        path.chmod(0o700)


def load_pilot_environment():
    for line in (STATE / "pilot.env").read_text(encoding="utf-8-sig").splitlines():
        if line and not line.startswith("#"):
            name, value = line.split("=", 1)
            if name in {"COMPOSE_FILE", "COMPOSE_PROJECT_NAME", "NUMFORGE_PORT", "NUMFORGE_GATEWAY_PORT", "NF_ACCOUNT_PUBLIC_ORIGIN"}:
                os.environ[name] = value
    if os.name == "nt":
        docker_bin = Path(os.environ["LOCALAPPDATA"]) / "Programs/DockerDesktop/resources/bin"
        os.environ["PATH"] = str(docker_bin) + os.pathsep + os.environ["PATH"]


def init():
    if any((STATE / name).exists() for name in ("admin.json", "migrate.json", "app.json", "admin-password")):
        raise ValueError("Compose credentials already exist; refusing to replace them.")
    private_directory(STATE)
    shared = {"host": "db", "port": 5432, "secret_key": secrets.token_urlsafe(64)}
    for name, user, database in (("admin", "nf_local_admin", "postgres"),
                                  ("migrate", "nf_migrate", "numforge_accounts"),
                                  ("app", "nf_accounts", "numforge_accounts")):
        config = shared | {"name": database, "user": user, "password": secrets.token_urlsafe(32)}
        path = STATE / f"{name}.json"
        path.write_text(json.dumps(config, indent=2) + "\n", encoding="utf-8")
        # Compose bind-mounts individual files; its non-root service must read them.
        # The enclosing host directory stays private.
        path.chmod(0o644)
        if name == "admin":
            (STATE / "admin-password").write_text(config["password"], encoding="utf-8")
            (STATE / "admin-password").chmod(0o644)
    print("Private Compose configuration created; existing native database is unchanged.")
    gateway_init()


def gateway_init():
    if not (STATE / "app.json").is_file():
        raise ValueError("Initialize private Compose credentials first.")
    path = STATE / "gateway-token"
    if not path.exists():
        with path.open("x", encoding="utf-8") as output:
            output.write(secrets.token_urlsafe(48))
        path.chmod(0o644)


def backup(path):
    path.parent.mkdir(parents=True, exist_ok=True)
    with path.open("xb") as output:
        try:
            docker("exec", "-T", "db", "pg_dump", "-U", "nf_local_admin", "-d", "numforge_accounts", "-Fc", stdout=output)
        except BaseException:
            output.close()
            path.unlink()
            raise
    print(f"Backup: {path}")


def verify(path):
    name = "nf_restore_" + secrets.token_hex(8)
    docker("exec", "-T", "db", "createdb", "-U", "nf_local_admin", "-O", "nf_migrate", name)
    try:
        with path.open("rb") as archive:
            docker("exec", "-T", "db", "pg_restore", "-U", "nf_local_admin", "-d", name,
                   "--exit-on-error", "--single-transaction", stdin=archive)
        docker("run", "--rm", "--no-deps", "setup", "python", "/support/accounts_setup.py", "verify", name)
    finally:
        docker("exec", "-T", "db", "dropdb", "-U", "nf_local_admin", "--force", name)


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("action", choices=("init", "gateway-init", "backup", "verify-backup"))
    parser.add_argument("--file", type=Path)
    args = parser.parse_args()
    if args.action == "init":
        init()
    elif args.action == "gateway-init":
        gateway_init()
    elif args.action == "backup":
        backup(args.file or STATE / "backups" / datetime.now(timezone.utc).strftime("accounts-%Y%m%dT%H%M%S-%fZ.dump"))
    elif args.file:
        verify(args.file)
    else:
        parser.error("verify-backup requires --file")
