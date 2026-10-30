param([ValidateSet('Ensure', 'EnableStartup', 'DisableStartup', 'Status')][string]$Action = 'Ensure')
$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path -Parent $PSScriptRoot
$state = Join-Path $projectRoot '.local/compose'
$launcher = Join-Path $projectRoot '.local/ops/hidden-launcher.exe'
$taskName = 'NumForge account pilot'
$description = "Account pilot recovery: $projectRoot"
$arguments = '-NoProfile -NonInteractive -ExecutionPolicy Bypass -File "' + $PSCommandPath + '" -Action Ensure'

if ($Action -in @('EnableStartup', 'DisableStartup')) {
    $existing = Get-ScheduledTask -TaskName $taskName -ErrorAction SilentlyContinue
    if ($existing -and ($existing.Description -cne $description -or @($existing.Actions).Count -ne 1 -or
        $existing.Actions[0].Execute -ine $launcher -or $existing.Actions[0].Arguments -cne $arguments)) {
        throw 'A different task owns this name; it was left unchanged.'
    }
    if ($Action -eq 'DisableStartup') {
        if ($existing) { Unregister-ScheduledTask -TaskName $taskName -Confirm:$false }
        return
    }
    if (-not (Test-Path -LiteralPath $launcher)) {
        & "$env:SystemRoot\Microsoft.NET\Framework64\v4.0.30319\csc.exe" /nologo /target:winexe "/out:$launcher" (Join-Path $PSScriptRoot 'remote_launcher.cs')
        if ($LASTEXITCODE -ne 0) { throw 'Hidden launcher compilation failed.' }
    }
    $userName = [Security.Principal.WindowsIdentity]::GetCurrent().Name
    $triggers = @((New-ScheduledTaskTrigger -AtLogOn -User $userName),
        (New-ScheduledTaskTrigger -Once -At ([datetime]::Now.AddMinutes(1)) -RepetitionInterval (New-TimeSpan -Minutes 1)))
    $settings = New-ScheduledTaskSettingsSet -MultipleInstances IgnoreNew -StartWhenAvailable -ExecutionTimeLimit (New-TimeSpan -Minutes 5) -AllowStartIfOnBatteries -DontStopIfGoingOnBatteries
    $principal = New-ScheduledTaskPrincipal -UserId $userName -LogonType Interactive -RunLevel Limited
    $taskAction = New-ScheduledTaskAction -Execute $launcher -Argument $arguments -WorkingDirectory $projectRoot
    Register-ScheduledTask -TaskName $taskName -Description $description -Action $taskAction -Trigger $triggers -Settings $settings -Principal $principal -Force | Out-Null
    return
}
$python = (Get-Content (Join-Path $projectRoot '.local/ops/backup-runner.json') -Raw | ConvertFrom-Json).python
if ($Action -eq 'Status') { & $python (Join-Path $PSScriptRoot 'pilot.py') status; exit $LASTEXITCODE }
try { $lock = [IO.File]::Open((Join-Path $state 'pilot-supervisor.lock'), 'OpenOrCreate', 'ReadWrite', 'None') }
catch [IO.IOException] { exit 0 } # Another check already owns the recovery operation.
Push-Location $projectRoot
try {
    & $python -u (Join-Path $PSScriptRoot 'pilot.py') ensure *> (Join-Path $state 'pilot-supervisor.log')
    exit $LASTEXITCODE
} finally { Pop-Location; $lock.Dispose() }
