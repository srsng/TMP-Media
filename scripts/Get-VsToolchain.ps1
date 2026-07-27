[CmdletBinding()]
param()

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

# 当前宿主进程有时同时携带 PATH 与 Path。Windows 的子进程环境不接受
# 这两个大小写不同、语义相同的键；删除全部大小写变体后只保留 Path。
$pathValue = [string]$env:Path
$pathKeys = @(
    [Environment]::GetEnvironmentVariables('Process').Keys |
        Where-Object {
            [string]::Equals(
                [string]$_,
                'Path',
                [System.StringComparison]::OrdinalIgnoreCase
            )
        }
)
foreach ($pathKey in $pathKeys) {
    Remove-Item -LiteralPath ("Env:$pathKey") -ErrorAction Stop
}
Set-Item -LiteralPath 'Env:Path' -Value $pathValue

$programFilesX86 = [Environment]::GetEnvironmentVariable('ProgramFiles(x86)')
if ([string]::IsNullOrWhiteSpace($programFilesX86)) {
    throw '未找到 ProgramFiles(x86) 环境变量，无法定位 vswhere.exe。'
}

$vswhere = Join-Path $programFilesX86 'Microsoft Visual Studio\Installer\vswhere.exe'
if (-not (Test-Path -LiteralPath $vswhere -PathType Leaf)) {
    throw "未找到 vswhere.exe：$vswhere。请安装 Visual Studio Installer。"
}

$requiredComponents = @(
    'Microsoft.VisualStudio.Component.VC.Tools.x86.x64',
    'Microsoft.VisualStudio.Component.VC.ATLMFC'
)

$vsRoots = @(& $vswhere -latest -products '*' -requires $requiredComponents -property installationPath)
if ($LASTEXITCODE -ne 0) {
    throw "vswhere 查询 Visual Studio 安装目录失败，退出码：$LASTEXITCODE。"
}
$vsRoot = $vsRoots |
    Where-Object { -not [string]::IsNullOrWhiteSpace($_) } |
    Select-Object -First 1
if (-not $vsRoot) {
    throw '未找到同时安装 C++ x86/x64 工具和 ATL/MFC 的 Visual Studio 实例。'
}
$vsRoot = $vsRoot.Trim()
$vsRootPrefix = $vsRoot.TrimEnd('\') + '\'

$msbuildCandidates = @(& $vswhere -latest -products '*' -requires $requiredComponents -find 'MSBuild\**\Bin\MSBuild.exe')
if ($LASTEXITCODE -ne 0) {
    throw "vswhere 查询 MSBuild.exe 失败，退出码：$LASTEXITCODE。"
}
$msbuild = $msbuildCandidates |
    Where-Object {
        -not [string]::IsNullOrWhiteSpace($_) -and
        $_.StartsWith($vsRootPrefix, [System.StringComparison]::OrdinalIgnoreCase) -and
        (Test-Path -LiteralPath $_ -PathType Leaf)
    } |
    Select-Object -First 1
if (-not $msbuild) {
    throw "所选 Visual Studio 实例中未找到 MSBuild.exe：$vsRoot"
}
$msbuild = $msbuild.Trim()

$vcToolsVersionFile = Join-Path $vsRoot 'VC\Auxiliary\Build\Microsoft.VCToolsVersion.default.txt'
if (-not (Test-Path -LiteralPath $vcToolsVersionFile -PathType Leaf)) {
    throw "未找到默认 VC 工具版本文件：$vcToolsVersionFile"
}
$vcToolsVersion = (Get-Content -LiteralPath $vcToolsVersionFile -Raw).Trim()
if ([string]::IsNullOrWhiteSpace($vcToolsVersion)) {
    throw "默认 VC 工具版本文件为空：$vcToolsVersionFile"
}

$dumpbin = Join-Path $vsRoot "VC\Tools\MSVC\$vcToolsVersion\bin\Hostx64\x64\dumpbin.exe"
if (-not (Test-Path -LiteralPath $dumpbin -PathType Leaf)) {
    throw "所选 Visual Studio 实例中未找到 dumpbin.exe：$dumpbin"
}

[pscustomobject]@{
    VsRoot  = $vsRoot
    MSBuild = $msbuild
    Dumpbin = $dumpbin
}
