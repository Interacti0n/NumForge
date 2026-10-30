"""End-to-end Compose check. Only disposable numforge-test-* projects may run."""
import json
import os
import secrets
from pathlib import Path
import sys
import urllib.request

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "scripts"))
import compose

if not os.environ.get("COMPOSE_PROJECT_NAME", "").startswith("numforge-test-"):
    raise SystemExit("Set COMPOSE_PROJECT_NAME=numforge-test-NAME; this test removes its project volumes.")
if not (compose.STATE / "app.json").exists():
    compose.init()
base = "http://127.0.0.1:" + os.environ.get("NUMFORGE_PORT", "18876")


def calculate():
    request = urllib.request.Request(base + "/api/evaluate?precision=10&angle=rad", data=b"2+2", method="POST", headers={"Origin": base})
    with urllib.request.urlopen(request, timeout=15) as response:
        assert json.load(response)["result"] == "4"


def account_code(code):
    compose.docker("exec", "-T", "accounts", "python", "manage.py", "shell", "-c", code)


try:
    compose.docker("up", "--build", "--wait", "--wait-timeout", "180")
    calculate()
    auth_checks = (ROOT / "tests/accounts/test_auth.py").read_text(encoding="utf-8")
    compose.docker("exec", "-T", "accounts", "python", input=
        "import sys, django\ndjango.setup()\n" + auth_checks +
        "\nresult=unittest.TextTestRunner(verbosity=2).run(unittest.defaultTestLoader.loadTestsFromTestCase(AuthChecks))\n"
        "sys.exit(0 if result.wasSuccessful() else 1)\n", text=True)
    account_code("from django.contrib.auth import get_user_model; "
                 "u=get_user_model().objects.create_user(username='container-check',password='test-only'); "
                 "assert u.check_password('test-only'); "
                 "from django.db import connection; c=connection.cursor(); "
                 "c.execute('SELECT rolsuper,rolcreatedb,rolcreaterole FROM pg_roles WHERE rolname=current_user'); "
                 "assert c.fetchone()==(False,False,False); "
                 "from identity.views import admit_login,LOGIN_LIMIT; from django.test import RequestFactory; "
                 "r=RequestFactory().post('/',REMOTE_ADDR='192.0.2.8'); "
                 "assert all(admit_login(r)==0 for _ in range(LOGIN_LIMIT))")
    compose.docker("restart", "accounts")
    account_code("from django.contrib.auth import get_user_model; "
                 "assert get_user_model().objects.get(username='container-check').check_password('test-only'); "
                 "from identity.views import admit_login; from django.test import RequestFactory; "
                 "assert admit_login(RequestFactory().post('/',REMOTE_ADDR='192.0.2.8'))>0")
    archive = compose.STATE / f"backups/container-check-{secrets.token_hex(6)}.dump"
    compose.backup(archive)
    compose.verify(archive)
    compose.docker("stop", "db")
    calculate()
    compose.docker("start", "db")
    compose.docker("up", "--wait", "--wait-timeout", "180")
    calculate()
    print("Compose build/start, persisted identity, restore and DB-independent calculation passed.")
finally:
    compose.docker("logs", "--no-color", "--tail", "60")
    compose.docker("down", "--volumes", "--remove-orphans")
