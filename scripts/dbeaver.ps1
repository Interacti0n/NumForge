$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path -Parent $PSScriptRoot
$env:PGPASSFILE = Join-Path $projectRoot '.local/ops/pgpass.conf'
if (-not (Test-Path -LiteralPath $env:PGPASSFILE -PathType Leaf)) { throw 'Run scripts/ops_db.py first.' }
Start-Process -FilePath (Join-Path $projectRoot '.local/tools/dbeaver/dbeaver.exe') -ArgumentList @('-data', ('"' + (Join-Path $projectRoot '.local/tools/dbeaver-workspace') + '"')) -WindowStyle Normal
