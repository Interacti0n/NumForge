"""Create a local, read-only inspection role; never reuse application credentials."""
import json
from pathlib import Path
import secrets

import compose

import psycopg
from psycopg import sql

ROOT = Path(__file__).resolve().parents[1]
STATE = ROOT / ".local/ops"


def setup():
    compose.private_directory(STATE)
    admin = json.loads((ROOT / ".local/compose/admin.json").read_text())
    path = STATE / "db-inspect.json"
    if not path.exists():
        with path.open("x", encoding="utf-8") as output:
            json.dump({"user": "nf_inspect", "password": secrets.token_urlsafe(32)}, output)
    account = json.loads(path.read_text())
    role = sql.Identifier(account["user"])
    with psycopg.connect(host="127.0.0.1", port=55433, dbname="numforge_accounts",
                          user=admin["user"], password=admin["password"]) as connection:
        if not connection.execute("SELECT 1 FROM pg_roles WHERE rolname=%s", (account["user"],)).fetchone():
            connection.execute(sql.SQL("CREATE ROLE {} LOGIN PASSWORD {}").format(role, sql.Literal(account["password"])))
        connection.execute(sql.SQL("ALTER ROLE {} NOSUPERUSER NOCREATEDB NOCREATEROLE NOREPLICATION NOBYPASSRLS").format(role))
        connection.execute(sql.SQL("ALTER ROLE {} SET default_transaction_read_only=on").format(role))
        connection.execute(sql.SQL("GRANT CONNECT ON DATABASE numforge_accounts TO {}").format(role))
        connection.execute(sql.SQL("GRANT USAGE ON SCHEMA public TO {}").format(role))
        connection.execute(sql.SQL("GRANT SELECT ON ALL TABLES IN SCHEMA public TO {}").format(role))
        connection.execute(sql.SQL("ALTER DEFAULT PRIVILEGES FOR ROLE nf_migrate IN SCHEMA public GRANT SELECT ON TABLES TO {}").format(role))
    # This file is in the private host directory, never in source control.
    (STATE / "pgpass.conf").write_text(f"127.0.0.1:55433:numforge_accounts:{account['user']}:{account['password']}\n")
    with psycopg.connect(host="127.0.0.1", port=55433, dbname="numforge_accounts", **account) as connection:
        if not connection.execute("SELECT count(*) FROM django_migrations").fetchone()[0]:
            raise RuntimeError("Inspection database has no migration ledger")
        if connection.execute("SELECT has_table_privilege(current_user,'identity_user','INSERT')").fetchone()[0]:
            raise RuntimeError("Inspection role unexpectedly permits inserts")
        # Even disabling the convenience read-only setting must not grant writes.
        connection.commit()
        connection.execute("SET transaction_read_only=off")
        try:
            connection.execute("DELETE FROM identity_user WHERE false")
        except psycopg.errors.InsufficientPrivilege:
            connection.rollback()
        else:
            raise RuntimeError("Inspection role unexpectedly permits writes")
    print("Read-only PostgreSQL inspection verified; no rows changed.")


if __name__ == "__main__":
    setup()
