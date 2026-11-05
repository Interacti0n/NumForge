"""Disposable Kuma check: health, authenticated provisioning, failure detection and persistence."""
import json
import os
from pathlib import Path
import secrets
import subprocess
import sys
import tempfile
import urllib.request

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "scripts"))
import compose


def run():
    if not os.environ.get("COMPOSE_PROJECT_NAME", "").startswith("numforge-test-"):
        raise RuntimeError("Monitor checks require a disposable test project")
    (ROOT / "build").mkdir(exist_ok=True)
    # Override the whole port list, preserving the live monitor on 18880.
    with tempfile.TemporaryDirectory(dir=ROOT / "build") as temporary:
        override = Path(temporary) / "monitor.yaml"
        port = int(os.environ.get("NUMFORGE_TEST_MONITOR_PORT", "18886"))
        image = os.environ.get("NUMFORGE_TEST_MONITOR_IMAGE")
        previous_image = os.environ.get("NUMFORGE_TEST_MONITOR_BASE_IMAGE")
        if previous_image and not image:
            raise ValueError("Upgrade checks require a selected candidate image")
        image_ids = {selected: json.loads(subprocess.check_output(
            ["docker", "image", "inspect", selected]))[0]["Id"]
            for selected in (image, previous_image) if selected}

        def check_image(selected):
            container = compose.docker("ps", "-q", "uptime").stdout.decode().strip()
            actual = json.loads(subprocess.check_output(["docker", "inspect", container]))[0]
            assert actual["HostConfig"]["ReadonlyRootfs"]
            assert "ALL" in actual["HostConfig"]["CapDrop"]
            assert "no-new-privileges:true" in actual["HostConfig"]["SecurityOpt"]
            assert actual["Config"]["User"] in {"node", "1000", "1000:1000"}
            assert actual["State"]["Health"]["Status"] == "healthy"
            if selected:
                assert actual["Image"] == image_ids[selected]
            print("Verified monitor image and runtime protections.", flush=True)

        def select_image(selected):
            # JSON cannot express Compose's !override tag for the entire port list.
            override.write_text('services:\n  uptime:\n    ports: !override ' + json.dumps([f"127.0.0.1:{port}:3001"]) +
                                ('\n    image: ' + json.dumps(selected) if selected else '') + '\n')

        select_image(previous_image or image)
        os.environ["COMPOSE_FILE"] = os.pathsep.join(("compose.yaml", "compose.tools.yaml", str(override)))
        push_token = secrets.token_urlsafe(24)
        config = {"username": "monitor-fixture", "password": secrets.token_urlsafe(32), "monitors": [
            {"name": "fixture-up", "type": "keyword", "url": "http://127.0.0.1:3001", "keyword": "Uptime Kuma", "maxretries": 0},
            {"name": "fixture-down", "type": "keyword", "url": "http://127.0.0.1:3001", "keyword": "impossible-numforge-keyword", "maxretries": 0},
            {"name": "fixture-push", "type": "push", "pushToken": push_token, "interval": 3600, "maxretries": 0}]}

        def push(status):
            url = f"http://127.0.0.1:{port}/api/push/{push_token}?status={status}&msg=fixture"
            with urllib.request.build_opener(urllib.request.ProxyHandler({})).open(url, timeout=10) as response:
                assert json.load(response)["ok"]

        def provision():
            # Reauthenticating after every change also verifies persisted credentials.
            compose.docker("exec", "-T", "uptime", "node", "/opt/setup_monitor.cjs", input=json.dumps(config), encoding="utf-8")

        def patch_checks():
            guards = (ROOT / "deploy/patch_monitor.cjs").read_text(encoding="utf-8")
            sqlite_guards = (ROOT / "deploy/build_monitor_sqlite.cjs").read_text(encoding="utf-8")
            checks = (ROOT / "tests/containers/check_monitor_patches.cjs").read_text(encoding="utf-8")
            checks += "\n(()=>{\n" + (ROOT / "tests/containers/check_monitor_protobuf.cjs").read_text(encoding="utf-8") + "\n})();"
            checks += "\n(()=>{\n" + (ROOT / "tests/containers/check_monitor_liquid.cjs").read_text(encoding="utf-8") + "\n})();"
            checks += "\n(()=>{\n" + (ROOT / "tests/containers/check_monitor_grpc.cjs").read_text(encoding="utf-8") + "\n})();"
            checks += "\n(()=>{\n" + (ROOT / "tests/containers/check_monitor_ws.cjs").read_text(encoding="utf-8") + "\n})();"
            checks += "\n(()=>{\n" + (ROOT / "tests/containers/check_monitor_mysql.cjs").read_text(encoding="utf-8") + "\n})();"
            checks += "\n(()=>{\n" + (ROOT / "tests/containers/check_monitor_lodash.cjs").read_text(encoding="utf-8") + "\n})();"
            checks += "\n(()=>{\n" + (ROOT / "tests/containers/check_monitor_axios.cjs").read_text(encoding="utf-8") + "\n})();"
            checks += "\n(()=>{\n" + (ROOT / "tests/containers/check_monitor_glob.cjs").read_text(encoding="utf-8") + "\n})();"
            checks += "\n(()=>{\n" + (ROOT / "tests/containers/check_monitor_nodemailer.cjs").read_text(encoding="utf-8") + "\n})();"
            checks += "\n(()=>{\n" + (ROOT / "tests/containers/check_monitor_tar.cjs").read_text(encoding="utf-8") + "\n})();"
            checks += "\n(()=>{\n" + (ROOT / "tests/containers/check_monitor_msal.cjs").read_text(encoding="utf-8") + "\n})();"
            checks += "\n(()=>{\n" + (ROOT / "tests/containers/check_monitor_lower.cjs").read_text(encoding="utf-8") + "\n})();"
            checks += "\n(()=>{\n" + (ROOT / "tests/containers/check_monitor_sqlite.cjs").read_text(encoding="utf-8") + "\n})();"
            checks += "\n(()=>{\n" + (ROOT / "tests/containers/check_monitor_binding.cjs").read_text(encoding="utf-8") + "\n})();"
            checks += "\n(()=>{\n" + (ROOT / "tests/containers/check_monitor_glob14.cjs").read_text(encoding="utf-8") + "\n})();"
            bootstrap = "const Module=require('node:module');const guard=new Module('/tmp/nf-guards.cjs');guard.filename='/tmp/nf-guards.cjs';guard._compile(" + json.dumps(guards) + ",guard.filename);globalThis.monitorPatchGuards=guard.exports;\n"
            bootstrap += "const nativeGuard=new Module('/tmp/nf-sqlite.cjs');nativeGuard.filename='/tmp/nf-sqlite.cjs';nativeGuard._compile(" + json.dumps(sqlite_guards) + ",nativeGuard.filename);globalThis.monitorSqliteGuards=nativeGuard.exports;\n"
            compose.docker("exec", "-T", "uptime", "node", "-", input=bootstrap + checks, encoding="utf-8")

        def system_checks(selected):
            # A rollback parent deliberately lacks these new system backports.
            if previous_image and selected == previous_image:
                return
            binding = json.loads((ROOT / "deploy/monitor-binding.json").read_text(encoding="utf-8"))
            compose.docker("exec", "-T", "uptime", "node", "-e",
                           "const assert=require('node:assert/strict'); "
                           "const r=require('/opt/numforge/monitor-sqlite.json'); "
                           "assert.deepEqual(r.replacement,JSON.parse(process.argv[1])); "
                           "assert.equal(require('@louislam/sqlite3/package.json').name,'@vscode/sqlite3');",
                           json.dumps(binding))
            for check, guard in (("check_accounts_sqlite.py", "NUMFORGE_SQLITE_BACKPORT_CHECK"),
                                 ("check_monitor_system.py", "NUMFORGE_MONITOR_SYSTEM_CHECK")):
                compose.docker("exec", "-T", "-e", guard + "=1", "uptime", "python3", "-",
                               input=(ROOT / "tests/containers" / check).read_text(encoding="utf-8"),
                               encoding="utf-8")

        def snapshot(history_limit=None):
            # Hash private fixture rows instead of logging password hashes/tokens.
            program = '''const S=require('@louislam/sqlite3'),crypto=require('node:crypto');
const db=new S.Database('data/kuma.db',S.OPEN_READONLY);
const all=q=>new Promise((resolve,reject)=>db.all(q,(e,r)=>e?reject(e):resolve(r)));
(async()=>{const users=await all('SELECT id,username,password FROM user ORDER BY id');
const monitors=await all('SELECT id,name,type,url,keyword FROM monitor ORDER BY id');
const history=await all('SELECT id,monitor_id,status,time FROM heartbeat WHERE id<=LIMIT_ID ORDER BY id');
const digest=crypto.createHash('sha256').update(JSON.stringify({users,monitors,history})).digest('hex');
console.log(JSON.stringify({digest,history_limit:history.at(-1)?.id??0}));db.close();})().catch(e=>{console.error(e);process.exitCode=1;db.close();});'''
            program = program.replace("LIMIT_ID", str(history_limit if history_limit is not None else 9223372036854775807))
            result = compose.docker("exec", "-T", "uptime", "node", "-e", program)
            return json.loads(result.stdout)

        try:
            compose.docker("up", "--no-build" if image else "--build", "--no-deps", "--wait", "--wait-timeout", "120", "uptime")
            check_image(previous_image or image)
            provision()
            patch_checks()
            system_checks(previous_image or image)
            # Read-only SQLite queries expose no credentials or real user data.
            verify = '''const sqlite=require('@louislam/sqlite3');const db=new sqlite.Database('data/kuma.db',sqlite.OPEN_READONLY);
let tries=0;function check(){db.all("select m.name,h.status from monitor m left join heartbeat h on h.id=(select max(id) from heartbeat where monitor_id=m.id)",(err,rows)=>{
if(err)throw err;if(rows.length===3&&rows.some(r=>r.name==='fixture-up'&&r.status===1)&&rows.some(r=>r.name==='fixture-down'&&r.status===0)&&rows.some(r=>r.name==='fixture-push'&&r.status===1)){console.log('Monitor up/down, push and persistence verified');db.close();}
else if(++tries<20)setTimeout(check,1000);else{db.close();process.exitCode=1;}});}check();'''
            push("down")
            compose.docker("exec", "-T", "uptime", "node", "-e", verify.replace("name==='fixture-push'&&r.status===1", "name==='fixture-push'&&r.status===0"))
            push("up")
            compose.docker("exec", "-T", "uptime", "node", "-e", verify)
            stored = snapshot()
            compose.docker("restart", "uptime")
            compose.docker("up", "--no-build", "--no-deps", "--wait", "--wait-timeout", "120", "uptime")
            compose.docker("exec", "-T", "uptime", "node", "-e", verify)
            check_image(previous_image or image)
            assert snapshot(stored["history_limit"]) == stored, "Stored identities/config/history changed after restart"
            provision()
            if previous_image:
                for selected in (image, previous_image, image):
                    select_image(selected)
                    compose.docker("up", "--no-build", "--no-deps", "--wait", "--wait-timeout", "120", "uptime")
                    check_image(selected)
                    patch_checks()
                    system_checks(selected)
                    # Check stored monitor identities/history before provisioning.
                    compose.docker("exec", "-T", "uptime", "node", "-e", verify)
                    assert snapshot(stored["history_limit"]) == stored, "Stored identities/config/history changed during upgrade or rollback"
                    provision()
                    push("down")
                    compose.docker("exec", "-T", "uptime", "node", "-e", verify.replace("name==='fixture-push'&&r.status===1", "name==='fixture-push'&&r.status===0"))
                    push("up")
                    compose.docker("exec", "-T", "uptime", "node", "-e", verify)
                print("Monitor upgrade, rollback and reapply verified.", flush=True)
        finally:
            compose.docker("logs", "--tail", "15", "uptime")
            compose.docker("down", "--volumes", "--remove-orphans")


if __name__ == "__main__":
    run()
