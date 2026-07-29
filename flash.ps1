param(
    [Parameter(Position=0)][string]$Action,
    [Parameter(Position=1)][string]$Project,
    [string]$Port,
    [switch]$Monitor,
    [switch]$List,
    [switch]$Probe
)

$ROOT = $PSScriptRoot
$IDF_PATH = "D:\espidf\.espressif\v6.0.2\esp-idf"
$exportCmd = "`"$IDF_PATH\export.bat`" >nul 2>&1"

if ($Project) { cd (Join-Path $ROOT $Project) }
elseif ($Action -and (Test-Path (Join-Path $ROOT $Action))) { cd (Join-Path $ROOT $Action); $Action = $null }

# Enumerate all serial ports
function Get-SerialPorts {
    # mode command is most reliable on Windows
    $ports = @()
    cmd /c "mode" 2>&1 | ForEach-Object { if ($_ -match "^Status for device (COM\d+):") { $ports += $Matches[1] } }
    if ($ports) { return $ports }
    try { return [System.IO.Ports.SerialPort]::GetPortNames() } catch {}
    try { return Get-WmiObject Win32_SerialPort 2>$null | ForEach-Object { $_.DeviceID } } catch {}
    return @()
}

# Probe a single port with esptool
function Test-EspPort($p) {
    $result = cmd /c "$exportCmd && esptool.py chip_id --port $p" 2>&1
    $chip = if ($result -match "Chip type:\s+(\S+)") { $Matches[1] } else { $null }
    $mac  = if ($result -match "MAC:\s+(\S+)") { $Matches[1] } else { $null }
    return @{ Chip = $chip; Mac = $mac }
}

# Handle --list / list / --probe / probe as actions
if ($List -or $Action -eq "list") {
    $allPorts = Get-SerialPorts
    if (-not $allPorts) { Write-Host "No serial ports found."; exit 0 }
    Write-Host "Serial ports:" -ForegroundColor Cyan
    foreach ($p in $allPorts) { Write-Host "  $p" }
    exit 0
}

if ($Probe -or $Action -eq "probe") {
    $allPorts = Get-SerialPorts
    if (-not $allPorts) { Write-Host "No serial ports found."; exit 1 }
    Write-Host "Probing ports for ESP devices..." -ForegroundColor Cyan
    $found = @()
    foreach ($p in $allPorts) {
        Write-Host -NoNewline "  $p ... " -ForegroundColor DarkGray
        $info = Test-EspPort $p
        if ($info.Chip) {
            Write-Host "$($info.Chip) ($($info.Mac))" -ForegroundColor Green
            $found += $p
        } else {
            Write-Host "not ESP" -ForegroundColor DarkGray
        }
    }
    if ($found.Count -eq 0) { Write-Host "`nNo ESP devices found." -ForegroundColor Red; exit 1 }
    Write-Host "`nFound $($found.Count) ESP device(s): $($found -join ', ')" -ForegroundColor Cyan
    exit 0
}

# Determine what to do
if ($Monitor) { $action = "flash monitor"; $needsPort = $true }
elseif ($Action -eq "monitor") { $action = "monitor"; $needsPort = $true }
elseif ($Action -eq "build") { $action = "build"; $needsPort = $false }
elseif ($Action -eq "flash") { $action = "flash"; $needsPort = $true }
elseif ($Action) { $action = $Action; $needsPort = $false }
else { $action = "flash"; $needsPort = $true }

# Auto-detect port by probing all serial ports with esptool
if ($needsPort -and -not $Port) {
    $allPorts = Get-SerialPorts
    $espPorts = @()
    foreach ($p in $allPorts) {
        $info = Test-EspPort $p
        if ($info.Chip) { $espPorts += $p }
    }
    if ($espPorts.Count -eq 0) { Write-Error "No ESP device found. Try 'probe' to scan all ports, or '-p COMxx'."; exit 1 }
    elseif ($espPorts.Count -eq 1) { $Port = $espPorts[0] }
    else { Write-Host "Multiple ESP devices: $($espPorts -join ', ')`nUse -p COMxx to specify."; exit 1 }
}

if ($needsPort) {
    cmd /c "$exportCmd && idf.py -p $Port $action"
} else {
    cmd /c "$exportCmd && idf.py $action"
}
