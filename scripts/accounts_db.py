"""Local-only PostgreSQL lifecycle. Run with the optional accounts virtualenv."""
import argparse
import json
import os
from pathlib import Path
import secrets
import subprocess
import sys
from datetime import datetime, timezone

import psycopg
from psycopg import sql

ROOT = Path(__file__).resolve().parents[1]
STATE = ROOT / ".local/accounts"


def read_config(path):
    return json.loads(Path(path).read_text(encoding="utf-8"))


def connect(config, **overrides):
    fields = {key: config[key] for key in ("host", "port", "user", "password")}
    fields["dbname"] = config["name"]
    return psycopg.connect(**(fields | overrides), connect_timeout=5, autocommit=True)


def run(command, **kwargs):
    if os.name == "nt":
        kwargs["creationflags"] = subprocess.CREATE_NO_WINDOW
    return subprocess.run([str(part) for part in command], check=True, **kwargs)


def pg_binary(name):
    directory = os.environ.get("NF_POSTGRES_BIN", str(STATE / "pgsql/bin"))
    return Path(directory) / (name + (".exe" if os.name == "nt" else ""))


def start():
    if not (STATE / "data/PG_VERSION").is_file():
        raise ValueError("Initialize the local cluster first.")
    status = subprocess.run([str(pg_binary("pg_ctl")), "status", "-D", str(STATE / "data")],
                            capture_output=True, creationflags=subprocess.CREATE_NO_WINDOW if os.name == "nt" else 0)
    if status.returncode:
        run([pg_binary("pg_ctl"), "start", "-w", "-t", "30", "-D", STATE / "data",
             "-l", STATE / "postgres.log"])


def init(port):
    """Fresh cluster only; never overwrite credentials or an existing database."""
    if any((STATE / name).exists() for name in ("admin.json", "migrate.json", "app.json", "data")):
        raise ValueError("Local account state already exists; init will not replace it.")
    if not pg_binary("initdb").is_file():
        raise ValueError("Install PostgreSQL binaries or set NF_POSTGRES_BIN first.")
    STATE.mkdir(parents=True, exist_ok=True)
    if os.name == "nt":
        # Remove inherited access before creating password files or cluster data.
        identity = subprocess.check_output(["whoami"], text=True).strip()
        run(["icacls", STATE, "/inheritance:r", "/grant:r", f"{identity}:(OI)(CI)F",
             "*S-1-5-18:(OI)(CI)F", "*S-1-5-32-544:(OI)(CI)F"], stdout=subprocess.DEVNULL)
    else:
        STATE.chmod(0o700)
    shared = {"host": "127.0.0.1", "port": port, "secret_key": secrets.token_urlsafe(64)}
    configurations = {
        "admin": shared | {"name": "postgres", "user": "nf_local_admin", "password": secrets.token_urlsafe(32)},
        "migrate": shared | {"name": "numforge_accounts", "user": "nf_migrate", "password": secrets.token_urlsafe(32)},
        "app": shared | {"name": "numforge_accounts", "user": "nf_accounts", "password": secrets.token_urlsafe(32)},
    }
    for name, config in configurations.items():
        path = STATE / f"{name}.json"
        path.write_text(json.dumps(config, indent=2) + "\n", encoding="utf-8")
        if os.name != "nt":
            path.chmod(0o600)
    password_file = STATE / "init-password"
    try:
        password_file.write_text(configurations["admin"]["password"], encoding="utf-8")
        run([pg_binary("initdb"), "-D", STATE / "data", "-U", "nf_local_admin",
             "--pwfile", password_file, "--auth=scram-sha-256", "--encoding=UTF8", "--locale=C"])
    finally:
        password_file.unlink(missing_ok=True)
    with (STATE / "data/postgresql.conf").open("a", encoding="utf-8") as output:
        output.write(f"\nlisten_addresses = '127.0.0.1'\nport = {port}\nmax_connections = 20\nshared_buffers = '64MB'\n")
    start()
    provision(configurations)
    migrate()


def provision(configurations):
    """Provision an empty database, shared by native and container setup."""
    with connect(configurations["admin"]) as connection:
        for name in ("migrate", "app"):
            config = configurations[name]
            connection.execute(sql.SQL("CREATE ROLE {} LOGIN PASSWORD {} NOSUPERUSER NOCREATEDB NOCREATEROLE NOREPLICATION").format(
                sql.Identifier(config["user"]), sql.Literal(config["password"])))
        connection.execute("CREATE DATABASE numforge_accounts OWNER nf_migrate")
    with connect(configurations["admin"], dbname="numforge_accounts") as connection:
        connection.execute("REVOKE ALL ON DATABASE numforge_accounts FROM PUBLIC")
        connection.execute("GRANT CONNECT ON DATABASE numforge_accounts TO nf_migrate, nf_accounts")
        connection.execute("REVOKE CREATE ON SCHEMA public FROM PUBLIC")
        connection.execute("GRANT USAGE ON SCHEMA public TO nf_accounts")
        connection.execute("ALTER DEFAULT PRIVILEGES FOR ROLE nf_migrate IN SCHEMA public GRANT SELECT, INSERT, UPDATE, DELETE ON TABLES TO nf_accounts")
        connection.execute("ALTER DEFAULT PRIVILEGES FOR ROLE nf_migrate IN SCHEMA public GRANT USAGE, SELECT ON SEQUENCES TO nf_accounts")


def migrate():
    environment = os.environ | {"NF_ACCOUNTS_CONFIG": str(STATE / "migrate.json")}
    run([sys.executable, ROOT / "apps/accounts/manage.py", "migrate", "--noinput"], env=environment)
    restrict_migration_ledger(read_config(STATE / "migrate.json"))


def restrict_migration_ledger(config):
    with connect(config) as connection:
        connection.execute("REVOKE ALL ON django_migrations FROM nf_accounts")


def backup(destination=None, config=None):
    config = config or read_config(STATE / "migrate.json")
    destination = Path(destination) if destination else STATE / "backups" / (
        datetime.now(timezone.utc).strftime("accounts-%Y%m%dT%H%M%S-%fZ.dump"))
    destination = destination.resolve()
    destination.parent.mkdir(parents=True, exist_ok=True)
    if destination.exists():
        raise ValueError("Backup destination already exists.")
    environment = os.environ | {"PGPASSWORD": config["password"]}
    try:
        run([pg_binary("pg_dump"), "--host", config["host"], "--port", config["port"],
             "--username", config["user"], "--dbname", config["name"], "--no-password",
             "--format=custom", "--file", destination], env=environment)
    except BaseException:
        destination.unlink(missing_ok=True)
        raise
    print(f"Backup: {destination}")
    return destination


def verify_backup(path, config=None):
    """Restore only into a newly created disposable DB; never replace the source."""
    admin = read_config(STATE / "admin.json")
    migrator = config or read_config(STATE / "migrate.json")
    name = "nf_restore_" + secrets.token_hex(8)
    with connect(admin) as connection:
        connection.execute(sql.SQL("CREATE DATABASE {} OWNER {}").format(sql.Identifier(name), sql.Identifier(migrator["user"])))
        try:
            run([pg_binary("pg_restore"), "--host", admin["host"], "--port", admin["port"],
                 "--username", admin["user"], "--dbname", name, "--no-password", "--exit-on-error",
                 "--single-transaction", Path(path).resolve()], env=os.environ | {"PGPASSWORD": admin["password"]})
            with connect(admin, dbname=name) as restored, connect(migrator) as source:
                compare_databases(source, restored)
        finally:
            connection.execute(sql.SQL("DROP DATABASE {} WITH (FORCE)").format(sql.Identifier(name)))


def compare_databases(source, restored):
    tables = source.execute("SELECT tablename FROM pg_tables WHERE schemaname = 'public' ORDER BY tablename").fetchall()
    restored_tables = restored.execute("SELECT tablename FROM pg_tables WHERE schemaname = 'public' ORDER BY tablename").fetchall()
    if not tables or tables != restored_tables:
        raise ValueError("Restored table set differs from source.")
    for (table,) in tables:
        query = sql.SQL("SELECT row_to_json(t)::text FROM {} t ORDER BY row_to_json(t)::text").format(sql.Identifier(table))
        if source.execute(query).fetchall() != restored.execute(query).fetchall():
            raise ValueError(f"Restored data differs in {table}; quiesce writes and retry.")
    print(f"Restore verified: {len(tables)} tables and all rows; source unchanged.")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("action", choices=("init", "start", "stop", "status", "migrate", "backup", "verify-backup"))
    parser.add_argument("--port", type=int, default=55432)
    parser.add_argument("--file", type=Path)
    args = parser.parse_args()
    if not 1024 <= args.port <= 65535:
        parser.error("Use a port between 1024 and 65535.")
    if args.action == "init":
        init(args.port)
    elif args.action == "start":
        start()
    elif args.action == "stop":
        run([pg_binary("pg_ctl"), "stop", "-w", "-t", "30", "-m", "fast", "-D", STATE / "data"])
    elif args.action == "migrate":
        migrate()
    elif args.action == "status":
        with connect(read_config(STATE / "app.json")) as connection:
            print(connection.execute("SELECT current_database(), current_user, version()").fetchone())
    elif args.action == "backup":
        backup(args.file)
    elif args.file:
        verify_backup(args.file)
    else:
        parser.error("verify-backup requires --file.")


if __name__ == "__main__":
    main()
