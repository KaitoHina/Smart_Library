param(
    [ValidateRange(1, 65535)]
    [int]$WebPort = 8080
)

$ErrorActionPreference = 'Stop'
$node = Get-Command node -ErrorAction SilentlyContinue
if (-not $node) { throw 'Install Node.js 22 or newer, then reopen PowerShell.' }
if ([int](& $node.Source -p "process.versions.node.split('.')[0]") -lt 22) {
    throw 'Node.js 22 or newer is required.'
}

$url = "http://127.0.0.1:$WebPort"
$env:PORT = "$WebPort"
# ESP8266 reaches this server through the computer's LAN address.
$env:DASHBOARD_HOST = '0.0.0.0'
$server = Start-Process -FilePath $node.Source -ArgumentList 'dashboard-server.cjs' -WorkingDirectory $PSScriptRoot -WindowStyle Hidden -PassThru
try {
    $ready = $false
    for ($attempt = 0; $attempt -lt 20; $attempt++) {
        if ($server.HasExited) { throw "Dashboard server exited. Check whether port $WebPort is in use." }
        try {
            Invoke-WebRequest -Uri "$url/api/telemetry" -UseBasicParsing -TimeoutSec 1 | Out-Null
            $ready = $true
            break
        } catch {
            Start-Sleep -Milliseconds 250
        }
    }
    if (-not $ready) { throw "Dashboard did not start at $url." }
    Start-Process $url
    Write-Host "Dashboard running at $url. ESP8266 should POST to this computer's LAN IP on port $WebPort."
    Write-Host 'Press Ctrl+C to stop.'
    while (-not $server.HasExited) { Start-Sleep -Seconds 1 }
} finally {
    if (-not $server.HasExited) { Stop-Process -Id $server.Id }
    $server.Dispose()
}
