param(
    [ValidateSet('Start', 'Stop', 'Status', 'Restart', 'Ensure', 'EnableStartup', 'DisableStartup')][string]$Action = 'Start',
    [ValidateRange(1024, 65535)][int]$Port = 8766,
    [string]$ServerPath,
    [string]$CloudflaredPath,
    [ValidateRange(1, 16)][int]$Workers = 4,
    [ValidateRange(1, 64)][int]$QueueCapacity = 64,
    [switch]$Json,
    [switch]$Scheduled
)
$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path $PSScriptRoot -Parent
$stateRoot = Join-Path $projectRoot 'build/remote-state'
$statePath = Join-Path $stateRoot "state-$Port.json"
$supervised = $Action -eq 'Ensure'
$powershellPath = Join-Path $env:SystemRoot 'System32/WindowsPowerShell/v1.0/powershell.exe'
$userIdentity = [Security.Principal.WindowsIdentity]::GetCurrent()
$userSid = $userIdentity.User.Value
$userName = $userIdentity.Name
$hash = [Security.Cryptography.SHA256]::Create()
try { $projectId = ([BitConverter]::ToString($hash.ComputeHash([Text.Encoding]::UTF8.GetBytes("$($projectRoot.ToLowerInvariant())|$userSid")))).Replace('-', '').Substring(0, 12) }
finally { $hash.Dispose() }
$taskName = "NumForge-preview-$projectId-$Port"
$taskDescription = "NumForge preview: $projectRoot; port $Port; user $userSid"
$taskPrefix = '-NoProfile -NonInteractive -WindowStyle Hidden -File "{0}" -Action Ensure -Port {1} -Scheduled ' -f $PSCommandPath, $Port

function Get-StartupTask {
    $task = Get-ScheduledTask -TaskPath '\' -TaskName $taskName -ErrorAction SilentlyContinue
    if (-not $task) { return }
    $execute = if (@($task.Actions).Count -eq 1) { $task.Actions[0].Execute } else { $null }
    $knownLauncher = $execute -and (Split-Path $execute -Parent) -ieq $stateRoot -and
        (Split-Path $execute -Leaf) -cmatch '^remote-launcher-[a-f0-9]{16}\.exe$'
    if ($task -and ($task.Description -cne $taskDescription -or @($task.Actions).Count -ne 1 -or
        (-not $knownLauncher -and $execute -ine $powershellPath) -or $task.Actions[0].WorkingDirectory -ine $projectRoot -or
        -not $task.Actions[0].Arguments.StartsWith($taskPrefix, [StringComparison]::Ordinal))) {
        throw "Task $taskName is not owned by this preview; it was left unchanged."
    }
    return $task
}
function Get-WindowlessLauncher {
    $source = Join-Path $PSScriptRoot 'remote_launcher.cs'
    $sourceHash = [Security.Cryptography.SHA256]::Create()
    try { $digest = ([BitConverter]::ToString($sourceHash.ComputeHash([IO.File]::ReadAllBytes($source)))).Replace('-', '').ToLowerInvariant().Substring(0, 16) }
    finally { $sourceHash.Dispose() }
    $launcher = Join-Path $stateRoot "remote-launcher-$digest.exe"
    # Versioned output avoids replacing a launcher still executing another port's check.
    $compileLock = [IO.File]::Open("$launcher.lock", 'OpenOrCreate', 'ReadWrite', 'None')
    $temporary = "$launcher.$([guid]::NewGuid().ToString('N')).tmp.exe"
    try {
        if (-not (Test-Path -LiteralPath $launcher)) {
            $compile = '$ProgressPreference = ''SilentlyContinue''; ' +
                "Add-Type -Path '$($source.Replace("'", "''"))' -OutputAssembly '$($temporary.Replace("'", "''"))' -OutputType WindowsApplication -ErrorAction Stop"
            $encoded = [Convert]::ToBase64String([Text.Encoding]::Unicode.GetBytes($compile))
            & $powershellPath -NoProfile -NonInteractive -EncodedCommand $encoded
            if ($LASTEXITCODE -ne 0) { throw 'Windowless launcher compilation failed.' }
            [IO.File]::Move($temporary, $launcher)
        }
        return $launcher
    } finally {
        if (Test-Path -LiteralPath $temporary) { Remove-Item -LiteralPath $temporary }
        $compileLock.Dispose()
    }
}
function Disable-Startup {
    if (Get-StartupTask) { Unregister-ScheduledTask -TaskPath '\' -TaskName $taskName -Confirm:$false }
}
function Write-SupervisorLog($message) {
    $logPath = Join-Path $stateRoot "supervisor-$Port.log"
    try {
        if ((Test-Path -LiteralPath $logPath) -and (Get-Item -LiteralPath $logPath).Length -gt 256KB) {
            [IO.File]::Copy($logPath, "$logPath.previous", $true)
            [IO.File]::WriteAllText($logPath, '')
        }
        Add-Content -LiteralPath $logPath -Value "$([datetime]::UtcNow.ToString('o')) $message" -Encoding utf8
    } catch { } # Diagnostics must not prevent recovery.
}

function Get-OwnedProcess($entry) {
    if (-not $entry) { return }
    try {
        $process = Get-Process -Id $entry.Id -ErrorAction Stop
        if ($process.Path -eq $entry.Path -and $process.StartTime.ToUniversalTime().Ticks -eq
            ([datetime]$entry.StartTime).ToUniversalTime().Ticks) { return $process }
    } catch { return }
}
function Get-Identity($process, $executablePath) {
    # Process.Path can still be empty immediately after Start-Process on Windows.
    @{ Id = $process.Id; Path = $executablePath; StartTime = $process.StartTime.ToUniversalTime().ToString('o') }
}
function Stop-OwnedProcess($entry) {
    $process = Get-OwnedProcess $entry
    if ($process) {
        Stop-Process -InputObject $process -Force
        if (-not $process.WaitForExit(8000)) { throw "Process $($entry.Id) did not stop." }
    }
}
function Test-Calculation($url) {
    try {
        $reply = Invoke-WebRequest -Uri "http://127.0.0.1:$Port/api/evaluate?precision=10&angle=rad" `
            -Method Post -Body '2+2' -ContentType 'text/plain' -Headers @{ Origin = $url } `
            -TimeoutSec 2 -UseBasicParsing
        $data = $reply.Content | ConvertFrom-Json
        return $reply.StatusCode -eq 200 -and $data.ok -eq $true -and $data.result -ceq '4'
    } catch { return $false }
}
function Save-State($value) {
    $temporary = "$statePath.$([guid]::NewGuid().ToString('N')).tmp"
    try {
        [IO.File]::WriteAllText($temporary, ($value | ConvertTo-Json -Depth 6), [Text.UTF8Encoding]::new($false))
        if (Test-Path -LiteralPath $statePath) { [IO.File]::Replace($temporary, $statePath, [NullString]::Value) }
        else { [IO.File]::Move($temporary, $statePath) }
    } finally { if (Test-Path -LiteralPath $temporary) { Remove-Item -LiteralPath $temporary } }
}
function Assert-FreePort {
    $probe = [Net.Sockets.TcpListener]::new([Net.IPAddress]::Loopback, $Port)
    try { $probe.Start() } finally { $probe.Stop() }
}
function Start-Backend($path, $arguments, $url) {
    Assert-FreePort
    $process = Start-Process -FilePath $path -ArgumentList $arguments -PassThru -WindowStyle Hidden `
        -RedirectStandardError (Join-Path $stateRoot "server-$Port.stderr.log") `
        -RedirectStandardOutput (Join-Path $stateRoot "server-$Port.stdout.log")
    try {
        $identity = Get-Identity $process $path
        $startup = [Diagnostics.Stopwatch]::StartNew()
        while ($startup.Elapsed.TotalSeconds -lt 10) {
            if ($process.HasExited) { throw 'NumForge exited during startup; inspect its stderr log.' }
            if ((Test-Calculation $url) -and (Get-OwnedProcess $identity)) { return $identity }
            Start-Sleep -Milliseconds 250
        }
        throw 'NumForge did not pass its calculation/origin check.'
    } catch {
        # This invocation created the process; cleanup must also work if its
        # module path never became readable during startup.
        if (-not $process.HasExited) { $process.Kill(); [void]$process.WaitForExit(8000) }
        throw
    }
}

# Serialize actions for this project/port; never race state replacement or two starts.
New-Item -ItemType Directory -Path $stateRoot -Force | Out-Null
try { $actionLock = [IO.File]::Open("$statePath.lock", 'OpenOrCreate', 'ReadWrite', 'None') }
catch {
    $lockError = $_.Exception.GetBaseException()
    if ($Scheduled -and $supervised -and $lockError -is [IO.IOException] -and ($lockError.HResult -band 0xFFFF) -eq 32) {
        Write-Output 'Another remote action is active; scheduled check skipped.'
        return
    }
    throw "Another remote action is active for port $Port, or its state lock is inaccessible."
}
try {
    # A queued scheduler invocation must not resurrect processes after Stop/DisableStartup.
    if ($Scheduled) {
        if (-not $supervised) { throw '-Scheduled is only valid with Ensure.' }
        $task = Get-StartupTask
        if (-not $task -or $task.State -eq 'Disabled') { return }
    }
    if ($Action -eq 'DisableStartup') {
        Disable-Startup
        Write-Output "Automatic startup/recovery disabled: $taskName. Running processes were retained."
        return
    }
    $state = $null
    if (Test-Path -LiteralPath $statePath) {
        $state = Get-Content -LiteralPath $statePath -Raw | ConvertFrom-Json
        if ($state.Port -ne $Port -or $state.Url -cnotmatch '^https://[a-z0-9-]+\.trycloudflare\.com$') {
            throw 'Invalid remote state; inspect the state file before continuing.'
        }
        # Old helpers recorded the backend first and the tunnel second.
        if (-not $state.SchemaVersion) {
            if (@($state.Processes).Count -ne 2) { throw 'Invalid legacy remote state.' }
            $state = [pscustomobject]@{ SchemaVersion = 2; Port = $Port; Url = $state.Url
                Server = $state.Processes[0]; Tunnel = $state.Processes[1]; Workers = $null; QueueCapacity = $null
                ServerArguments = @('--no-browser', '--port', "$Port", '--origin', $state.Url) }
        } elseif ($state.SchemaVersion -ne 2) { throw 'Unsupported remote state version.' }
    }
    $serverAlive = $null -ne (Get-OwnedProcess $state.Server)
    $tunnelAlive = $null -ne (Get-OwnedProcess $state.Tunnel)
    if ($Action -eq 'Status') {
        $healthy = $serverAlive -and (Test-Calculation $state.Url)
        $condition = if (-not $state) { 'NotRecorded' } elseif (-not $serverAlive -and -not $tunnelAlive) { 'Stopped' }
            elseif ($serverAlive -and $tunnelAlive -and $healthy) { 'Ready' } else { 'Incomplete' }
        $report = [pscustomobject]@{ State = $condition; Url = $state.Url; Port = $Port
            ServerAlive = $serverAlive; TunnelAlive = $tunnelAlive; LocalHealthy = $healthy
            ServerPath = $state.Server.Path; Workers = $state.Workers; QueueCapacity = $state.QueueCapacity
            PublicReachability = 'Not checked' }
        if ($Json) { $report | ConvertTo-Json } else { $report | Format-List }
        return
    }
    if ($Action -eq 'Stop') {
        Disable-Startup
        Stop-OwnedProcess $state.Server
        Stop-OwnedProcess $state.Tunnel
        if ($state) { Remove-Item -LiteralPath $statePath }
        Write-Output 'Recorded remote processes stopped; unrelated processes were left running.'
        return
    }
    if ($Action -eq 'Ensure' -and $serverAlive -and $tunnelAlive) {
        # Liveness recovery must not discard sessions just because all workers are busy.
        Write-Output "Recorded processes already running. $($state.Url)"
        return
    }
    if ($Action -eq 'Ensure' -or $Action -eq 'EnableStartup') {
        # Recorded paths/settings follow manual upgrades; supplied values bootstrap a lost state file.
        if ($state.Server.Path -and ($Action -eq 'Ensure' -or -not $ServerPath)) { $ServerPath = $state.Server.Path }
        if ($state.Tunnel.Path -and ($Action -eq 'Ensure' -or -not $CloudflaredPath)) { $CloudflaredPath = $state.Tunnel.Path }
        if (-not $PSBoundParameters.ContainsKey('Workers') -or $Action -eq 'Ensure') {
            if ($state.Workers) { $Workers = $state.Workers }
        }
        if (-not $PSBoundParameters.ContainsKey('QueueCapacity') -or $Action -eq 'Ensure') {
            if ($state.QueueCapacity) { $QueueCapacity = $state.QueueCapacity }
        }
        if (-not $ServerPath) { $ServerPath = Join-Path $projectRoot 'build/remote/Release/numforge_web.exe' }
        if (-not $CloudflaredPath) { $CloudflaredPath = Join-Path $projectRoot 'build/tools/cloudflared.exe' }
        $ServerPath = (Resolve-Path -LiteralPath $ServerPath).Path
        $CloudflaredPath = (Resolve-Path -LiteralPath $CloudflaredPath).Path
        if (-not (Test-Path -LiteralPath $ServerPath -PathType Leaf) -or
            -not (Test-Path -LiteralPath $CloudflaredPath -PathType Leaf)) { throw 'Preview paths must name executable files.' }
        if ($Action -eq 'EnableStartup') {
            [void](Get-StartupTask) # Refuse to replace a task with a colliding name/foreign action.
            $taskArguments = $taskPrefix + ('-ServerPath "{0}" -CloudflaredPath "{1}" -Workers {2} -QueueCapacity {3}' -f
                $ServerPath, $CloudflaredPath, $Workers, $QueueCapacity)
            $taskAction = New-ScheduledTaskAction -Execute (Get-WindowlessLauncher) -Argument $taskArguments -WorkingDirectory $projectRoot
            $triggers = @((New-ScheduledTaskTrigger -AtLogOn -User $userName),
                (New-ScheduledTaskTrigger -Once -At ([datetime]::Now.AddMinutes(1)) -RepetitionInterval (New-TimeSpan -Minutes 1)))
            $settings = New-ScheduledTaskSettingsSet -MultipleInstances IgnoreNew -StartWhenAvailable `
                -AllowStartIfOnBatteries -DontStopIfGoingOnBatteries -ExecutionTimeLimit (New-TimeSpan -Minutes 3)
            $principal = New-ScheduledTaskPrincipal -UserId $userName -LogonType Interactive -RunLevel Limited
            Register-ScheduledTask -TaskPath '\' -TaskName $taskName -Action $taskAction -Trigger $triggers `
                -Settings $settings -Principal $principal -Description $taskDescription -Force | Out-Null
            Write-Output "Automatic startup/recovery enabled: $taskName (current user, windowless, at logon and every minute)."
            return
        }
        if ($tunnelAlive) { $Action = 'Restart' }
        else {
            Stop-OwnedProcess $state.Server
            $serverAlive = $false
            $Action = 'Start'
        }
        Write-SupervisorLog "Recovering with $Action; previous URL: $($state.Url)"
    }
    if ($Action -eq 'Restart') {
        if (-not $tunnelAlive) { throw 'No live recorded tunnel. Use Stop then Start to obtain a new URL.' }
        if (-not $ServerPath) { $ServerPath = $state.Server.Path }
        if (-not $PSBoundParameters.ContainsKey('Workers') -and $state.Workers) { $Workers = $state.Workers }
        if (-not $PSBoundParameters.ContainsKey('QueueCapacity') -and $state.QueueCapacity) { $QueueCapacity = $state.QueueCapacity }
    } else {
        if ($serverAlive -or $tunnelAlive) { throw 'Recorded processes still exist. Use Restart or Stop first.' }
        if (-not $ServerPath) { $ServerPath = Join-Path $projectRoot 'build/remote/Release/numforge_web.exe' }
    }
    # Resolve the candidate before stopping a healthy backend; pin this path in state.
    $ServerPath = (Resolve-Path -LiteralPath $ServerPath).Path
    if (-not (Test-Path -LiteralPath $ServerPath -PathType Leaf)) { throw 'ServerPath must name an executable file.' }
    $arguments = @('--no-browser', '--port', "$Port", '--origin')
    if ($Action -eq 'Restart') {
        $old = $state.Server
        $oldArguments = $state.ServerArguments
        $oldWorkers = $state.Workers; $oldQueue = $state.QueueCapacity
        Stop-OwnedProcess $old
        $state.Server = $null
        try {
            $state.Server = Start-Backend $ServerPath ($arguments + $state.Url + @('--workers', "$Workers", '--queue-capacity', "$QueueCapacity")) $state.Url
            $state.ServerArguments = $arguments + $state.Url + @('--workers', "$Workers", '--queue-capacity', "$QueueCapacity")
            $state.Workers = $Workers; $state.QueueCapacity = $QueueCapacity
            Save-State $state
        } catch {
            $failure = $_
            try {
                @([datetime]::UtcNow.ToString('o'); "Candidate: $ServerPath"; "$failure"
                    Get-Content -LiteralPath (Join-Path $stateRoot "server-$Port.stderr.log") -Tail 20) |
                    Set-Content -LiteralPath (Join-Path $stateRoot "server-$Port.failure.log") -Encoding utf8
            } catch { } # Logging must not prevent restoration.
            Stop-OwnedProcess $state.Server
            $state.Server = $null
            try {
                $state.Server = Start-Backend $old.Path $oldArguments $state.Url
                $state.ServerArguments = $oldArguments
                $state.Workers = $oldWorkers; $state.QueueCapacity = $oldQueue
                Save-State $state
            } catch {
                Stop-OwnedProcess $state.Server
                $state.Server = $null
                Save-State $state
                throw "Restart failed and rollback failed; tunnel retained. $failure; $_"
            }
            throw "Restart failed; previous backend restored and tunnel retained. $failure"
        }
        Write-Output "Restarted backend; sessions cleared. $($state.Url)"
        if ($supervised) { Write-SupervisorLog "Backend restored; URL: $($state.Url)" }
        return
    }
    if (-not $CloudflaredPath) { $CloudflaredPath = Join-Path $projectRoot 'build/tools/cloudflared.exe' }
    $CloudflaredPath = (Resolve-Path -LiteralPath $CloudflaredPath).Path
    if (-not (Test-Path -LiteralPath $CloudflaredPath -PathType Leaf)) { throw 'CloudflaredPath must name an executable file.' }
    Assert-FreePort
    $tunnel = $null; $backend = $null
    try {
        $tunnelLog = Join-Path $stateRoot "tunnel-$Port.log"
        $tunnel = Start-Process -FilePath $CloudflaredPath -ArgumentList @('tunnel', '--no-autoupdate', '--url', "http://127.0.0.1:$Port") `
            -PassThru -WindowStyle Hidden -RedirectStandardError $tunnelLog `
            -RedirectStandardOutput (Join-Path $stateRoot "tunnel-$Port.stdout.log")
        $tunnelIdentity = Get-Identity $tunnel $CloudflaredPath
        $url = $null
        for ($attempt = 0; $attempt -lt 120; $attempt++) {
            if ($tunnel.HasExited) { throw 'Tunnel exited during startup; inspect its stderr log.' }
            $log = Get-Content -LiteralPath $tunnelLog -Raw -ErrorAction SilentlyContinue
            if ($log -cmatch 'https://[a-z0-9-]+\.trycloudflare\.com') { $url = $Matches[0]; break }
            Start-Sleep -Milliseconds 500
        }
        if (-not $url) { throw 'No tunnel URL within 60 seconds.' }
        $arguments += $url, '--workers', "$Workers", '--queue-capacity', "$QueueCapacity"
        $backend = Start-Backend $ServerPath $arguments $url
        if (-not (Get-OwnedProcess $tunnelIdentity)) { throw 'Tunnel stopped while the backend was starting.' }
        Save-State ([pscustomobject]@{ SchemaVersion = 2; Port = $Port; Url = $url; Server = $backend
            Tunnel = $tunnelIdentity; Workers = $Workers; QueueCapacity = $QueueCapacity; ServerArguments = $arguments })
        Write-Output $url
        if ($supervised) { Write-SupervisorLog "Preview restored; URL: $url" }
    } catch {
        Stop-OwnedProcess $backend
        if ($tunnel -and -not $tunnel.HasExited) { $tunnel.Kill(); [void]$tunnel.WaitForExit(8000) }
        throw
    }
} catch {
    if ($supervised) { Write-SupervisorLog "Recovery failed: $_" }
    throw
} finally { $actionLock.Dispose() }
