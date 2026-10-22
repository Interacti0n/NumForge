param([string]$ServerPath, [string]$TunnelPath, [int]$Port)
$ErrorActionPreference = 'Stop'
Import-Module ScheduledTasks
$helper = Join-Path (Split-Path $PSScriptRoot -Parent) 'scripts/remote.ps1'
$global:NumForgeStartupFixtureTask = $null
$global:NumForgeStartupFixtureRemovals = 0
# Use real task-definition cmdlets, but intercept every persistent scheduler write/read.
function Get-ScheduledTask { param($TaskPath, $TaskName, $ErrorAction) return $global:NumForgeStartupFixtureTask }
function Register-ScheduledTask {
    param($TaskPath, $TaskName, $Action, $Trigger, $Settings, $Principal, $Description, [switch]$Force)
    $global:NumForgeStartupFixtureTask = [pscustomobject]@{ TaskName = $TaskName; TaskPath = $TaskPath
        Actions = @($Action); Triggers = $Trigger; Settings = $Settings; Principal = $Principal
        Description = $Description; State = 'Ready' }
}
function Unregister-ScheduledTask {
    param($TaskPath, $TaskName, [switch]$Confirm)
    if ($TaskName -cne $global:NumForgeStartupFixtureTask.TaskName) { throw 'Wrong task removed' }
    $global:NumForgeStartupFixtureTask = $null; $global:NumForgeStartupFixtureRemovals++
}
function Assert($value, $message) { if (-not $value) { throw $message } }
function Enable { & $helper -Action EnableStartup -Port $Port -ServerPath $ServerPath -CloudflaredPath $TunnelPath -Workers 2 -QueueCapacity 8 | Out-Null }
Enable
$task = $global:NumForgeStartupFixtureTask
Assert ($task.Triggers.Count -eq 2) 'Logon and periodic triggers are required'
Assert ($task.Triggers[1].Repetition.Interval -eq 'PT1M') 'Recovery interval must be one minute'
Assert (-not $task.Triggers[1].Repetition.Duration) 'Periodic recovery must not expire after a day'
Assert ($task.Principal.LogonType -eq 'Interactive' -and $task.Principal.RunLevel -eq 'Limited') 'No stored password or elevation'
Assert ((Split-Path $task.Actions[0].Execute -Leaf) -match '^remote-launcher-[a-f0-9]{16}\.exe$') 'Scheduler must start the windowless launcher'
$launcherBytes = [IO.File]::ReadAllBytes($task.Actions[0].Execute)
$peOffset = [BitConverter]::ToInt32($launcherBytes, 0x3c)
Assert ([BitConverter]::ToUInt16($launcherBytes, $peOffset + 92) -eq 2) 'Launcher must use the Windows GUI subsystem, not a console'
$launcherScratch = Join-Path (Split-Path $PSScriptRoot -Parent) "build/launcher check-$([guid]::NewGuid().ToString('N'))"
New-Item -ItemType Directory -Path $launcherScratch | Out-Null
try {
    $probePath = Join-Path $launcherScratch 'argument check.ps1'
    $outputPath = Join-Path $launcherScratch 'result.txt'
    [IO.File]::WriteAllText($probePath, @'
param([string]$Value, [string]$Output)
Add-Type 'using System; using System.Runtime.InteropServices; public static class ConsoleProbe { [DllImport("kernel32.dll")] public static extern IntPtr GetConsoleWindow(); }'
if ([ConsoleProbe]::GetConsoleWindow() -ne [IntPtr]::Zero) { exit 98 }
[IO.File]::WriteAllText($Output, $Value)
exit 17
'@)
    $probeArguments = '-NoProfile -NonInteractive -File "{0}" -Value "value with \"quotes\" and tail\\" -Output "{1}"' -f $probePath, $outputPath
    $probeSettings = [Diagnostics.ProcessStartInfo]::new($task.Actions[0].Execute, $probeArguments)
    $probeSettings.UseShellExecute = $false
    $probeSettings.CreateNoWindow = $true
    $probeProcess = [Diagnostics.Process]::Start($probeSettings)
    try {
        if (-not $probeProcess.WaitForExit(15000)) { $probeProcess.Kill(); [void]$probeProcess.WaitForExit(8000); throw 'Launcher probe timeout' }
        Assert ($probeProcess.ExitCode -eq 17) 'Launcher must preserve the child exit code and create no console'
        Assert ([IO.File]::ReadAllText($outputPath) -ceq 'value with "quotes" and tail\') 'Launcher must preserve quoted arguments, spaces and trailing backslashes'
    } finally { $probeProcess.Dispose() }
} finally {
    Assert ((Get-Item -LiteralPath $launcherScratch).Parent.FullName -ieq (Join-Path (Split-Path $PSScriptRoot -Parent) 'build')) 'Cleanup must remain in the owned build directory'
    Remove-Item -LiteralPath $launcherScratch -Recurse -Force
}
Assert ($task.Principal.UserId -eq [Security.Principal.WindowsIdentity]::GetCurrent().Name -and
    $task.Triggers[0].UserId -eq $task.Principal.UserId) 'Triggers and execution must use the current Windows account name'
Assert ($task.Settings.MultipleInstances -eq 'IgnoreNew' -and $task.Settings.ExecutionTimeLimit -eq 'PT3M') 'Bounded actions cannot overlap'
Assert ($task.Actions[0].Arguments.Contains('-Scheduled') -and
    $task.Actions[0].Arguments.Contains((' -ServerPath "{0}"' -f $ServerPath))) 'Bootstrap path with spaces must be quoted'
Enable
Assert ($global:NumForgeStartupFixtureTask.TaskName -eq $task.TaskName) 'Registration must update one project/user/port task'
$global:NumForgeStartupFixtureTask.Actions[0].Execute = Join-Path $env:SystemRoot 'System32/WindowsPowerShell/v1.0/powershell.exe'
Enable
Assert ((Split-Path $global:NumForgeStartupFixtureTask.Actions[0].Execute -Leaf) -match '^remote-launcher-') 'Existing PowerShell tasks must upgrade in place'
# A colliding foreign task may neither be overwritten nor unregistered.
$global:NumForgeStartupFixtureTask.Description = 'Unrelated task'
foreach ($action in @('EnableStartup', 'DisableStartup', 'Stop')) {
    $rejected = $false
    try {
        & $helper -Action $action -Port $Port -ServerPath $ServerPath -CloudflaredPath $TunnelPath | Out-Null
    } catch { $rejected = $_ -match 'not owned by this preview' }
    Assert $rejected "Foreign task must be preserved by $action"
}
Assert ($global:NumForgeStartupFixtureRemovals -eq 0) 'No foreign registration removed'
$global:NumForgeStartupFixtureTask = $task
$global:NumForgeStartupFixtureTask.State = 'Disabled'
& $helper -Action Ensure -Port $Port -Scheduled -ServerPath 'missing.exe' | Out-Null
& $helper -Action DisableStartup -Port $Port | Out-Null
Assert (-not $global:NumForgeStartupFixtureTask -and $global:NumForgeStartupFixtureRemovals -eq 1) 'Owned registration removed'
& $helper -Action Ensure -Port $Port -Scheduled -ServerPath 'missing.exe' | Out-Null
& $helper -Action DisableStartup -Port $Port | Out-Null
Assert ($global:NumForgeStartupFixtureRemovals -eq 1) 'Unregister is idempotent; queued checks cannot resurrect a disabled preview'
Enable
& $helper -Action Stop -Port $Port | Out-Null
Assert (-not $global:NumForgeStartupFixtureTask -and $global:NumForgeStartupFixtureRemovals -eq 2) 'Stop must also disable automatic resurrection'
Write-Output 'Startup definitions, ownership, disable and queued-invocation guards passed without scheduler registration.'
