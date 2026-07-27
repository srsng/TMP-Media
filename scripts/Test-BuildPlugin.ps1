[CmdletBinding()]
param()

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

function Assert-True {
    param(
        [Parameter(Mandatory)][bool]$Condition,
        [Parameter(Mandatory)][string]$Message
    )

    if (-not $Condition) {
        throw $Message
    }
}

$repositoryRoot = [System.IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
$buildScript = Join-Path $repositoryRoot 'scripts\Build-Plugin.ps1'
$toolchainScript = Join-Path $repositoryRoot 'scripts\Get-VsToolchain.ps1'

Assert-True (Test-Path -LiteralPath $buildScript -PathType Leaf) "未找到构建脚本：$buildScript"
Assert-True (Test-Path -LiteralPath $toolchainScript -PathType Leaf) "未找到工具链脚本：$toolchainScript"

$tokens = $null
$parseErrors = $null
[void][System.Management.Automation.Language.Parser]::ParseFile(
    $buildScript,
    [ref]$tokens,
    [ref]$parseErrors
)
Assert-True ($parseErrors.Count -eq 0) (
    "Build-Plugin.ps1 存在 PowerShell 语法错误：`n" +
    (($parseErrors | ForEach-Object Message) -join "`n")
)

$command = Get-Command -Name $buildScript

$configurationAttribute = $command.Parameters['Configuration'].Attributes |
    Where-Object { $_ -is [System.Management.Automation.ValidateSetAttribute] } |
    Select-Object -First 1
Assert-True ($null -ne $configurationAttribute) 'Configuration 参数缺少 ValidateSet。'
Assert-True (
    (@($configurationAttribute.ValidValues | Sort-Object) -join ',') -eq 'Debug,Release'
) 'Configuration 仅应允许 Debug、Release。'

$platformAttribute = $command.Parameters['Platform'].Attributes |
    Where-Object { $_ -is [System.Management.Automation.ValidateSetAttribute] } |
    Select-Object -First 1
Assert-True ($null -ne $platformAttribute) 'Platform 参数缺少 ValidateSet。'
Assert-True (
    (@($platformAttribute.ValidValues | Sort-Object) -join ',') -eq 'ARM64EC,Win32,x64'
) 'Platform 仅应允许 Win32、x64、ARM64EC。'

Assert-True $command.Parameters.ContainsKey('Verify') '构建脚本缺少 Verify 开关。'

$scriptText = Get-Content -LiteralPath $buildScript -Raw
foreach ($requiredText in @(
    'Get-VsToolchain.ps1',
    'TrafficMonitorMedia.vcxproj',
    'TMPluginGetInstance',
    "if (`$Platform -eq 'Win32')",
    "bin\`$Configuration",
    "bin\`$Platform\`$Configuration"
)) {
    Assert-True $scriptText.Contains($requiredText) "构建脚本缺少关键契约：$requiredText"
}

$null = & $toolchainScript
$pathEntries = @(
    [Environment]::GetEnvironmentVariables('Process').GetEnumerator() |
        Where-Object {
            [string]::Equals(
                [string]$_.Key,
                'Path',
                [System.StringComparison]::OrdinalIgnoreCase
            )
        }
)
Assert-True ($pathEntries.Count -eq 1) (
    "工具链脚本执行后应只保留一个 Path 环境键，实际为：" +
    (($pathEntries | ForEach-Object { [string]$_.Key }) -join ', ')
)
Assert-True ([string]$pathEntries[0].Key -ceq 'Path') '工具链脚本应将环境键规范为 Path。'
Assert-True (-not [string]::IsNullOrWhiteSpace([string]$pathEntries[0].Value)) '规范化后的 Path 不应为空。'

Write-Output 'Build-Plugin.ps1 契约检查通过。'
