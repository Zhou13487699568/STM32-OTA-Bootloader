# =============================================================================
#  build.ps1 —— 用 Keil(armcc) 编译本工程，并在 VS Code 里显示错误/体积
#  用法：
#    powershell -NoProfile -ExecutionPolicy Bypass -File scripts\build.ps1
#    powershell ... -File scripts\build.ps1 -Rebuild     # 全量重编
#    powershell ... -File scripts\build.ps1 -Clean       # 只清理产物
# =============================================================================
param(
    [switch]$Rebuild,
    [switch]$Clean
)

. (Join-Path $PSScriptRoot 'tools.ps1')

if (-not $Uv4)   { Write-Host "× 找不到 UV4.exe，请修改 scripts/tools.ps1" -ForegroundColor Red; exit 1 }
if (-not (Test-Path -LiteralPath $Proj)) { Write-Host "× 找不到工程文件 $Proj" -ForegroundColor Red; exit 1 }

# ---------- 只清理 ----------
if ($Clean) {
    if (Test-Path -LiteralPath $OutDir) {
        Get-ChildItem -LiteralPath $OutDir -File -ErrorAction SilentlyContinue |
            Where-Object { $_.Extension -in '.o', '.d', '.crf', '.axf', '.hex', '.map', '.lnp', '.htm', '.lst', '.dep', '.build_log.htm', '.log' } |
            Remove-Item -Force -ErrorAction SilentlyContinue
    }
    Write-Host "√ 已清理构建产物（$OutDir）" -ForegroundColor Green
    exit 0
}

# ---------- 编译 ----------
$mode = if ($Rebuild) { '-r' } else { '-b' }
$modeText = if ($Rebuild) { '全量重编' } else { '增量编译' }
Write-Host "=== Keil $modeText ===" -ForegroundColor Cyan
Write-Host "  工程: $Proj"

if (Test-Path -LiteralPath $BuildLog) { Remove-Item -LiteralPath $BuildLog -Force -ErrorAction SilentlyContinue }

# UV4 是 GUI 子系统程序，普通 & 调用不会等待，必须 Start-Process -Wait
$sw = [Diagnostics.Stopwatch]::StartNew()
$p = Start-Process -FilePath $Uv4 -ArgumentList @($mode, $Proj, '-j0', '-o', $BuildLog) -Wait -PassThru
$sw.Stop()

$logText = ''
if (Test-Path -LiteralPath $BuildLog) { $logText = Get-Content -LiteralPath $BuildLog -Raw -ErrorAction SilentlyContinue }

# 把 armcc 的告警/错误原样打出来（VS Code 的 Problem Matcher 会据此生成可点击条目）
if ($logText) {
    $lines = $logText -split "`r?`n"
    foreach ($ln in $lines) {
        if ($ln -match ':\s*(error|warning)\s' -or $ln -match '^Error:') {
            $color = if ($ln -match 'error|Error') { 'Red' } else { 'Yellow' }
            Write-Host "  $ln" -ForegroundColor $color
        }
    }
}

# ---------- 结果 ----------
# UV4 的退出码：0 = 无错无警告，1 = 只有警告（编译仍然成功），2 = 有错误
$ok = ($p.ExitCode -le 1)
$errCount = ([regex]::Matches($logText, '(\d+) Error\(s\)') | ForEach-Object { [int]$_.Groups[1].Value } | Select-Object -Last 1)
$warnCount = ([regex]::Matches($logText, '(\d+) Warning\(s\)') | ForEach-Object { [int]$_.Groups[1].Value } | Select-Object -Last 1)

if ($ok) {
    Write-Host ("√ 编译通过（耗时 {0:N1}s，{1} warning）" -f $sw.Elapsed.TotalSeconds, [int]$warnCount) -ForegroundColor Green
} else {
    Write-Host ("× 编译失败（{0} error）" -f [int]$errCount) -ForegroundColor Red
}

# ---------- 从 map 里读出 Flash/RAM 占用，顺便守住 20KB 红线 ----------
if (Test-Path -LiteralPath $Map) {
    $mapText = Get-Content -LiteralPath $Map -Raw
    $mRom = [regex]::Match($mapText, 'Total ROM Size[^0-9]*(\d+)')
    $mRw  = [regex]::Match($mapText, 'Total RW  Size[^0-9]*(\d+)')
    $mLim = [regex]::Match($mapText, 'Load Region LR_IROM1[^\n]*Max:\s*0x([0-9a-fA-F]+)')
    if ($mRom.Success) {
        $rom = [int]$mRom.Groups[1].Value
        $rw  = if ($mRw.Success) { [int]$mRw.Groups[1].Value } else { 0 }
        $lim = if ($mLim.Success) { [Convert]::ToInt32($mLim.Groups[1].Value, 16) } else { 20480 }
        $pct = 100.0 * $rom / $lim
        Write-Host ("  Flash: {0} / {1} B ({2:N1}%)   余量 {3} B" -f $rom, $lim, $pct, ($lim - $rom)) -ForegroundColor $(if ($rom -gt $lim) { 'Red' } elseif ($pct -gt 90) { 'Yellow' } else { 'Green' })
        Write-Host ("  RAM  : {0} B / 20480 B" -f $rw) -ForegroundColor DarkGray
    }
    if (Test-Path -LiteralPath $Hex) {
        $hexSize = (Get-Item -LiteralPath $Hex).Length
        Write-Host ("  产物: {0}  ({1:N0} B)" -f (Split-Path $Hex -Leaf), $hexSize) -ForegroundColor DarkGray
    }
}

exit $p.ExitCode
