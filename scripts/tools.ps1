# =============================================================================
#  tools.ps1 —— 本工程所有工具路径的唯一来源
#  其它脚本（build.ps1 / flash.ps1）都 dot-source 这个文件。
#  如果哪台机器上工具换了位置，只需要改这里，不用改 .vscode/tasks.json。
# =============================================================================

$ErrorActionPreference = 'Stop'

function Find-FirstExisting {
    param([string[]]$Paths)
    foreach ($p in $Paths) {
        if ($p -and (Test-Path -LiteralPath $p)) { return $p }
    }
    return $null
}

# ---------- 工程路径 ----------
$script:Root     = Split-Path -Parent $PSScriptRoot                    # 工程根目录
$script:Proj     = Join-Path $Root 'MDK-ARM\bootloder.uvprojx'         # Keil 工程
$script:OutDir   = Join-Path $Root 'MDK-ARM\bootloder'                 # 构建输出目录
$script:Hex      = Join-Path $OutDir 'bootloder.hex'
$script:Axf      = Join-Path $OutDir 'bootloder.axf'
$script:Map      = Join-Path $OutDir 'bootloder.map'
$script:BuildLog = Join-Path $OutDir 'build_vscode.log'

# ---------- Keil uVision（编译，GUI 程序，必须用 Start-Process -Wait 等它）----------
$script:Uv4 = Find-FirstExisting @(
    'D:\BaiduNetdiskDownload\yehuoxiaozhi_hal\keil5-path\core\UV4\UV4.exe',
    'C:\Keil_v5\UV4\UV4.exe',
    'D:\Keil_v5\UV4\UV4.exe',
    'C:\Keil\UV4\UV4.exe'
)
if (-not $Uv4) { $script:Uv4 = (Get-Command 'UV4.exe' -ErrorAction SilentlyContinue).Source }

# ---------- STM32CubeProgrammer CLI（烧写，驱动 ST-Link）----------
$script:CubeProg = Find-FirstExisting @(
    'D:\BaiduNetdiskDownload\freertos_cubide\software\STM32CubeIDE_1.19.0\STM32CubeIDE\plugins\com.st.stm32cube.ide.mcu.externaltools.cubeprogrammer.win32_2.2.200.202503041107\tools\bin\STM32_Programmer_CLI.exe',
    'C:\Program Files\STMicroelectronics\STM32Cube\STM32CubeProgrammer\bin\STM32_Programmer_CLI.exe',
    'C:\Program Files (x86)\STMicroelectronics\STM32Cube\STM32CubeProgrammer\bin\STM32_Programmer_CLI.exe'
)
if (-not $CubeProg) { $script:CubeProg = (Get-Command 'STM32_Programmer_CLI.exe' -ErrorAction SilentlyContinue).Source }

# ---------- OpenOCD + arm-none-eabi-gdb（VS Code 调试）----------
$script:OpenOcd = Find-FirstExisting @(
    'D:\BaiduNetdiskDownload\freertos_cubide\software\STM32CubeIDE_1.19.0\STM32CubeIDE\plugins\com.st.stm32cube.ide.mcu.externaltools.openocd.win32_2.4.200.202505051030\tools\bin\openocd.exe',
    'C:\Program Files\OpenOCD\bin\openocd.exe'
)
if (-not $OpenOcd) { $script:OpenOcd = (Get-Command 'openocd.exe' -ErrorAction SilentlyContinue).Source }

# OpenOCD 的配置文件目录（interface/target 都在这里）
$script:OpenOcdScripts = Find-FirstExisting @(
    'D:\BaiduNetdiskDownload\freertos_cubide\software\STM32CubeIDE_1.19.0\STM32CubeIDE\plugins\com.st.stm32cube.ide.mcu.debug.openocd_2.3.100.202501240831\resources\openocd\st_scripts'
)
if (-not $OpenOcdScripts -and $OpenOcd) {
    $cand = Join-Path (Split-Path (Split-Path $OpenOcd)) 'share\openocd\scripts'
    if (Test-Path -LiteralPath $cand) { $script:OpenOcdScripts = $cand }
}

$script:Gdb = Find-FirstExisting @(
    'D:\BaiduNetdiskDownload\freertos_cubide\software\STM32CubeIDE_1.19.0\STM32CubeIDE\plugins\com.st.stm32cube.ide.mcu.externaltools.gnu-tools-for-stm32.13.3.rel1.win32_1.0.0.202411081344\tools\bin\arm-none-eabi-gdb.exe',
    'C:\Program Files (x86)\Arm GNU Toolchain arm-none-eabi\bin\arm-none-eabi-gdb.exe'
)
if (-not $Gdb) { $script:Gdb = (Get-Command 'arm-none-eabi-gdb.exe' -ErrorAction SilentlyContinue).Source }

# 外设寄存器描述文件（VS Code 调试时能看到 RCC/GPIO 等寄存器）
$script:Svd = Find-FirstExisting @(
    'D:\BaiduNetdiskDownload\yehuoxiaozhi_hal\keil5-path\pack\Keil\STM32F1xx_DFP\2.3.0\SVD\STM32F103xx.svd'
)

# ---------- Git ----------
$script:Git = (Get-Command 'git.exe' -ErrorAction SilentlyContinue).Source
if (-not $Git) { $script:Git = Find-FirstExisting @('D:\BaiduNetdiskDownload\ai\git\Git\cmd\git.exe') }

# ---------- 自检输出（脚本跑起来时能一眼看出缺什么）----------
function Show-ToolStatus {
    $rows = @(
        @{ n = 'UV4 (编译)';          v = $Uv4 }
        @{ n = 'CubeProgrammer (烧写)'; v = $CubeProg }
        @{ n = 'OpenOCD (调试)';       v = $OpenOcd }
        @{ n = 'OpenOCD 配置文件';     v = $OpenOcdScripts }
        @{ n = 'arm-none-eabi-gdb';    v = $Gdb }
        @{ n = 'SVD (寄存器视图)';     v = $Svd }
        @{ n = 'git';                  v = $Git }
    )
    foreach ($r in $rows) {
        $mark = if ($r.v) { 'OK  ' } else { '缺失' }
        Write-Host ("  [{0}] {1,-22} {2}" -f $mark, $r.n, $r.v) -ForegroundColor $(if ($r.v) { 'DarkGray' } else { 'Red' })
    }
}
