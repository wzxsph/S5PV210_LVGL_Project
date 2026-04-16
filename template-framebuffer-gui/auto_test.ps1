param(
    [string]$SerialPort = "COM6",
    [int]$BaudRate = 115200,
    [int]$Timeout = 180
)

$scriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$tftpScript = Join-Path $scriptDir "tftp_server.py"

Write-Host "========================================" -ForegroundColor Cyan
Write-Host "  S5PV210 LVGL Auto Test" -ForegroundColor Cyan
Write-Host "========================================" -ForegroundColor Cyan

$tftpProcess = $null

if (Get-Command python -ErrorAction SilentlyContinue) {
    Write-Host "[INFO] Starting TFTP server..." -ForegroundColor Yellow
    $tftpProcess = Start-Process -FilePath "python" -ArgumentList $tftpScript -PassThru -WindowStyle Minimized
    Write-Host "[OK] TFTP Server started (PID: $($tftpProcess.Id))" -ForegroundColor Green
    Start-Sleep -Seconds 2
}
else {
    Write-Host "[ERROR] Python not found. Cannot start TFTP server." -ForegroundColor Red
    exit 1
}

$serial = New-Object System.IO.Ports.SerialPort $SerialPort, $BaudRate, None, 8, one
$serial.ReadTimeout = 5000
$serial.WriteTimeout = 5000
$serial.Open()
Start-Sleep -Milliseconds 500
$serial.DiscardInBuffer()

Write-Host "[OK] Connected to $SerialPort" -ForegroundColor Green
Write-Host ""

Write-Host "[0] Sending newline to get prompt..." -ForegroundColor Yellow
$serial.Write([char]13)
Start-Sleep -Milliseconds 800

# First, let's see what's happening on the serial port
Write-Host "[DEBUG] Reading initial serial output..." -ForegroundColor Cyan
Start-Sleep -Milliseconds 2000
$initialOutput = $serial.ReadExisting()
Write-Host "[DEBUG] Initial output: '$initialOutput'" -ForegroundColor Cyan

# Try to get to U-Boot prompt
Write-Host "[DEBUG] Trying to enter U-Boot..." -ForegroundColor Cyan
for ($i=0; $i -lt 5; $i++) {
    $serial.Write(" " + [char]13)
    Start-Sleep -Milliseconds 200
}
Start-Sleep -Milliseconds 1000
$ubootOutput = $serial.ReadExisting()
Write-Host "[DEBUG] U-Boot prompt attempt: '$ubootOutput'" -ForegroundColor Cyan

Write-Host "[1] Sending tftp command..." -ForegroundColor Yellow
$tftpCommand = "tftp 0x30000000 template-framebuffer-gui.bin"
Write-Host "[DEBUG] Command: $tftpCommand" -ForegroundColor Cyan
$serial.Write($tftpCommand + [char]13)
Start-Sleep -Milliseconds 200

# Don't clear buffer immediately, let's see what's returned
Write-Host "[DEBUG] Reading response..." -ForegroundColor Cyan
Start-Sleep -Milliseconds 1000
$response = $serial.ReadExisting()
Write-Host "[DEBUG] Response: '$response'" -ForegroundColor Cyan

$buffer = ""  # Reset buffer AFTER sending tftp command

Write-Host "[2] Waiting for TFTP to complete..." -ForegroundColor Yellow
$tftpDone = $false
$tftpFailed = $false
$tftpStartTime = Get-Date
$timeoutSeconds = 120
$maxWaitSeconds = $timeoutSeconds
$startTime = Get-Date
$lastProgressTime = Get-Date

while ($true) {
    if ($serial.BytesToRead -gt 0) {
        $data = $serial.ReadExisting()
        Write-Host -NoNewline $data
        $buffer += $data
        
        # Check for SUCCESS: U-Boot prints "Bytes transferred = XXXXX"
        if ($buffer -match "Bytes transferred") {
            $tftpDone = $true
            Write-Host ""
            Write-Host "[OK] TFTP transfer completed successfully!" -ForegroundColor Green
            Start-Sleep -Milliseconds 1500
            break
        }
        
        # Check for FAILURE: Only match specific TFTP error messages from U-Boot
        if ($buffer -match "(TFTP error|Retry count|exceeded|Access violation|Cannot load)") {
            $tftpFailed = $true
            Write-Host ""
            Write-Host "[ERROR] TFTP transfer failed (U-Boot reported error)!" -ForegroundColor Red
            break
        }
        
        # Detect TFTP is actively loading (progress indicator)
        if ($data -match "Loading:") {
            $tftpLoading = $true
            $lastProgressTime = Get-Date
        }
    }
    
    # Only check for prompt return if TFTP never started loading
    if (-not $tftpLoading) {
        if ($buffer -match "x210 #" -and $buffer.Length -gt 30) {
            if (-not ($buffer -match "Bytes transferred")) {
                $tftpFailed = $true
                Write-Host ""
                Write-Host "[ERROR] TFTP returned to prompt without completing!" -ForegroundColor Red
                break
            }
        }
    }
    
    $elapsed = (Get-Date) - $startTime
    if ($elapsed.TotalSeconds -gt $maxWaitSeconds) {
        Write-Host ""
        Write-Host "[ERROR] TFTP timeout after $($maxWaitSeconds)s" -ForegroundColor Red
        $tftpFailed = $true
        break
    }
    
    Start-Sleep -Milliseconds 200
}

# If TFTP failed or didn't complete, abort the test
if (-not $tftpDone) {
    Write-Host ""
    Write-Host "[ABORT] TFTP transfer did not complete. Aborting test (go command NOT sent)." -ForegroundColor Red
    
    $serial.Close()
    
    if ($tftpProcess -and !$tftpProcess.HasExited) {
        Stop-Process -Id $tftpProcess.Id -Force -ErrorAction SilentlyContinue
        Write-Host "[INFO] TFTP server stopped" -ForegroundColor Yellow
    }
    
    exit 1
}

# Only proceed with go command if TFTP was successful
Write-Host ""
Write-Host "[3] Sending go command..." -ForegroundColor Yellow
$serial.Write("go 0x30000000" + [char]13)

Write-Host ""
Write-Host "[OUTPUT] Waiting for response..." -ForegroundColor Yellow
Write-Host "----------------------------------------" -ForegroundColor Gray

$outputStartTime = Get-Date
$outputBuffer = ""

while ($true) {
    if ($serial.BytesToRead -gt 0) {
        $data = $serial.ReadExisting()
        Write-Host -NoNewline $data
        $outputBuffer += $data
    }
    
    Start-Sleep -Milliseconds 50
    
    $elapsed = (Get-Date) - $outputStartTime
    if ($elapsed.TotalSeconds -gt $Timeout) {
        Write-Host ""
        Write-Host "[WARN] Output timeout after ${Timeout}s" -ForegroundColor Yellow
        break
    }
}

$serial.Close()

if ($tftpProcess -and !$tftpProcess.HasExited) {
    Stop-Process -Id $tftpProcess.Id -Force -ErrorAction SilentlyContinue
    Write-Host "[OK] TFTP server stopped" -ForegroundColor Green
}

Write-Host ""
Write-Host "----------------------------------------" -ForegroundColor Gray
Write-Host "[DONE] Test complete" -ForegroundColor Cyan
