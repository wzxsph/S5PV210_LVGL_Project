param(
    [string]$SerialPort = "COM6",
    [int]$BaudRate = 115200,
    [int]$Timeout = 30
)

$scriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$targetScript = Join-Path $scriptDir "template-framebuffer-gui\auto_test.ps1"

if (-not (Test-Path $targetScript)) {
    Write-Host "[ERROR] Target script not found: $targetScript" -ForegroundColor Red
    exit 1
}

& $targetScript -SerialPort $SerialPort -BaudRate $BaudRate -Timeout $Timeout
exit $LASTEXITCODE
