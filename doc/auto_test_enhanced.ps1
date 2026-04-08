# requires -RunAsAdministrator
param(
    [string]$SerialPort = "COM6",
    [int]$BaudRate = 115200,
    [int]$Timeout = 30,
    [string]$BinFile = "template-framebuffer-gui.bin",
    [switch]$SkipFirewall,
    [string]$TFTPIP = $null
)

$scriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$tftpScript = Join-Path $scriptDir "tftp_server.py"
$binFilePath = Join-Path $scriptDir "template-framebuffer-gui\output\$BinFile"

Write-Host "========================================" -ForegroundColor Cyan
Write-Host "  S5PV210 LVGL Auto Test (Enhanced)" -ForegroundColor Cyan
Write-Host "========================================" -ForegroundColor Cyan
Write-Host "[INFO] Target binary: $BinFile" -ForegroundColor Gray
Write-Host "[INFO] Time: $(Get-Date -Format 'yyyy-MM-dd HH:mm:ss')" -ForegroundColor Gray

if (-not (Test-Path $binFilePath)) {
    Write-Host "[ERROR] Binary file not found: $binFilePath" -ForegroundColor Red
    Write-Host "[HINT] Run 'make' in template-framebuffer-gui/ first" -ForegroundColor Yellow
    exit 1
}
Write-Host "[OK] Found binary file: $([math]::Round((Get-Item $binFilePath).Length/1KB, 2)) KB" -ForegroundColor Green

if (-not (Get-Command python -ErrorAction SilentlyContinue)) {
    Write-Host "[ERROR] Python not found. Cannot start TFTP server." -ForegroundColor Red
    exit 1
}

# 检查管理员权限
$isAdmin = ([Security.Principal.WindowsPrincipal][Security.Principal.WindowsIdentity]::GetCurrent()).IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)
if (-not $isAdmin) {
    Write-Host "[WARN] Not running as Administrator. This may cause issues with:" -ForegroundColor Yellow
    Write-Host "       - Firewall rules configuration" -ForegroundColor Yellow
    Write-Host "       - Network binding to specific interfaces" -ForegroundColor Yellow
    Write-Host "[HINT] Consider running PowerShell as Administrator for better compatibility" -ForegroundColor Yellow
}

# 配置防火墙规则（如果需要）
if (-not $SkipFirewall) {
    Write-Host "`n[FIREWALL] Checking firewall rules..." -ForegroundColor Yellow
    
    try {
        # 检查是否已存在 TFTP 规则
        $existingRule = Get-NetFirewallRule -DisplayName "S5PV210 TFTP Server" -ErrorAction SilentlyContinue
        
        if (-not $existingRule) {
            Write-Host "[FIREWALL] Creating inbound rule for TFTP (UDP 69)..." -ForegroundColor Yellow
            
            New-NetFirewallRule `
                -DisplayName "S5PV210 TFTP Server" `
                -Direction Inbound `
                -Protocol UDP `
                -LocalPort 69 `
                -Action Allow `
                -Description "Allow TFTP traffic for S5PV210 development" `
                -ErrorAction Stop | Out-Null
                
            Write-Host "[OK] Firewall rule created successfully" -ForegroundColor Green
        }
        else {
            Write-Host "[OK] Firewall rule already exists" -ForegroundColor Green
        }
        
        # 启用规则（如果被禁用）
        Enable-NetFirewallRule -DisplayName "S5PV210 TFTP Server" -ErrorAction SilentlyContinue
        
    }
    catch {
        Write-Host "[WARN] Could not configure firewall: $_" -ForegroundColor Yellow
        Write-Host "[HINT] Manually allow UDP port 69 in Windows Firewall" -ForegroundColor Yellow
    }
}

# 构建 TFTP 服务器参数
$tftpArgs = @($tftpScript)
if ($TFTPIP) {
    $tftpArgs += "--host"
    $tftpArgs += $TFTPIP
}

Write-Host "`n[TFTP] Starting TFTP server..." -ForegroundColor Yellow
if ($TFTPIP) {
    Write-Host "[TFTP] Binding to specified IP: $TFTPIP" -ForegroundColor Gray
} else {
    Write-Host "[TFTP] Auto-detecting best network interface..." -ForegroundColor Gray
}

$tftpProcess = Start-Process -FilePath "python" -ArgumentList $tftpArgs -PassThru -WindowStyle Minimized
Write-Host "[OK] TFTP Server started (PID: $($tftpProcess.Id))" -ForegroundColor Green

# 等待 TFTP 服务器完全初始化
Start-Sleep -Seconds 3

# 显示网络配置摘要
Write-Host "`n[NETWORK] Configuration summary:" -ForegroundColor Cyan
Write-Host "       Serial Port : $SerialPort @ $BaudRate baud" -ForegroundColor Gray
Write-Host "       Binary File : $BinFile ($([math]::Round((Get-Item $binFilePath).Length/1KB, 2)) KB)" -ForegroundColor Gray
if ($TFTPIP) {
    Write-Host "       TFTP Bind   : $TFTPIP:69 (manual)" -ForegroundColor Gray
} else {
    Write-Host "       TFTP Bind   : Auto-detect (see TFTP server output)" -ForegroundColor Gray
}
Write-Host ""

try {
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

    Write-Host "[1] Sending tftp command..." -ForegroundColor Yellow
    $tftpCmd = "tftp 0x30000000 $BinFile"
    Write-Host "     Command: $tftpCmd" -ForegroundColor Gray
    $serial.Write($tftpCmd + [char]13)
    Start-Sleep -Milliseconds 200
    $serial.DiscardInBuffer()
    $buffer = ""

    Write-Host "[2] Waiting for TFTP to complete..." -ForegroundColor Yellow
    Write-Host "     (This may take up to 120 seconds for ~1MB file)" -ForegroundColor Gray
    $tftpDone = $false
    $tftpFailed = $false
    $buffer = ""
    $tftpLoading = $false
    $maxWaitSeconds = 120
    $startTime = Get-Date
    $lastProgressTime = Get-Date

    while ($true) {
        if ($serial.BytesToRead -gt 0) {
            $data = $serial.ReadExisting()
            Write-Host -NoNewline $data
            $buffer += $data
            
            if ($buffer -match "Bytes transferred") {
                $tftpDone = $true
                Write-Host ""
                Write-Host "[OK] TFTP transfer completed successfully!" -ForegroundColor Green
                Start-Sleep -Milliseconds 1500
                break
            }
            
            if ($buffer -match "(TFTP error|Retry count|exceeded|Access violation|Cannot load)") {
                $tftpFailed = $true
                Write-Host ""
                Write-Host "[ERROR] TFTP transfer failed!" -ForegroundColor Red
                break
            }
            
            if ($data -match "Loading:") {
                $tftpLoading = $true
                $lastProgressTime = Get-Date
            }
            
            # 显示进度点
            if ($data -match "^[#\.]+$") {
                # Progress indicators, just continue
            }
        }
        
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

    if (-not $tftpDone) {
        Write-Host ""
        Write-Host "[ABORT] TFTP transfer did not complete." -ForegroundColor Red
        Write-Host "" 
        Write-Host "[DIAGNOSIS] Possible causes:" -ForegroundColor Yellow
        Write-Host "  1. Network cable not connected or loose" -ForegroundColor Gray
        Write-Host "  2. Study210 board not powered on or in U-Boot" -ForegroundColor Gray
        Write-Host "  3. IP address mismatch (board expects different server IP)" -ForegroundColor Gray
        Write-Host "  4. Windows Firewall blocking TFTP traffic" -ForegroundColor Gray
        Write-Host "  5. VPN software interfering with network routing" -ForegroundColor Gray
        Write-Host ""
        Write-Host "[TROUBLESHOOTING]" -ForegroundColor Cyan
        Write-Host "  - Check physical connection (cable, LEDs)" -ForegroundColor Gray
        Write-Host "  - Verify board shows 'x210 #' prompt" -ForegroundColor Gray
        Write-Host "  - Try: ping 192.168.1.10 from this PC" -ForegroundColor Gray
        Write-Host "  - Run script as Administrator" -ForegroundColor Gray
        Write-Host "  - Temporarily disable VPN and retry" -ForegroundColor Gray
        
        $serial.Close()
        
        if ($tftpProcess -and !$tftpProcess.HasExited) {
            Stop-Process -Id $tftpProcess.Id -Force -ErrorAction SilentlyContinue
            Write-Host "[INFO] TFTP server stopped" -ForegroundColor Yellow
        }
        
        exit 1
    }

    Write-Host ""
    Write-Host "[3] Sending go command..." -ForegroundColor Yellow
    $serial.Write("go 0x30000000" + [char]13)

    Write-Host ""
    Write-Host "[OUTPUT] Program execution output:" -ForegroundColor Yellow
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
            Write-Host "[INFO] Output capture timeout after ${Timeout}s (normal)" -ForegroundColor Yellow
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
    Write-Host "[DONE] Test completed at $(Get-Date -Format 'HH:mm:ss')" -ForegroundColor Cyan
    
    # 统计信息
    $totalTime = ((Get-Date) - $startTime).TotalSeconds
    Write-Host "[STATS] Total test duration: $([math]::Round($totalTime, 1))s" -ForegroundColor Gray
    
}
catch {
    Write-Host "[ERROR] Serial port error: $_" -ForegroundColor Red
    Write-Host "[HINT] Check that $SerialPort is available and not in use" -ForegroundColor Yellow
    
    if ($serial.IsOpen) { $serial.Close() }
    if ($tftpProcess -and !$tftpProcess.HasExited) { Stop-Process -Id $tftpProcess.Id -Force -ErrorAction SilentlyContinue }
    
    exit 1
}
