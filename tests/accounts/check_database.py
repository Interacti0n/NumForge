"""Real PostgreSQL integration checks in a disposable DB, never the account DB."""
import json
import os
from pathlib import Path
import secrets
import sys
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "scripts"))
import accounts_db as db


class DatabaseChecks(unittest.TestCase):
    def test_runtime_has_no_schema_or_cluster_privileges(self):
        with db.connect(APP) as connection:
            flags = connection.execute("SELECT rolsuper, rolcreatedb, rolcreaterole, rolreplication FROM pg_roles WHERE rolname = current_user").fetchone()
            self.assertEqual(flags, (False, False, False, False))
            for statement in ("CREATE TABLE forbidden (id integer)",
                              "ALTER TABLE identity_user ADD COLUMN forbidden integer",
                              "DELETE FROM django_migrations",
                              "CREATE DATABASE forbidden", "SET ROLE nf_migrate"):
                with self.assertRaises(db.psycopg.errors.InsufficientPrivilege):
                    connection.execute(statement)

    def test_user_password_session_and_unique_constraint(self):
        from django.contrib.auth import get_user_model
        from django.contrib.sessions.backends.db import SessionStore
        from django.db import IntegrityError, transaction

        users = get_user_model().objects
        user = users.create_user(username="database-check", password="not-a-real-account-password")
        self.assertNotEqual(user.password, "not-a-real-account-password")
        self.assertTrue(user.check_password("not-a-real-account-password"))
        self.assertFalse(user.check_password("wrong"))
        with self.assertRaises(IntegrityError), transaction.atomic():
            users.create_user(username=user.username)
        session = SessionStore()
        session["probe"] = {"user": user.pk, "exact_text": "123456789012345678901234567890"}
        session.save()
        self.assertEqual(SessionStore(session_key=session.session_key)["probe"]["user"], user.pk)
        self.assertFalse(SessionStore(session_key="missing-session").load())

    def test_no_public_routes(self):
        from django.test import Client

        client = Client(HTTP_HOST="localhost")
        for path in ("/", "/login", "/register", "/admin/"):
            self.assertEqual(client.get(path, secure=True).status_code, 404)


if __name__ == "__main__":
    admin = db.read_config(db.STATE / "admin.json")
    migrator = db.read_config(db.STATE / "migrate.json")
    APP = db.read_config(db.STATE / "app.json")
    name = "nf_test_" + secrets.token_hex(8)
    migrator = migrator | {"name": name}
    APP = APP | {"name": name}
    with db.connect(admin) as connection:
        connection.execute(db.sql.SQL("CREATE DATABASE {} OWNER nf_migrate").format(db.sql.Identifier(name)))
        try:
            with db.connect(admin, dbname=name) as test_admin:
                test_admin.execute(db.sql.SQL("REVOKE ALL ON DATABASE {} FROM PUBLIC").format(db.sql.Identifier(name)))
                test_admin.execute(db.sql.SQL("GRANT CONNECT ON DATABASE {} TO nf_migrate, nf_accounts").format(db.sql.Identifier(name)))
                test_admin.execute("REVOKE CREATE ON SCHEMA public FROM PUBLIC")
                test_admin.execute("GRANT USAGE ON SCHEMA public TO nf_accounts")
                test_admin.execute("ALTER DEFAULT PRIVILEGES FOR ROLE nf_migrate IN SCHEMA public GRANT SELECT, INSERT, UPDATE, DELETE ON TABLES TO nf_accounts")
                test_admin.execute("ALTER DEFAULT PRIVILEGES FOR ROLE nf_migrate IN SCHEMA public GRANT USAGE, SELECT ON SEQUENCES TO nf_accounts")
            with tempfile.TemporaryDirectory(prefix="checks-", dir=db.STATE) as temporary:
                config_file = Path(temporary) / "config.json"

                def manage(*arguments):
                    config_file.write_text(json.dumps(migrator), encoding="utf-8")
                    db.run([sys.executable, ROOT / "apps/accounts/manage.py", *arguments],
                           env=os.environ | {"NF_ACCOUNTS_CONFIG": str(config_file)})
                    if arguments[0] == "migrate":
                        db.restrict_migration_ledger(migrator)

                manage("migrate", "--noinput", "--verbosity=0")
                manage("makemigrations", "--check", "--dry-run")
                config_file.write_text(json.dumps(APP), encoding="utf-8")
                os.environ["NF_ACCOUNTS_CONFIG"] = str(config_file)
                os.environ["DJANGO_SETTINGS_MODULE"] = "config.settings"
                sys.path.insert(0, str(ROOT / "apps/accounts"))
                import django
                django.setup()
                from test_auth import AuthChecks
                suite = unittest.TestSuite(unittest.defaultTestLoader.loadTestsFromTestCase(case) for case in (DatabaseChecks, AuthChecks))
                result = unittest.TextTestRunner(verbosity=2).run(suite)
                from django.db import connections
                connections.close_all()
                if not result.wasSuccessful():
                    raise SystemExit(1)
                archive = db.backup(Path(temporary) / "check.dump", migrator)
                db.verify_backup(archive, migrator)
                manage("migrate", "identity", "zero", "--noinput", "--verbosity=0")
                manage("migrate", "--noinput", "--verbosity=0")
                with db.connect(APP) as restored:
                    if restored.execute("SELECT count(*) FROM identity_user").fetchone()[0] != 0:
                        raise AssertionError("Reverse/forward migration did not recreate the identity table.")
                print("Migration reversal/reapplication and runtime grants verified.")
        finally:
            connection.execute(db.sql.SQL("DROP DATABASE {} WITH (FORCE)").format(db.sql.Identifier(name)))
