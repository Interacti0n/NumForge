param(
    [ValidateSet('Start', 'Stop', 'Status')][string]$Action = 'Start',
    [ValidateRange(1024, 65535)][int]$Port = 8766,
    [string]$ServerPath,
    [string]$CloudflaredPath
)
$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path $PSScriptRoot -Parent
$stateRoot = Join-Path $projectRoot 'build/remote-state'
$statePath = Join-Path $stateRoot "state-$Port.json"
function Get-OwnedProcess($entry) {
    $process = Get-Process -Id $entry.Id -ErrorAction SilentlyContinue
    if ($process -and $process.Path -eq $entry.Path -and
        $process.StartTime.ToUniversalTime().Ticks -eq ([datetime]$entry.StartTime).ToUniversalTime().Ticks) {
        return $process
    }
}
if (Test-Path -LiteralPath $statePath) {
    $state = Get-Content -LiteralPath $statePath -Raw | ConvertFrom-Json
    $owned = @($state.Processes | ForEach-Object { Get-OwnedProcess $_ })
    if ($Action -eq 'Status') {
        if ($owned.Count -eq 2) { Write-Output $state.Url } else { Write-Output 'Remote server is stopped or incomplete.' }
        return
    }
    if ($Action -eq 'Stop') {
        $owned | Stop-Process -Force
        Remove-Item -LiteralPath $statePath
        Write-Output 'Remote server and tunnel stopped. Existing local servers were left running.'
        return
    }
    if ($owned.Count) { throw "Remote processes already exist. Use -Action Stop -Port $Port first." }
} elseif ($Action -ne 'Start') {
    Write-Output 'No remote instance recorded on this port.'
    return
}
if (-not $ServerPath) { $ServerPath = Join-Path $projectRoot 'build/remote/Release/numforge_web.exe' }
if (-not $CloudflaredPath) { $CloudflaredPath = Join-Path $projectRoot 'build/tools/cloudflared.exe' }
$ServerPath = (Resolve-Path -LiteralPath $ServerPath).Path
$CloudflaredPath = (Resolve-Path -LiteralPath $CloudflaredPath).Path
# Probe ownership before exposing anything through a tunnel.
$probe = [Net.Sockets.TcpListener]::new([Net.IPAddress]::Loopback, $Port)
try { $probe.Start() } finally { $probe.Stop() }
New-Item -ItemType Directory -Path $stateRoot -Force | Out-Null
$tunnel = $null
$server = $null
try {
    $tunnelLog = Join-Path $stateRoot "tunnel-$Port.log"
    $tunnelOut = Join-Path $stateRoot "tunnel-$Port.stdout.log"
    $tunnelOptions = @{
        FilePath = $CloudflaredPath
        ArgumentList = @('tunnel', '--no-autoupdate', '--url', "http://127.0.0.1:$Port")
        PassThru = $true; WindowStyle = 'Hidden'
        RedirectStandardError = $tunnelLog; RedirectStandardOutput = $tunnelOut
    }
    $tunnel = Start-Process @tunnelOptions
    $url = $null
    for ($attempt = 0; $attempt -lt 120; $attempt++) {
        if ($tunnel.HasExited) { throw "Tunnel exited; inspect $tunnelLog" }
        $log = Get-Content -LiteralPath $tunnelLog -Raw -ErrorAction SilentlyContinue
        if ($log -match 'https://[a-z0-9-]+\.trycloudflare\.com') { $url = $Matches[0]; break }
        Start-Sleep -Milliseconds 500
    }
    if (-not $url) { throw "No tunnel URL within 60 seconds; inspect $tunnelLog" }
    $serverOptions = @{
        FilePath = $ServerPath
        ArgumentList = @('--no-browser', '--port', "$Port", '--origin', $url)
        PassThru = $true; WindowStyle = 'Hidden'
        RedirectStandardError = Join-Path $stateRoot "server-$Port.stderr.log"
        RedirectStandardOutput = Join-Path $stateRoot "server-$Port.stdout.log"
    }
    $server = Start-Process @serverOptions
    $ready = $false
    for ($attempt = 0; $attempt -lt 40; $attempt++) {
        if ($server.HasExited) { throw 'NumForge exited during startup; inspect its stderr log.' }
        try {
            $response = Invoke-WebRequest -Uri "http://127.0.0.1:$Port/" -TimeoutSec 2 -UseBasicParsing
            $ready = $response.StatusCode -eq 200
        } catch {}
        if ($ready) { break }
        Start-Sleep -Milliseconds 250
    }
    if (-not $ready) { throw 'NumForge did not become ready.' }
    $processes = @($server, $tunnel) | ForEach-Object {
        @{ Id = $_.Id; Path = $_.Path; StartTime = $_.StartTime.ToUniversalTime().ToString('o') }
    }
    @{ Url = $url; Port = $Port; Processes = @($processes) } | ConvertTo-Json -Depth 4 |
        Set-Content -LiteralPath $statePath -Encoding utf8
    Write-Output $url
    Write-Output "Stop with: powershell -File scripts/remote.ps1 -Action Stop -Port $Port"
} catch {
    foreach ($process in @($server, $tunnel)) {
        if ($process -and -not $process.HasExited) { Stop-Process -Id $process.Id -Force }
    }
    throw
}
