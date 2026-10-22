# Remote hosting

Keep the NumForge C server on loopback and place an HTTPS tunnel or reverse
proxy in front of it. Pages and browser API requests share the public hostname.

```sh
numforge_web --no-browser --port 8765 --origin https://calc.example.com
```

`--origin` allows one additional exact browser origin. Use a lowercase hostname
with no path, trailing slash, credentials, query or fragment. Omit default ports
(`:443` for HTTPS, `:80` for HTTP); nondefault ports are supported. Wildcards and
IPv6 origins are not supported. Localhost origins for the backend port remain
accepted. Other POST origins return 403 even with forged forwarding headers.
Native clients can omit Origin; this option is not authentication.

The default command retains the local origin policy. `--origin` does not change
socket binding or supply TLS. HTTPS and public connectivity belong to the proxy.

## Temporary public link from a Windows PC

Build an isolated executable:

```powershell
cmake -S . -B build/remote -DBUILD_TESTING=OFF -DNUMFORGE_WARNINGS_AS_ERRORS=ON
cmake --build build/remote --config Release --parallel 1
```

Download Windows amd64 `cloudflared` from the
[official downloads](https://developers.cloudflare.com/cloudflare-one/networks/connectors/cloudflare-tunnel/downloads/)
to `build/tools/cloudflared.exe`, or pass `-CloudflaredPath`. The helper does not
install or download software itself.

```powershell
powershell -File scripts/remote.ps1
powershell -File scripts/remote.ps1 -Action Status
powershell -File scripts/remote.ps1 -Action Restart
powershell -File scripts/remote.ps1 -Action Stop
```

The helper starts hidden processes on port 8766, gets a Quick Tunnel URL and
starts NumForge with that exact origin. Use `-ServerPath` to select an executable
from a [verified extracted package](../project/RELEASE_PACKAGING.md), or build the
default `build/remote/Release/numforge_web.exe` above. The selected absolute path,
arguments and process identities are saved under ignored `build/remote-state/`.
Keep this directory while running. `-Port` and `-CloudflaredPath` override defaults.

Preview defaults are four workers and 64 queue slots; `-Workers 1-16` and
`-QueueCapacity 1-64` override them. These are helper settings; the executable's
own default queue remains 32. `Status` verifies recorded process paths/start
times and a local 2+2 request with the public Origin. `-Action Status -Json`
returns machine-readable fields. `Ready` confirms local serving and a live
tunnel process, not public reachability; that field is explicitly not checked.

`Restart` replaces only the backend while retaining the live tunnel and its URL.
It inherits the saved executable/settings unless overridden, for example:

```powershell
powershell -File scripts/remote.ps1 -Action Restart -ServerPath C:\NumForge\candidate\numforge_web.exe
```

Sessions are lost on restart. If the candidate fails its calculation/origin check,
the helper attempts to restore the previous backend and arguments, reports an
error, and retains the tunnel. If rollback also fails, status is incomplete;
recover with an available executable or Stop/Start. `Ensure` repairs stopped
processes: it preserves a live tunnel, or replaces the old-origin backend when
creating a new tunnel URL. Start/Restart alone do not install automatic recovery.

Actions are serialized per project/port. Stop checks both executable path and
creation time before terminating a recorded process; unrelated processes are
left alone. Old state files remain readable. Startup failure cleans up its own
new processes. Logs retain the latest server/tunnel output; a failed restart
also saves `server-PORT.failure.log` before rollback replaces the server log.
These Windows replacements force-stop processes; they do not drain active work.
The [Windows preview checks](../performance/baselines/SERVER.md#windows-quick-tunnel-preview)
record public calculations, session bursts and restart/rollback through a real
tunnel, with their scope and remaining operating limits.

Anyone with the printed URL can use it, including a phone on mobile data. Keep
the PC awake and online. The tunnel needs no inbound firewall exception or router
port forwarding. Its address changes on recreation. Quick Tunnels are for
testing, with no uptime guarantee; see
[Cloudflare's documentation](https://developers.cloudflare.com/tunnel/get-started/quick-tunnels/).

### Automatic startup and recovery

After selecting a working executable with Start/Restart, opt in with:

```powershell
powershell -File scripts/remote.ps1 -Action EnableStartup
powershell -File scripts/remote.ps1 -Action DisableStartup
```

EnableStartup registers one task per project, Windows user and port. It runs
hidden with the current user's limited privileges, without a stored password,
after that user logs in and every minute while logged in. Task settings ignore
overlapping runs and limit each invocation to three minutes; see
[Windows task settings](https://learn.microsoft.com/en-us/powershell/module/scheduledtasks/new-scheduledtasksettingsset).
This is a preview supervisor, not a service that runs before login. It does not
wake the PC or change its sleep policy; its checks may run on battery power.

The task invokes `Ensure`, retaining recorded paths/settings after manual
upgrades; its explicit executable arguments bootstrap a missing state file.
Rerun EnableStartup after an upgrade to refresh those fallback paths. Dead
processes are repaired on a subsequent check, subject to startup/network delays.
Live processes are not restarted merely because work is slow or queued. Use
Status for local calculation health; public reachability is checked separately.
Recovery actions/errors go to `build/remote-state/supervisor-PORT.log`, retaining
one previous log when it exceeds 256 KiB. The current URL remains in the state file.

DisableStartup removes only this preview's verified task and keeps the web
running. Stop also removes that registration before stopping owned processes;
a delayed scheduled invocation cannot restart a disabled preview. Sessions are
lost whenever recovery replaces the backend, and a recreated Quick Tunnel gets
a new public address.

## Stable hosting

For a PC with a stable hostname, create a named Cloudflare Tunnel, route your
domain to the loopback server and set `--origin` to its HTTPS origin. Run both
processes through a service supervisor. Keep tunnel credentials outside Git.

On a Linux VPS, install NumForge under `/opt/numforge`, run it as a dedicated
unprivileged user and put Caddy in front. Example Caddyfile:

```caddyfile
calc.example.com {
    reverse_proxy 127.0.0.1:8765
}
```

Replace the example domain and point DNS to the VPS. Open ports 80/443 for Caddy
and keep backend port 8765 inaccessible from the Internet. Preserve Origin;
do not rewrite all origins to localhost or enable wildcard CORS. Caddy manages
the HTTPS certificate; see [its HTTPS guide](https://caddyserver.com/docs/quick-starts/https).

Example systemd unit, after creating a `numforge` user:

```ini
[Unit]
Description=NumForge web calculator
After=network.target

[Service]
User=numforge
ExecStart=/opt/numforge/bin/numforge_web --no-browser --port 8765 --origin https://calc.example.com
Restart=on-failure
RestartSec=3
NoNewPrivileges=true
PrivateTmp=true
ProtectSystem=strict
ProtectHome=true
MemoryMax=512M
CPUQuota=100%

[Install]
WantedBy=multi-user.target
```

Keep versioned application directories and a verified previous archive for
rollback. The [packaging rehearsal](../project/RELEASE_PACKAGING.md#restart-and-rollback-rehearsal)
checks cold replacement/restart on an isolated port before any live switch.
Verify the actual supervisor, proxy, operating limits and rollback on the host;
the rehearsal alone does not establish those deployment properties.

## Capacity and state

The loopback server uses four fixed workers and a FIFO queue of 32 connections.
`--workers 1-16` and `--queue-capacity 1-64` override those defaults. Each connection
carries one request; queued entries retain sockets/timestamps, not parsed bodies.
The OS listen backlog is separately requested as 64 and is subject to OS limits.

Queued connections expire after one second, checked by the acceptor every 100 ms
even when workers are occupied. Full/expired queues get a best-effort HTTP 503
with `code:server_busy` and `Retry-After: 1`, then close. A peer still sending data
can observe a reset instead; clients must handle transport failures too. Request
reading has a two-second deadline from acceptance, including queue time. Sending
has a separate two-second deadline; mathematical work keeps its cooperative limits.
These are scheduling policies, not hard real-time guarantees.

Ctrl+C/Ctrl+Break on Windows or SIGINT/SIGTERM on POSIX stops admission, rejects
queued sockets, joins active requests under their existing limits and then frees
sessions. Forced process termination bypasses this cleanup.

The store holds 64 sessions and 64 independent legacy cache entries. Idle values
expire after 30 minutes since the last request releases its reservation. Held
readers/writers and queued callers pin their state; expiry never interrupts them.
Empty/expired slots are reused first, then FIFO replacement skips pinned slots.
If every replacement is pinned, admission returns `session_busy`. Expired/released
session IDs remain tombstones until replacement; use a fresh ID after losing state.

Retained heap requests are capped at 8 MiB per ID across both pools and 128 MiB
per store. This includes numeric objects/reserved limb capacity, displays,
variables, preview, detached ans and both histories. Fixed inline store memory,
allocator overhead, sockets and temporary computation/serialization are separate;
the cap is not process RSS. Those temporary operations keep their existing budgets.
Writes reserve the complete prospective size before changing state. Rejection
returns HTTP 400, `session_memory_limit`, preserving data, RNG and retry revisions.
Clear calculator/conversion history, delete variables or reset a session to free
space; another client's release can also make a retry succeed.

Sessions are memory-only and disappear on restart; IDs are not authenticated
users. [Local Windows measurements](../performance/baselines/SERVER.md) distinguish
the default queue from `--workers 4 --queue-capacity 64`: the latter admitted all
60-client bursts in the recorded workload, while the default rejected some.
This is a candidate setting to validate, not a universal 60-user capacity claim.
Repeat load/memory checks on the target host with its actual CPU/memory limits,
proxy and intended expressions before choosing production capacity.

Before running a busy public service, add ingress rate limits, monitoring and
resource limits, and complete load/deployment validation.
Multiple instances need sticky routing for the existing in-memory sessions.
HTTPS exposure alone does not make this a scalable multiuser service.
