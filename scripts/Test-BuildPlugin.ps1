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
$project = Join-Path $repositoryRoot 'TrafficMonitorMedia\TrafficMonitorMedia.vcxproj'

Assert-True (Test-Path -LiteralPath $buildScript -PathType Leaf) "未找到构建脚本：$buildScript"
Assert-True (Test-Path -LiteralPath $toolchainScript -PathType Leaf) "未找到工具链脚本：$toolchainScript"
Assert-True (Test-Path -LiteralPath $project -PathType Leaf) "未找到 Visual C++ 工程：$project"

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
    (@($platformAttribute.ValidValues | Sort-Object) -join ',') -eq 'ARM64EC,Win32,x64,x86'
) 'Platform 仅应允许 Win32、x86、x64、ARM64EC。'

Assert-True $command.Parameters.ContainsKey('Verify') '构建脚本缺少 Verify 开关。'

$scriptText = Get-Content -LiteralPath $buildScript -Raw
foreach ($requiredText in @(
    'Get-VsToolchain.ps1',
    'TrafficMonitorMedia.vcxproj',
    'TMPluginGetInstance',
    "`$msbuildPlatform = if (`$Platform -eq 'x86')",
    "if (`$msbuildPlatform -eq 'Win32')",
    "bin\`$Configuration",
    "bin\`$msbuildPlatform\`$Configuration"
)) {
    Assert-True $scriptText.Contains($requiredText) "构建脚本缺少关键契约：$requiredText"
}

$projectText = Get-Content -LiteralPath $project -Raw
foreach ($configuration in @('Debug', 'Release')) {
    foreach ($platform in @('Win32', 'x64', 'ARM64EC')) {
        $projectConfiguration = '<ProjectConfiguration Include="' + $configuration + '|' + $platform + '">'
        Assert-True $projectText.Contains($projectConfiguration) (
            "Visual C++ 工程缺少配置：$configuration|$platform。"
        )
    }
}

foreach ($platform in @('Win32', 'x64', 'ARM64EC')) {
    foreach ($configuration in @('Debug', 'Release')) {
        $condition = 'Condition="''$(Configuration)|$(Platform)''==''' + $configuration + '|' + $platform + '''"'
        $pchContract = '<PrecompiledHeader ' + $condition + '>Create</PrecompiledHeader>'
        Assert-True $projectText.Contains($pchContract) (
            "Visual C++ 工程缺少预编译头配置：$configuration|$platform。"
        )
    }
}

$solution = Join-Path $repositoryRoot 'TrafficMonitorMedia.sln'
Assert-True (Test-Path -LiteralPath $solution -PathType Leaf) "未找到解决方案：$solution"
$solutionText = Get-Content -LiteralPath $solution -Raw
Assert-True (-not $solutionText.Contains('Any CPU')) '解决方案不应引用 C++ 工程不存在的 Any CPU 配置。'
foreach ($target in @(
    @('Debug', 'x86', 'Win32'),
    @('Release', 'x86', 'Win32'),
    @('Debug', 'x64', 'x64'),
    @('Release', 'x64', 'x64'),
    @('Debug', 'ARM64EC', 'ARM64EC'),
    @('Release', 'ARM64EC', 'ARM64EC')
)) {
    $solutionConfiguration = $target[0]
    $solutionPlatform = $target[1]
    $projectPlatform = $target[2]
    Assert-True $solutionText.Contains("$solutionConfiguration|$solutionPlatform = $solutionConfiguration|$solutionPlatform") (
        "解决方案缺少平台：$solutionConfiguration|$solutionPlatform。"
    )
    Assert-True $solutionText.Contains(".$solutionConfiguration|$solutionPlatform.ActiveCfg = $solutionConfiguration|$projectPlatform") (
        "解决方案平台映射错误：$solutionConfiguration|$solutionPlatform 应映射到 $solutionConfiguration|$projectPlatform。"
    )
    Assert-True $solutionText.Contains(".$solutionConfiguration|$solutionPlatform.Build.0 = $solutionConfiguration|$projectPlatform") (
        "解决方案构建映射错误：$solutionConfiguration|$solutionPlatform 应构建 $solutionConfiguration|$projectPlatform。"
    )
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
