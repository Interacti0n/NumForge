"""Real TLS/cookie/CSRF checks, called only inside the disposable stack test."""
import copy
import http.cookiejar
import json
import os
import ssl
import subprocess
import time
import urllib.error
import urllib.request

import compose


def verify_process_reaping():
    """Exercise the actual HTTPS probe; orphaned ssl_client children must be reaped."""
    if not os.environ.get("COMPOSE_PROJECT_NAME", "").startswith("numforge-test-"):
        raise RuntimeError("Gateway process checks require a disposable test project.")
    container = compose.docker("ps", "-q", "gateway").stdout.decode().strip()
    configuration = json.loads(subprocess.check_output(["docker", "inspect", container]))[0]
    assert configuration["HostConfig"]["Init"] is True
    assert configuration["HostConfig"]["PidsLimit"] == 256
    probe = ('i=0; while [ "$i" -lt 40 ]; do '
             'wget -T 3 -q --no-check-certificate -O /dev/null https://localhost:8443/ || exit; '
             'i=$((i+1)); done')
    compose.docker("exec", "-T", "gateway", "/bin/sh", "-c", probe)
    for _ in range(20):
        output = compose.docker("exec", "-T", "gateway", "ps", "-o", "pid,ppid,stat,comm").stdout.decode()
        zombies = [line for line in output.splitlines()[1:]
                   if len(line.split()) >= 4 and line.split()[2].startswith("Z")]
        if not zombies:
            break
        time.sleep(0.1)
    assert not zombies, "Gateway retained zombie processes after HTTPS checks: " + repr(zombies)
    print("Verified gateway init, PID bound and 40 HTTPS checks without retained zombies.")


def run():
    project = os.environ.get("COMPOSE_PROJECT_NAME", "")
    if not project.startswith("numforge-test-"):
        raise RuntimeError("Gateway checks require a disposable test project.")
    compose.gateway_init()
    os.environ["COMPOSE_FILE"] = os.pathsep.join(["compose.yaml", "compose.accounts.yaml"])
    os.environ.setdefault("NUMFORGE_GATEWAY_PORT", "18879")
    base = "https://localhost:" + os.environ["NUMFORGE_GATEWAY_PORT"]
    os.environ["NF_ACCOUNT_PUBLIC_ORIGIN"] = base
    compose.docker("run", "--rm", "--no-deps", "--entrypoint", "/bin/sh", "gateway", "-c",
                   'NF_GATEWAY_TOKEN=$(cat /run/secrets/gateway_token); export NF_GATEWAY_TOKEN; exec caddy validate --config /etc/caddy/Caddyfile --adapter caddyfile')
    compose.docker("up", "--no-build", "--wait", "--wait-timeout", "180", "gateway", "accounts")
    verify_process_reaping()
    root = compose.STATE / f"{project}-root.crt"
    compose.docker("cp", "gateway:/data/caddy/pki/authorities/local/root.crt", str(root))
    context = ssl.create_default_context(cafile=str(root))
    jar = http.cookiejar.CookieJar()

    def opener(cookies):
        return urllib.request.build_opener(urllib.request.ProxyHandler({}),
            urllib.request.HTTPSHandler(context=context), urllib.request.HTTPCookieProcessor(cookies))

    client = opener(jar)

    def request(path, data=None, token=None, origin=None, target=client, **headers):
        headers["Origin"] = origin or base
        if token:
            headers["X-CSRFToken"] = token
        if data is not None:
            headers["Content-Type"] = "application/json"
        query = urllib.request.Request(base + path, data=json.dumps(data).encode() if data is not None else None, headers=headers)
        try:
            response = target.open(query, timeout=15)
        except urllib.error.HTTPError as error:
            response = error
        with response:
            body = response.read().decode()
            return response.status, json.loads(body) if "application/json" in response.headers.get("Content-Type", "") else body

    for language in ("sk", "en"):
        status, page = request("/login?lang="+language)
        assert status == 200 and f'<html lang="{language}">' in page
    calculation = urllib.request.Request(base+"/api/evaluate?precision=10", data=b"2+2", headers={"Origin":base, "Content-Type":"text/plain"})
    with client.open(calculation, timeout=15) as response:
        assert json.load(response)["result"] == "4"
    assert request("/api/evaluate?precision=10", {"dummy":1}, origin="https://foreign.example")[0] == 403
    assert request("/api/account/login", {"username":"container-check", "password":"test-only"})[0] == 403
    token = request("/api/account/session")[1]["csrf_token"]
    status, data = request("/api/account/login", {"username":"container-check", "password":"test-only"}, token,
                           **{"X-Numforge-Client-IP":"192.0.2.8", "X-Forwarded-For":"192.0.2.8"})
    assert status == 200 and data["user"]["username"] == "container-check"
    session = next(cookie for cookie in jar if cookie.name == "numforge_account")
    assert session.secure and session.has_nonstandard_attr("HttpOnly")
    account_started = compose.docker("exec", "-T", "accounts", "cat", "/proc/1/stat").stdout.split()[21]
    compose.docker("restart", "db")
    compose.docker("up", "--no-deps", "--wait", "--wait-timeout", "180", "db")
    assert compose.docker("exec", "-T", "accounts", "cat", "/proc/1/stat").stdout.split()[21] == account_started
    status, after_restart = request("/api/account/session")
    assert status == 200 and after_restart["user"] == data["user"]
    replay = http.cookiejar.CookieJar()
    for cookie in jar:
        replay.set_cookie(copy.copy(cookie))
    assert request("/api/account/logout", {}, data["csrf_token"])[0] == 200
    assert request("/api/account/session", target=opener(replay))[1]["user"] is None
    if os.environ.get("NUMFORGE_CHECK_ACCOUNT_BROWSER") == "1":
        subprocess.run(["node", str(compose.ROOT / "tests/containers/account-ui.cjs")], check=True,
                       env=os.environ | {"NF_UI_BASE_URL":base})
        subprocess.run(["node", str(compose.ROOT / "tests/containers/preferences-ui.cjs")], check=True,
                       env=os.environ | {"NF_UI_BASE_URL":base})
        subprocess.run(["node", str(compose.ROOT / "tests/containers/workspaces-ui.cjs")], check=True,
                       env=os.environ | {"NF_UI_BASE_URL":base})
    print("Verified TLS, SK/EN, CSRF, secure login/logout, first session request after DB restart and cookie replay rejection.")
