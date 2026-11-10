param(
    [ValidatePattern('^COM[0-9]+$')]
    [string]$Port,
    [ValidateRange(1, 65535)]
    [int]$WebPort = 8080
)

$ErrorActionPreference = 'Stop'
$node = Get-Command node -ErrorAction SilentlyContinue
if (-not $node) { throw 'Install Node.js 22 or newer, then reopen PowerShell.' }
if ([int](& $node.Source -p "process.versions.node.split('.')[0]") -lt 22) {
    throw 'Node.js 22 or newer is required.'
}

if (-not $Port) {
    $ports = [System.IO.Ports.SerialPort]::GetPortNames()
    if (-not $ports) { throw 'No COM ports found. Connect the Mega using a USB data cable.' }
    Write-Host "Available ports: $($ports -join ', ')"
    $Port = Read-Host 'Enter the Mega COM port shown in Arduino IDE'
    if ($Port -notin $ports) { throw "Port $Port is not available." }
}

$url = "http://127.0.0.1:$WebPort"
$env:PORT = "$WebPort"
$env:DASHBOARD_HOST = '127.0.0.1'
$server = Start-Process -FilePath $node.Source -ArgumentList 'dashboard-server.cjs' -WorkingDirectory $PSScriptRoot -WindowStyle Hidden -PassThru
try {
    $ready = $false
    for ($attempt = 0; $attempt -lt 20; $attempt++) {
        if ($server.HasExited) { throw "Dashboard server exited. Check whether port $WebPort is in use." }
        try {
            Invoke-WebRequest -Uri "$url/api/telemetry" -UseBasicParsing -TimeoutSec 1 | Out-Null
            if ($server.HasExited) { throw 'Dashboard server exited.' }
            $ready = $true
            break
        } catch {
            Start-Sleep -Milliseconds 250
        }
    }
    if (-not $ready) { throw "Dashboard did not start at $url." }
    Start-Process $url
    Write-Host 'Select Live hardware in the browser.'
    & (Join-Path $PSScriptRoot 'dashboard-serial-bridge.ps1') -Port $Port -Endpoint "$url/api/telemetry"
} finally {
    if (-not $server.HasExited) { Stop-Process -Id $server.Id }
    $server.Dispose()
}
