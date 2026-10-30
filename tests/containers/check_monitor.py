"""Disposable Kuma check: health, authenticated provisioning, failure detection and persistence."""
import json
import os
from pathlib import Path
import secrets
import sys
import tempfile

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
        override.write_text(f'services:\n  uptime:\n    ports: !override ["127.0.0.1:{port}:3001"]\n')
        os.environ["COMPOSE_FILE"] = os.pathsep.join(("compose.yaml", "compose.tools.yaml", str(override)))
        config = {"username": "monitor-fixture", "password": secrets.token_urlsafe(32), "monitors": [
            {"name": "fixture-up", "type": "keyword", "url": "http://127.0.0.1:3001", "keyword": "Uptime Kuma", "maxretries": 0},
            {"name": "fixture-down", "type": "keyword", "url": "http://127.0.0.1:3001", "keyword": "impossible-numforge-keyword", "maxretries": 0}]}
        try:
            compose.docker("up", "--build", "--no-deps", "--wait", "--wait-timeout", "120", "uptime")
            compose.docker("exec", "-T", "uptime", "node", "/opt/setup_monitor.cjs", input=json.dumps(config), encoding="utf-8")
            # Read-only SQLite queries expose no credentials or real user data.
            verify = '''const sqlite=require('@louislam/sqlite3');const db=new sqlite.Database('data/kuma.db',sqlite.OPEN_READONLY);
let tries=0;function check(){db.all("select m.name,h.status from monitor m left join heartbeat h on h.id=(select max(id) from heartbeat where monitor_id=m.id)",(err,rows)=>{
if(err)throw err;if(rows.length===2&&rows.some(r=>r.name==='fixture-up'&&r.status===1)&&rows.some(r=>r.name==='fixture-down'&&r.status===0)){console.log('Monitor up/down and persistence verified');db.close();}
else if(++tries<20)setTimeout(check,1000);else{db.close();process.exitCode=1;}});}check();'''
            compose.docker("exec", "-T", "uptime", "node", "-e", verify)
            compose.docker("restart", "uptime")
            compose.docker("up", "--no-build", "--no-deps", "--wait", "--wait-timeout", "120", "uptime")
            compose.docker("exec", "-T", "uptime", "node", "-e", verify)
        finally:
            compose.docker("logs", "--tail", "15", "uptime")
            compose.docker("down", "--volumes", "--remove-orphans")


if __name__ == "__main__":
    run()
