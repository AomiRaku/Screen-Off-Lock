#Requires -Version 7.0
<#
    ScreenOffLock —— 构建脚本

    在没有编译器的机器上，会自动把便携版 w64devkit 下载并解压到仓库内的
    .toolchain/ 目录，然后用其中的 g++ 编译。全程不需要管理员权限，也不写入
    任何系统目录；删掉 .toolchain 即可彻底还原。

    用法：
        pwsh -File build.ps1              正常构建
        pwsh -File build.ps1 -Clean       先清掉 dist 再构建
        pwsh -File build.ps1 -Reinstall   连工具链一起重新下载
#>

[CmdletBinding()]
param(
    [switch]$Clean,
    [switch]$Reinstall
)

$ErrorActionPreference = 'Stop'

$Root         = $PSScriptRoot
$ToolchainDir = Join-Path $Root '.toolchain'
$DistDir      = Join-Path $Root 'dist'
$SourceFile   = Join-Path $Root 'src\main.cpp'
$OutputExe    = Join-Path $DistDir 'ScreenOffLock.exe'
$SfxName      = 'w64devkit-x64-2.10.0.7z.exe'

function Write-Step($msg) { Write-Host "[*] $msg" -ForegroundColor Cyan }
function Write-Ok($msg)   { Write-Host "[+] $msg" -ForegroundColor Green }

if ($Clean -and (Test-Path $DistDir)) {
    Write-Step '清理 dist'
    Remove-Item $DistDir -Recurse -Force
}

if ($Reinstall -and (Test-Path $ToolchainDir)) {
    Write-Step '清理 .toolchain'
    Remove-Item $ToolchainDir -Recurse -Force
}

function Find-Gxx {
    Get-ChildItem -Path $ToolchainDir -Filter 'g++.exe' -Recurse -ErrorAction SilentlyContinue |
        Select-Object -First 1
}

# --- 1. 准备工具链 ---------------------------------------------------------

$gxx = Find-Gxx

if (-not $gxx) {
    Write-Step '未找到 g++，准备下载便携工具链 w64devkit'

    $python = Get-Command python -ErrorAction SilentlyContinue
    if (-not $python) {
        throw '需要 Python 来下载工具链；或者自行安装 MinGW-w64 并把 g++.exe 放进 PATH。'
    }

    & $python.Source (Join-Path $Root 'scripts\fetch-toolchain.py')
    if ($LASTEXITCODE -ne 0) { throw '工具链下载失败。' }

    $sfx = Join-Path $ToolchainDir $SfxName
    if (-not (Test-Path $sfx)) { throw "找不到下载好的自解压包：$sfx" }

    Write-Step '解压工具链'
    & $sfx '-y' "-o$ToolchainDir"
    if ($LASTEXITCODE -ne 0) { throw "解压工具链失败（退出码 $LASTEXITCODE）。" }

    $gxx = Find-Gxx
}

if (-not $gxx) { throw '解压之后仍然找不到 g++.exe。' }
Write-Ok "g++  -> $($gxx.FullName)"

# --- 2. 编译 ---------------------------------------------------------------

if (-not (Test-Path $DistDir)) {
    New-Item -ItemType Directory -Path $DistDir | Out-Null
}

Write-Step '编译资源 src\app.rc'

$WindresExe   = Join-Path (Split-Path -Parent $gxx.FullName) 'windres.exe'
$ResourceFile = Join-Path $Root 'src\app.rc'
$ResourceObj  = Join-Path $Root 'build\app_res.o'
$BuildDir     = Join-Path $Root 'build'

if (-not (Test-Path $BuildDir)) {
    New-Item -ItemType Directory -Path $BuildDir | Out-Null
}

if (Test-Path $WindresExe) {
    & $WindresExe -I (Join-Path $Root 'src') -i $ResourceFile -o $ResourceObj
    if ($LASTEXITCODE -ne 0) { throw "资源编译失败（退出码 $LASTEXITCODE）。" }
} else {
    Write-Warning '找不到 windres.exe，将生成不带图标与版本信息的 exe。'
    $ResourceObj = $null
}

Write-Step '编译 src\main.cpp'

$linkArgs = @(
    '-municode', '-mwindows', '-O2', '-s', '-static',
    '-o', $OutputExe,
    $SourceFile
)
if ($ResourceObj) { $linkArgs += $ResourceObj }
$linkArgs += @('-luser32', '-lshell32', '-ladvapi32', '-lpowrprof')

& $gxx.FullName @linkArgs

if ($LASTEXITCODE -ne 0) { throw "编译失败（退出码 $LASTEXITCODE）。" }

$size = (Get-Item $OutputExe).Length
Write-Ok ("完成 -> {0}  ({1:N1} KB)" -f $OutputExe, ($size / 1KB))
