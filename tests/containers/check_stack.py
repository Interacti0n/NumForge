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
    compose.docker("run", "--rm", "--no-deps", "-e", "NF_ACCOUNTS_CONFIG=/run/secrets/migrate",
                   "setup", "python", "manage.py", "makemigrations", "--check", "--dry-run")
    calculate()
    auth_checks = (ROOT / "tests/accounts/test_auth.py").read_text(encoding="utf-8")
    preference_checks = (ROOT / "tests/accounts/test_preferences.py").read_text(encoding="utf-8")
    workspace_checks = (ROOT / "tests/accounts/test_workspaces.py").read_text(encoding="utf-8")
    compose.docker("exec", "-T", "accounts", "python", input=
        "import sys, django\ndjango.setup()\n" + auth_checks + "\n" + preference_checks + "\n" + workspace_checks +
        "\nresult=unittest.TextTestRunner(verbosity=2).run(unittest.TestSuite(unittest.defaultTestLoader.loadTestsFromTestCase(c) for c in (AuthChecks,PreferencesChecks,WorkspaceChecks)))\n"
        "sys.exit(0 if result.wasSuccessful() else 1)\n", encoding="utf-8")
    account_code("from django.contrib.auth import get_user_model; "
                 "u=get_user_model().objects.create_user(username='container-check',password='test-only'); "
                 "get_user_model().objects.create_user(username='container-other',password='test-only'); "
                 "from identity.models import Preferences,preference_defaults; "
                 "Preferences.objects.create(user=get_user_model().objects.get(username='preferences-alice'), "
                 "values=preference_defaults()|{'precision':20},revision=1); "
                 "from identity.models import Workspace; "
                 "Workspace.objects.create(user=u,name='Container persisted',size=1,data={'version':1,'tool':'calculator',"
                 "'inputs':{'expression':'12345678901234567890','settings':preference_defaults()}}); "
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
                 "from identity.models import Preferences; p=Preferences.objects.get(user__username='preferences-alice'); "
                 "assert p.revision==1 and p.values['precision'] in (20,21); "
                 "from identity.models import Workspace; "
                 "assert Workspace.objects.get(user__username='container-check').data['inputs']['expression']=='12345678901234567890'; "
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
    import check_gateway
    check_gateway.run()
finally:
    compose.docker("logs", "--no-color", "--tail", "60")
    compose.docker("down", "--volumes", "--remove-orphans")
