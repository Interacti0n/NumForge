$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path -Parent $PSScriptRoot
$state = Join-Path $projectRoot '.local/ops'
$python = (Get-Content (Join-Path $state 'backup-runner.json') -Raw | ConvertFrom-Json).python
Push-Location $projectRoot
try {
    # A bounded log replaces the previous run; Restic retains the actual snapshots.
    & $python -u (Join-Path $PSScriptRoot 'ops_backup.py') run *> (Join-Path $state 'backup.log')
    exit $LASTEXITCODE
} finally { Pop-Location }
