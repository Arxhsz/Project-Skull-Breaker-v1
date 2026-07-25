[CmdletBinding()]
param(
    [string]$Port = "",
    [switch]$UploadFilesystem
)

$ErrorActionPreference = "Stop"
Set-StrictMode -Version Latest

$branch = "fix/radio-lifecycle-ui-flicker"
$archiveUrl = "https://github.com/Arxhsz/Project-Skull-Breaker-v1/archive/refs/heads/$branch.zip"
$documents = [Environment]::GetFolderPath("MyDocuments")
$destination = Join-Path $documents "Project-Skull-Breaker-radio-test"
$tempRoot = Join-Path ([System.IO.Path]::GetTempPath()) ("skullbreaker-" + [Guid]::NewGuid().ToString("N"))
$archivePath = Join-Path $tempRoot "firmware.zip"
$extractPath = Join-Path $tempRoot "extract"

function Invoke-PlatformIO {
    param(
        [Parameter(Mandatory = $true)]
        [string[]]$PlatformIOArguments
    )

    $knownPlatformIO = Join-Path $env:USERPROFILE ".platformio\penv\Scripts\platformio.exe"
    if (Test-Path -LiteralPath $knownPlatformIO) {
        & $knownPlatformIO @PlatformIOArguments
        if ($LASTEXITCODE -ne 0) {
            throw "PlatformIO failed with exit code $LASTEXITCODE."
        }
        return
    }

    $pio = Get-Command platformio -ErrorAction SilentlyContinue
    if ($null -eq $pio) {
        $pio = Get-Command pio -ErrorAction SilentlyContinue
    }
    if ($null -ne $pio) {
        & $pio.Source @PlatformIOArguments
        if ($LASTEXITCODE -ne 0) {
            throw "PlatformIO failed with exit code $LASTEXITCODE."
        }
        return
    }

    $py = Get-Command py -ErrorAction SilentlyContinue
    if ($null -eq $py) {
        $py = Get-Command python -ErrorAction SilentlyContinue
    }
    if ($null -eq $py) {
        throw "Python and PlatformIO were not found. Install Python 3, then run this command again."
    }

    Write-Host "PlatformIO was not found. Installing it for this Windows account..." -ForegroundColor Yellow
    & $py.Source -m pip install --user --upgrade platformio
    if ($LASTEXITCODE -ne 0) {
        throw "PlatformIO installation failed."
    }

    & $py.Source -m platformio @PlatformIOArguments
    if ($LASTEXITCODE -ne 0) {
        throw "PlatformIO failed with exit code $LASTEXITCODE."
    }
}

function Resolve-UploadPort {
    param([string]$RequestedPort)

    if (-not [string]::IsNullOrWhiteSpace($RequestedPort)) {
        return $RequestedPort.ToUpperInvariant()
    }

    $ports = @([System.IO.Ports.SerialPort]::GetPortNames() | Sort-Object)
    if ($ports.Count -eq 0) {
        throw "No COM ports were found. Connect the ESP32 by USB and run the command again."
    }
    if ($ports.Count -eq 1) {
        return $ports[0]
    }

    Write-Host "More than one COM port is available:" -ForegroundColor Yellow
    for ($i = 0; $i -lt $ports.Count; $i++) {
        Write-Host ("  [{0}] {1}" -f ($i + 1), $ports[$i])
    }

    while ($true) {
        $selection = Read-Host "Enter the number for the ESP32"
        $number = 0
        if ([int]::TryParse($selection, [ref]$number) -and $number -ge 1 -and $number -le $ports.Count) {
            return $ports[$number - 1]
        }
        Write-Host "Choose a number from 1 to $($ports.Count)." -ForegroundColor Yellow
    }
}

try {
    New-Item -ItemType Directory -Force -Path $tempRoot, $extractPath | Out-Null

    Write-Host "Downloading the radio-test firmware..." -ForegroundColor Cyan
    Invoke-WebRequest -UseBasicParsing -Uri $archiveUrl -OutFile $archivePath
    Expand-Archive -LiteralPath $archivePath -DestinationPath $extractPath -Force

    $source = Get-ChildItem -Path $extractPath -Directory | Select-Object -First 1
    if ($null -eq $source -or -not (Test-Path (Join-Path $source.FullName "platformio.ini"))) {
        throw "The downloaded archive did not contain a valid PlatformIO project."
    }

    if (Test-Path $destination) {
        $currentPath = [System.IO.Path]::GetFullPath((Get-Location).ProviderPath).TrimEnd('\')
        $destinationPath = [System.IO.Path]::GetFullPath($destination).TrimEnd('\')
        if ($currentPath.Equals($destinationPath, [System.StringComparison]::OrdinalIgnoreCase) -or
            $currentPath.StartsWith($destinationPath + '\', [System.StringComparison]::OrdinalIgnoreCase)) {
            Write-Host "Leaving the active project folder before creating its backup..." -ForegroundColor Yellow
            Set-Location $documents
        }

        $stamp = Get-Date -Format "yyyyMMdd-HHmmss"
        $backup = "$destination-backup-$stamp"
        Write-Host "Moving the previous folder to $backup" -ForegroundColor Yellow
        Move-Item -LiteralPath $destination -Destination $backup
    }

    New-Item -ItemType Directory -Force -Path $destination | Out-Null
    Copy-Item -Path (Join-Path $source.FullName "*") -Destination $destination -Recurse -Force
    Set-Location $destination

    $resolvedPort = Resolve-UploadPort -RequestedPort $Port
    Write-Host "Project folder: $destination" -ForegroundColor Green
    Write-Host "Upload port: $resolvedPort" -ForegroundColor Green

    Write-Host "Building firmware..." -ForegroundColor Cyan
    Invoke-PlatformIO -PlatformIOArguments @("run", "-e", "esp32dev")

    if ($UploadFilesystem) {
        Write-Host "Uploading LittleFS assets..." -ForegroundColor Cyan
        Invoke-PlatformIO -PlatformIOArguments @("run", "-e", "esp32dev", "--target", "uploadfs", "--upload-port", $resolvedPort)
    }

    Write-Host "Uploading firmware..." -ForegroundColor Cyan
    Invoke-PlatformIO -PlatformIOArguments @("run", "-e", "esp32dev", "--target", "upload", "--upload-port", $resolvedPort)

    Write-Host "Firmware upload completed successfully." -ForegroundColor Green
    Write-Host "If a future upload pauses at 'Connecting...', hold BOOT until writing starts." -ForegroundColor Yellow
}
finally {
    if (Test-Path $tempRoot) {
        Remove-Item -LiteralPath $tempRoot -Recurse -Force -ErrorAction SilentlyContinue
    }
}
