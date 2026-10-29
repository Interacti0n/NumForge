# Docker Compose

Optional Linux-container deployment, independent of the C SDK/CLI build.
Requires Docker Engine with Compose, or
[Docker Desktop/WSL 2 on Windows](https://docs.docker.com/desktop/setup/install/windows-install/).
Python 3.12 is needed on the host only for configuration and backup helpers.

```sh
python scripts/compose.py init
docker compose config --quiet
docker compose up --build --wait
```

Open **http://127.0.0.1:18876**. `NUMFORGE_PORT` changes both host/container ports,
preserving browser Origin checks. The existing preview on 8766 is unaffected.

| Service | Purpose |
| --- | --- |
| `web` | C calculator/web; builds without PostgreSQL or Python dependencies |
| `db` | PostgreSQL 18; persistent `accounts-data` volume, no published port |
| `setup` | One-shot role/schema provisioning and migrations before accounts start |
| `accounts` | Private Django/Gunicorn service with runtime credentials only |

Accounts currently have no public routes; gateway/TLS and login integration are
the next step. This is a local deployment foundation, not public account hosting.
Keep the host binding on loopback until HTTPS/origin configuration is explicit.
`web` has no dependency on the DB or accounts. Its default native binding remains
127.0.0.1; explicit `--bind 0.0.0.0` enables container networking.

Configuration/secrets live in ignored `.local/compose/`, separate from native
`.local/accounts/`. Do not delete or regenerate them while keeping a DB volume.
Each container receives only its needed secret files. Host directories are
private; individual files must be readable by the non-root service after Docker
bind-mounts them. Compose secrets are not an encrypted secret vault.

```sh
docker compose ps
docker compose logs --tail 50
docker compose stop
docker compose up --wait
docker compose down
```

`down` preserves the DB volume; **`down --volumes` destroys its data**.
Restart policies take effect when Docker itself is running; Docker Desktop's
startup/login behavior still needs validation on the target PC.

## Backups and moving to a server

Pause account writes during verification. The helper preserves binary dumps even
in Windows PowerShell; avoid shell redirection of binary `pg_dump` output there.

```sh
python scripts/compose.py backup
python scripts/compose.py verify-backup --file .local/compose/backups/FILE.dump
```

Verification creates a disposable database, compares all public table rows and
removes only that database. Protect copies of dumps/configuration on another
device. Native and container databases are separate: moving existing data needs
an explicit backup/restore, never copying a Windows PostgreSQL data directory.
On a Linux server, use the same Compose setup, restored data/configuration and a
separately configured HTTPS gateway. Review capacity, updates and backup retention
before public hosting; image builds do not imply tested ARM deployment.

CI builds the stack, checks browser-Origin calculations, persisted identities,
backup/restore and calculations with DB stopped. For a disposable local check,
set `COMPOSE_PROJECT_NAME=numforge-test-local`, then run
`python tests/containers/check_stack.py`. It removes that project's volumes.
