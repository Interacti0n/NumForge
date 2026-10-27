# Remote hosting

Keep C on loopback behind HTTPS; pages/API share one public hostname:

```sh
numforge_web --no-browser --port 8765 --origin https://calc.example.com
```

`--origin` permits one exact lowercase origin: no path/trailing slash, credentials,
query, fragment, wildcard or IPv6. Omit default 80/443 ports; nondefault ports work.
Local origins remain accepted. Foreign POST origins return 403; forwarding
headers cannot override this. Native clients may omit Origin. This is neither
authentication nor TLS/socket binding.

## Temporary public link from a Windows PC

```powershell
cmake -S . -B build/remote -DBUILD_TESTING=OFF -DNUMFORGE_WARNINGS_AS_ERRORS=ON
cmake --build build/remote --config Release --parallel 1
```

Get amd64 cloudflared from [official downloads](https://developers.cloudflare.com/cloudflare-one/networks/connectors/cloudflare-tunnel/downloads/)
as `build/tools/cloudflared.exe`, or pass `-CloudflaredPath`. The helper downloads nothing.

```powershell
powershell -File scripts/remote.ps1
powershell -File scripts/remote.ps1 -Action Status -Json
powershell -File scripts/remote.ps1 -Action Restart -ServerPath C:\NumForge\candidate\numforge_web.exe
powershell -File scripts/remote.ps1 -Action Stop
```

Start uses hidden processes/port 8766; `-Port` and `-ServerPath` override defaults.
Use a [verified extracted package](../project/RELEASE_PACKAGING.md). Paths,
arguments, identities and logs live in ignored `build/remote-state/`; preserve it
and the running package. Helper defaults: workers=4, queue=64, BelowNormal
priority. `-Workers`/`-QueueCapacity` configure them. Priority is not a CPU/RAM cap.
Status reports verified identities, local 2+2/origin health, CPU and process memory;
Ready does not prove public reachability.

Restart retains the tunnel/URL, clears sessions and attempts rollback on failed
health checks. Ensure repairs stopped processes, preserving a live tunnel or
replacing the old-origin backend when the tunnel address changes. Actions lock
per project/port and stop only matching path/start-time identities. Windows
replacement force-stops work; it does not drain requests. Failed rollback leaves
an incomplete state requiring an available executable or Stop/Start.

Anyone with the URL can calculate. Keep the PC awake/online. No router forwarding
is needed; recreated Quick Tunnels change address and have no uptime guarantee.
[Quick Tunnel limits](https://developers.cloudflare.com/tunnel/get-started/quick-tunnels/).

### Automatic startup and recovery

```powershell
powershell -File scripts/remote.ps1 -Action EnableStartup
powershell -File scripts/remote.ps1 -Action DisableStartup
```

EnableStartup registers one current-user/project/port task: at login and every
minute, with a cached GUI launcher that avoids console flashes. Requires Windows
PowerShell 5.1's .NET compiler; no stored password/service or pre-login startup.
It does not wake the PC/change sleep policy. Overlapping runs/busy locks are
skipped; each invocation is limited to three minutes.

Ensure uses saved paths/settings. Rerun EnableStartup after upgrades to refresh
fallback arguments; keep its cached launcher. Slow-but-live processes are not
restarted. Supervisor logs rotate at 256 KiB. DisableStartup keeps current
processes; Stop removes the task and owned processes. Delayed checks cannot
restart a disabled preview. Backend recovery loses sessions; tunnel recovery
can change the URL. Real reboot/login behavior needs a separate host test.

## Stable hosting

For a stable PC hostname, use a named tunnel, private credentials and a process
supervisor. A Linux VPS can use an unprivileged NumForge service behind Caddy:

```caddyfile
calc.example.com {
    reverse_proxy 127.0.0.1:8765
}
```

Point DNS/open proxy ports 80/443; keep backend 8765 private and preserve Origin.
Example systemd unit after creating the numforge user:

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

These example OS limits are not installed by NumForge. Verify supervisor,
proxy, host limits and rollback; keep a known previous archive.
[Restart rehearsal](../project/RELEASE_PACKAGING.md#restart-and-rollback-rehearsal).

## Capacity and state

| Resource | Policy |
| --- | --- |
| Workers / socket queue | Defaults 4/32; `--workers 1–16`, `--queue-capacity 1–64` |
| Queue wait | One second; acceptor checks every 100 ms, even while workers are busy |
| Read / send deadlines | Two seconds each; read begins at acceptance and includes queue time |
| Session / legacy-cache pools | 64 slots each; 30-minute idle expiry after reservation release |
| Retained requested heap | 8 MiB per ID across pools, 128 MiB/store |

Queue entries hold sockets/timestamps, not parsed bodies. Full/expired admission
gets best-effort 503 `server_busy`/Retry-After:1 or a transport reset. One request
per connection; OS listen backlog is separately requested as 64. Signal shutdown
stops admission, discards queued work, joins active requests and frees sessions;
force-stop bypasses cleanup.

Readers/writers/waiters pin state. Reuse empty/expired slots then unpinned FIFO;
all pinned returns `session_busy`. Expired/released IDs remain tombstones until
replacement; use a fresh ID after state loss. Sessions are memory-only, not users.

Retention counts values, reserved limb capacity, previews/ans, displays and both
histories; it excludes fixed storage, allocator overhead, sockets and transient
work, so it is not RSS. Writes reserve before commit. HTTP 400
`session_memory_limit` preserves data/RNG/revisions; clear/delete/reset to free
quota. Calculation allocation/time budgets are separate.

[Recorded load evidence](../performance/baselines/SERVER.md) applies only to its
host/profile/workload. Validate actual expressions, proxy and OS limits before
choosing capacity; add ingress limits/monitoring for public service. Multiple
instances need sticky routing for memory-only sessions. HTTPS alone adds no
authenticated ownership or scalable persistence.
