"""One-shot container provisioning/migrations; never run in the HTTP process."""
import os
import subprocess
import sys

sys.path.insert(0, "/support")
import accounts_db as db

configs = {name: db.read_config(f"/run/secrets/{name}") for name in ("admin", "migrate", "app")}
if len(sys.argv) == 3 and sys.argv[1] == "verify" and sys.argv[2].startswith("nf_restore_"):
    with db.connect(configs["migrate"]) as source, db.connect(configs["admin"], dbname=sys.argv[2]) as restored:
        db.compare_databases(source, restored)
elif len(sys.argv) == 1:
    with db.connect(configs["admin"]) as connection:
        exists = connection.execute("SELECT 1 FROM pg_database WHERE datname = %s", (configs["app"]["name"],)).fetchone()
    if not exists:
        db.provision(configs)
    subprocess.run([sys.executable, "manage.py", "migrate", "--noinput"], check=True,
                   env=os.environ | {"NF_ACCOUNTS_CONFIG": "/run/secrets/migrate"})
    db.restrict_migration_ledger(configs["migrate"])
else:
    raise SystemExit("Unsupported setup command")
