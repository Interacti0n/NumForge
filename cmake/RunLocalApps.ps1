param(
    [Parameter(Mandatory = $true)][string]$BuildDirectory,
    [ValidateSet('cli', 'web', 'both')][string]$Mode = 'both',
    [ValidateRange(1, 65535)][int]$Port = 8765
)

$ErrorActionPreference = 'Stop'
$appDirectory = (Resolve-Path -LiteralPath $BuildDirectory).Path
if (Test-Path -LiteralPath (Join-Path $appDirectory 'Release/numforge_web.exe')) {
    $appDirectory = Join-Path $appDirectory 'Release'
}
$cliPath = Join-Path $appDirectory 'calculator.exe'
$webPath = Join-Path $appDirectory 'numforge_web.exe'
if ($Mode -ne 'web' -and !(Test-Path -LiteralPath $cliPath -PathType Leaf)) {
    throw "CLI executable missing: $cliPath"
}
if ($Mode -ne 'cli') {
    if (!(Test-Path -LiteralPath $webPath -PathType Leaf)) {
        throw "Web executable missing: $webPath"
    }
    # Refuse an occupied port rather than opening an unrelated or stale server.
    $portProbe = [System.Net.Sockets.TcpListener]::new([System.Net.IPAddress]::Loopback, $Port)
    try { $portProbe.Start() }
    catch { throw "Port $Port is already in use. Choose NUMFORGE_LOCAL_PORT or stop that server yourself." }
    finally { $portProbe.Stop() }

    # Visible consoles are intentional: this mode explicitly opens the apps.
    $webProcess = Start-Process -FilePath $webPath -ArgumentList @('--port', "$Port") `
        -WorkingDirectory $appDirectory -WindowStyle Normal -PassThru
    $webUrl = "http://127.0.0.1:$Port/"
    $ready = $false
    for ($attempt = 0; $attempt -lt 30; $attempt++) {
        if ($webProcess.HasExited) { throw "Web server exited with code $($webProcess.ExitCode)." }
        try {
            $response = Invoke-WebRequest -Uri $webUrl -UseBasicParsing -TimeoutSec 1
            if ($response.StatusCode -eq 200) { $ready = $true; break }
        } catch { Start-Sleep -Milliseconds 100 }
    }
    if (!$ready) { throw "Web server did not become ready. Inspect its console (PID $($webProcess.Id))." }
    Write-Host "Web: $webUrl (PID $($webProcess.Id)); close its console or press Ctrl+C to stop it."
    Start-Process -FilePath $webUrl
}
if ($Mode -ne 'web') {
    $cliProcess = Start-Process -FilePath $cliPath -WorkingDirectory $appDirectory `
        -WindowStyle Normal -PassThru
    Write-Host "CLI started (PID $($cliProcess.Id)); type exit to close it."
}
