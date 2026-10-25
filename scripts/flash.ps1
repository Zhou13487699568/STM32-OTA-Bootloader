# =============================================================================
#  flash.ps1 —— 用 ST-Link 烧写 bootloader
#  默认走 STM32CubeProgrammer CLI（最稳），可选 -OpenOcd 走 OpenOCD。
#  只烧 .hex 覆盖到的区域（0x08000000~），不会动 A 区；要全片擦除加 -Erase。
#  用法：
#    powershell -NoProfile -ExecutionPolicy Bypass -File scripts\flash.ps1
#    powershell ... -File scripts\flash.ps1 -Erase
#    powershell ... -File scripts\flash.ps1 -OpenOcd
# =============================================================================
param(
    [switch]$OpenOcd,
    [switch]$Erase,
    [string]$HexPath
)

. (Join-Path $PSScriptRoot 'tools.ps1')

$hex = if ($HexPath) { $HexPath } else { $Hex }
if (-not (Test-Path -LiteralPath $hex)) {
    Write-Host "× 找不到固件：$hex" -ForegroundColor Red
    Write-Host "  请先编译（任务 build / rebuild）" -ForegroundColor Yellow
    exit 1
}
Write-Host ("=== 烧写 {0} ===" -f (Split-Path $hex -Leaf)) -ForegroundColor Cyan
Write-Host ("  文件大小: {0:N0} B" -f (Get-Item -LiteralPath $hex).Length)

if ($OpenOcd) {
    # ---------------- OpenOCD 方式 ----------------
    if (-not $OpenOcd -or -not $OpenOcdScripts) { Write-Host "× 找不到 OpenOCD 或其配置文件" -ForegroundColor Red; exit 1 }
    $fwd = $hex -replace '\\', '/'
    $cmds = @(
        'program "{0}" verify reset exit' -f $fwd
    )
    if ($Erase) { $cmds = @('reset halt', 'stm32f1x mass_erase 0') + $cmds }
    Write-Host "  OpenOCD: $OpenOcd" -ForegroundColor DarkGray
    & $OpenOcd -s $OpenOcdScripts -f interface/stlink.cfg -f target/stm32f1x.cfg -c ($cmds -join '; ')
    exit $LASTEXITCODE
}

# ---------------- STM32CubeProgrammer 方式 ----------------
if (-not $CubeProg) { Write-Host "× 找不到 STM32_Programmer_CLI.exe" -ForegroundColor Red; exit 1 }
Write-Host "  CubeProgrammer: $CubeProg" -ForegroundColor DarkGray

$cliArgs = @('-c', 'port=SWD')
if ($Erase) { $cliArgs += @('-e', 'all') }
$cliArgs += @('-w', $hex, '-v', '-rst')

& $CubeProg @cliArgs
$code = $LASTEXITCODE
if ($code -eq 0) {
    Write-Host "√ 烧写完成并已复位" -ForegroundColor Green
} else {
    Write-Host "× 烧写失败（退出码 $code）" -ForegroundColor Red
    Write-Host "  检查：ST-Link 是否插好、杜邦线 SWDIO/SWCLK/GND/3V3、目标板是否上电" -ForegroundColor Yellow
}
exit $code
