param(
    [string]$SerialPort = "COM6",
    [int]$BaudRate = 115200
)

Write-Host "========================================" -ForegroundColor Cyan
Write-Host "  Serial Port Test" -ForegroundColor Cyan
Write-Host "========================================" -ForegroundColor Cyan

$serial = New-Object System.IO.Ports.SerialPort $SerialPort, $BaudRate, None, 8, one
$serial.ReadTimeout = 2000
$serial.WriteTimeout = 2000

try {
    $serial.Open()
    Write-Host "[OK] Connected to $SerialPort" -ForegroundColor Green
    
    Write-Host "[INFO] Reading any existing data..." -ForegroundColor Yellow
    Start-Sleep -Seconds 2
    $data = $serial.ReadExisting()
    if ($data) {
        Write-Host "[INFO] Received: '$data'" -ForegroundColor Green
    } else {
        Write-Host "[INFO] No data received" -ForegroundColor Yellow
    }
    
    Write-Host "[INFO] Sending 'help' command..." -ForegroundColor Yellow
    $serial.Write("help" + [char]13)
    Start-Sleep -Seconds 1
    $data = $serial.ReadExisting()
    if ($data) {
        Write-Host "[INFO] Received: '$data'" -ForegroundColor Green
    } else {
        Write-Host "[INFO] No response" -ForegroundColor Yellow
    }
    
    Write-Host "[INFO] Sending newline..." -ForegroundColor Yellow
    $serial.Write([char]13)
    Start-Sleep -Seconds 1
    $data = $serial.ReadExisting()
    if ($data) {
        Write-Host "[INFO] Received: '$data'" -ForegroundColor Green
    } else {
        Write-Host "[INFO] No response" -ForegroundColor Yellow
    }
    
} catch {
    Write-Host "[ERROR] $($_.Exception.Message)" -ForegroundColor Red
} finally {
    if ($serial.IsOpen) {
        $serial.Close()
        Write-Host "[INFO] Serial port closed" -ForegroundColor Yellow
    }
}

Write-Host "========================================" -ForegroundColor Cyan
Write-Host "  Test completed" -ForegroundColor Cyan
Write-Host "========================================" -ForegroundColor Cyan
