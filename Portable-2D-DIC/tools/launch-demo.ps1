$ErrorActionPreference = "Stop"

$projectRoot = Split-Path -Parent $PSScriptRoot
$buildRoot = Join-Path $projectRoot "out/build/windows-studio-release"
$edgeExe = Join-Path $buildRoot "dic_edge.exe"
$studioExe = Join-Path $buildRoot "DIC Studio.exe"
$demoConfig = Join-Path $projectRoot "config/dic-edge.demo.conf"

foreach ($requiredPath in @($edgeExe, $studioExe, $demoConfig)) {
    if (-not (Test-Path -LiteralPath $requiredPath)) {
        throw "Required demo file is missing: $requiredPath"
    }
}

$resolvedEdge = (Resolve-Path -LiteralPath $edgeExe).Path
$allEdgeProcesses = @(Get-Process -Name "dic_edge" -ErrorAction SilentlyContinue)
$foreignEdgeProcesses = @($allEdgeProcesses |
    Where-Object { $_.Path -and $_.Path -ne $resolvedEdge })

if ($foreignEdgeProcesses.Count -gt 0) {
    $details = ($foreignEdgeProcesses | ForEach-Object { "PID $($_.Id): $($_.Path)" }) -join [Environment]::NewLine
    Add-Type -AssemblyName PresentationFramework
    [System.Windows.MessageBox]::Show(
        "检测到另一个 DIC Edge 正在运行。为防止 Studio 连接到错误的模拟数据，请先停止以下进程：`n`n$details",
        "DIC Edge 端口冲突",
        [System.Windows.MessageBoxButton]::OK,
        [System.Windows.MessageBoxImage]::Warning) | Out-Null
    throw "Another DIC Edge instance is already running: $details"
}

$edgeProcess = $allEdgeProcesses |
    Where-Object { $_.Path -eq $resolvedEdge } |
    Select-Object -First 1

if (-not $edgeProcess) {
    $edgeProcess = Start-Process -FilePath $edgeExe `
        -ArgumentList @("--config", $demoConfig) `
        -WorkingDirectory $projectRoot `
        -WindowStyle Hidden `
        -PassThru
    Write-Host "DIC Edge demo started (PID $($edgeProcess.Id))."
} else {
    Write-Host "Reusing DIC Edge demo (PID $($edgeProcess.Id))."
}

$ready = $false
for ($attempt = 0; $attempt -lt 30; $attempt++) {
    if ($edgeProcess.HasExited) {
        throw "DIC Edge exited during startup. Check that ports 17840-17842 are not already in use."
    }
    $client = [System.Net.Sockets.TcpClient]::new()
    try {
        $connection = $client.ConnectAsync("127.0.0.1", 17840)
        if ($connection.Wait(200) -and $client.Connected) {
            $ready = $true
            break
        }
    } catch {
        # Edge may still be initializing; retry below.
    } finally {
        $client.Dispose()
    }
    Start-Sleep -Milliseconds 100
}

if (-not $ready) {
    throw "DIC Edge did not open control port 17840 within 9 seconds."
}

Start-Process -FilePath $studioExe -WorkingDirectory $buildRoot
Write-Host "DIC Studio opened. In Project Center choose Quick Demo."
