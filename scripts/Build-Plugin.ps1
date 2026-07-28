[CmdletBinding()]
param(
    [ValidateSet('Debug', 'Release')]
    [string]$Configuration = 'Release',

    [ValidateSet('Win32', 'x86', 'x64', 'ARM64EC')]
    [string]$Platform = 'x64',

    [switch]$Verify
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$repositoryRoot = [System.IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
$project = Join-Path $repositoryRoot 'TrafficMonitorMedia\TrafficMonitorMedia.vcxproj'
$toolchainScript = Join-Path $PSScriptRoot 'Get-VsToolchain.ps1'

if (-not (Test-Path -LiteralPath $project -PathType Leaf)) {
    throw "未找到 Visual C++ 工程：$project"
}
if (-not (Test-Path -LiteralPath $toolchainScript -PathType Leaf)) {
    throw "未找到工具链定位脚本：$toolchainScript"
}

$toolchain = & $toolchainScript
Write-Output "MSBuild: $($toolchain.MSBuild)"
$msbuildPlatform = if ($Platform -eq 'x86') { 'Win32' } else { $Platform }
if ($Platform -eq $msbuildPlatform) {
    Write-Output "构建配置：$Configuration|$Platform"
}
else {
    Write-Output "构建配置：$Configuration|$Platform (MSBuild: $msbuildPlatform)"
}

& $toolchain.MSBuild `
    $project `
    '/m' `
    '/t:Build' `
    "/p:Configuration=$Configuration" `
    "/p:Platform=$msbuildPlatform" `
    '/v:minimal'

if ($LASTEXITCODE -ne 0) {
    throw "MSBuild 构建失败，退出码：$LASTEXITCODE；配置：$Configuration|$Platform。"
}

$outputDirectory = if ($msbuildPlatform -eq 'Win32') {
    Join-Path $repositoryRoot "TrafficMonitorMedia\bin\$Configuration"
}
else {
    Join-Path $repositoryRoot "TrafficMonitorMedia\bin\$msbuildPlatform\$Configuration"
}
$dllPath = Join-Path $outputDirectory 'TrafficMonitorMedia.dll'

if (-not (Test-Path -LiteralPath $dllPath -PathType Leaf)) {
    throw "构建结束但未找到 DLL：$dllPath"
}

if ($Verify) {
    $exports = & $toolchain.Dumpbin /exports $dllPath 2>&1
    if ($LASTEXITCODE -ne 0) {
        $exports | Write-Output
        throw "dumpbin 导出检查失败，退出码：$LASTEXITCODE；DLL：$dllPath"
    }

    if (-not ($exports | Select-String -Quiet -Pattern '\bTMPluginGetInstance\b')) {
        throw "DLL 未导出 TMPluginGetInstance：$dllPath"
    }

    Write-Output "已验证 DLL 导出：$dllPath"
}
else {
    Write-Output "构建完成：$dllPath"
}
