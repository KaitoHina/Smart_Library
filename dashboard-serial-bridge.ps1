param(
    [Parameter(Mandatory = $true)]
    [string]$Port,
    [uri]$Endpoint = 'http://127.0.0.1:8080/api/telemetry'
)

$serial = [System.IO.Ports.SerialPort]::new($Port, 115200)
$serial.ReadTimeout = 1000
$serial.DtrEnable = $true

try {
    $serial.Open()
    Write-Host "Forwarding $Port to $Endpoint. Press Ctrl+C to stop."
    while ($true) {
        try {
            $line = $serial.ReadLine().Trim()
        } catch [System.TimeoutException] {
            continue
        }
        if (-not $line.StartsWith('{')) { continue }
        try {
            Invoke-RestMethod -Uri $Endpoint -Method Post -ContentType 'application/json' -Body $line -TimeoutSec 2 -ErrorAction Stop | Out-Null
        } catch {
            Write-Warning "Dashboard POST failed. $($_.Exception.Message)"
            $serial.DiscardInBuffer()
        }
    }
} finally {
    $serial.Dispose()
}
