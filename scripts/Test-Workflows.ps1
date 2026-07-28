[CmdletBinding()]
param(
    [ValidateSet('CI', 'Release', 'All')]
    [string]$Scope = 'All'
)

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

function Assert-Matches {
    param(
        [Parameter(Mandatory)][string]$Content,
        [Parameter(Mandatory)][string]$Pattern,
        [Parameter(Mandatory)][string]$Message
    )

    Assert-True ([regex]::IsMatch($Content, $Pattern)) $Message
}

function Read-Workflow {
    param([Parameter(Mandatory)][string]$Path)

    Assert-True (Test-Path -LiteralPath $Path -PathType Leaf) "未找到工作流：$Path"
    return Get-Content -LiteralPath $Path -Raw
}

$repositoryRoot = [System.IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))

if ($Scope -in @('CI', 'All')) {
    $ciPath = Join-Path $repositoryRoot '.github\workflows\ci.yml'
    $ci = Read-Workflow $ciPath

    Assert-Matches $ci '(?m)^name:\s*CI\s*$' 'CI 工作流名称必须为 CI。'
    Assert-Matches $ci '(?m)^\s*branches:\s*\[main\]\s*$' 'CI 必须监听 main 分支 push。'
    Assert-Matches $ci '(?m)^\s*pull_request:\s*$' 'CI 必须监听 pull_request。'
    Assert-Matches $ci '(?m)^\s*workflow_dispatch:\s*$' 'CI 必须支持手动运行。'
    Assert-Matches $ci '(?ms)^permissions:\s*\r?\n\s+contents:\s*read\s*$' 'CI 顶层权限必须为 contents: read。'
    Assert-Matches $ci '(?m)^\s*runs-on:\s*windows-2025-vs2026\s*$' 'CI 必须使用 windows-2025-vs2026。'
    Assert-Matches $ci 'actions/checkout@v7' 'CI 必须使用 actions/checkout@v7。'
    Assert-Matches $ci 'actions/upload-artifact@v7' 'CI 必须使用 actions/upload-artifact@v7。'
    Assert-Matches $ci '(?s)Build-Plugin\.ps1.+-Verify' 'CI 必须调用公共构建脚本并启用导出验证。'
    Assert-Matches $ci 'retention-days:\s*7' 'CI Release Artifact 必须保留 7 天。'

    foreach ($target in @(
        @('Debug', 'x86', 'x86', 'TrafficMonitorMedia/bin/Debug/TrafficMonitorMedia.dll'),
        @('Debug', 'x64', 'x64', 'TrafficMonitorMedia/bin/x64/Debug/TrafficMonitorMedia.dll'),
        @('Debug', 'ARM64EC', 'arm64ec', 'TrafficMonitorMedia/bin/ARM64EC/Debug/TrafficMonitorMedia.dll'),
        @('Release', 'x86', 'x86', 'TrafficMonitorMedia/bin/Release/TrafficMonitorMedia.dll'),
        @('Release', 'x64', 'x64', 'TrafficMonitorMedia/bin/x64/Release/TrafficMonitorMedia.dll'),
        @('Release', 'ARM64EC', 'arm64ec', 'TrafficMonitorMedia/bin/ARM64EC/Release/TrafficMonitorMedia.dll')
    )) {
        $configuration = [regex]::Escape($target[0])
        $platform = [regex]::Escape($target[1])
        $artifactArchitecture = [regex]::Escape($target[2])
        $dllPath = [regex]::Escape($target[3])
        $pattern = (
            "(?ms)- configuration:\s*$configuration\s*\r?\n" +
            "\s+platform:\s*$platform\s*\r?\n" +
            "\s+artifact_arch:\s*$artifactArchitecture\s*\r?\n" +
            "\s+dll_path:\s*$dllPath"
        )
        Assert-Matches $ci $pattern (
            "CI 缺少矩阵组合：$($target[0])|$($target[1])，" +
            '或其对外架构名/输出路径不正确。'
        )
    }

    $ciConfigurations = [regex]::Matches($ci, '(?m)^\s+- configuration:').Count
    Assert-True ($ciConfigurations -eq 6) "CI 应恰好包含 6 个构建组合，实际为：$ciConfigurations。"
    Assert-Matches $ci 'name:\s*TrafficMonitorMedia-\$\{\{ matrix\.artifact_arch \}\}-Release' (
        'CI Release Artifact 必须使用对外架构名。'
    )
    Assert-True (-not $ci.Contains('artifact_arch: win32')) 'CI 对外架构名不应继续使用 win32。'
    Assert-True (-not $ci.Contains('TrafficMonitorMedia-' + '$' + '{{ matrix.platform }}-Release')) (
        'CI Artifact 不应直接使用 MSBuild 平台名。'
    )
    Assert-True (-not ([regex]::IsMatch($ci, '(?m)^\s+platform:\s*Win32\s*$'))) 'CI 应使用 x86 作为 32 位对外平台名。'
    Assert-True (-not ([regex]::IsMatch($ci, '(?m)^\s+platform:\s*ARM64\s*$'))) 'CI 不应引用工程不存在的 ARM64 平台。'

    Write-Output 'ci.yml 契约检查通过。'
}

if ($Scope -in @('Release', 'All')) {
    $releasePath = Join-Path $repositoryRoot '.github\workflows\release.yml'
    $release = Read-Workflow $releasePath

    Assert-Matches $release '(?m)^name:\s*Release\s*$' 'Release 工作流名称必须为 Release。'
    Assert-Matches $release "(?m)^\s+- 'v\*'\s*$" 'Release 必须监听 v* 标签。'
    Assert-Matches $release '(?m)^\s*workflow_dispatch:\s*$' 'Release 必须支持手动运行。'
    Assert-Matches $release '(?m)^\s*tag:\s*$' '手动发布必须要求 tag 输入。'
    Assert-Matches $release '(?ms)^permissions:\s*\r?\n\s+contents:\s*read\s*$' 'Release 顶层权限必须为 contents: read。'
    Assert-Matches $release '(?ms)^\s+release:\s*\r?\n.*?permissions:\s*\r?\n\s+contents:\s*write\s*$' '只有 release Job 应获得 contents: write。'
    Assert-Matches $release '(?m)^\s*runs-on:\s*windows-2025-vs2026\s*$' 'Release 构建必须使用 windows-2025-vs2026。'
    Assert-Matches $release 'actions/checkout@v7' 'Release 必须使用 actions/checkout@v7。'
    Assert-Matches $release 'actions/upload-artifact@v7' 'Release 必须使用 actions/upload-artifact@v7。'
    Assert-Matches $release 'actions/download-artifact@v8' 'Release 必须使用 actions/download-artifact@v8。'
    Assert-Matches $release '(?s)Build-Plugin\.ps1.+Release.+-Verify' 'Release 必须执行 Release 构建和导出验证。'
    Assert-Matches $release 'SHA256SUMS\.txt' 'Release 必须生成 SHA256SUMS.txt。'
    Assert-Matches $release 'packages\[@\].+-ne 3' 'Release 必须校验恰好生成三套发布包。'
    Assert-Matches $release 'gh release create' 'Release 必须使用 GitHub CLI 创建发布。'
    Assert-Matches $release '--verify-tag' 'Release 创建时必须验证标签存在。'
    Assert-Matches $release '--generate-notes' 'Release 必须自动生成发布说明。'

    foreach ($target in @(
        @('x86', 'x86', 'TrafficMonitorMedia/bin/Release/TrafficMonitorMedia.dll'),
        @('x64', 'x64', 'TrafficMonitorMedia/bin/x64/Release/TrafficMonitorMedia.dll'),
        @('ARM64EC', 'arm64ec', 'TrafficMonitorMedia/bin/ARM64EC/Release/TrafficMonitorMedia.dll')
    )) {
        $platform = [regex]::Escape($target[0])
        $packageArchitecture = [regex]::Escape($target[1])
        $dllPath = [regex]::Escape($target[2])
        $pattern = (
            "(?ms)- platform:\s*$platform\s*\r?\n" +
            "\s+package_arch:\s*$packageArchitecture\s*\r?\n" +
            "\s+dll_path:\s*$dllPath"
        )
        Assert-Matches $release $pattern (
            "Release 缺少构建目标：$($target[0])，" +
            '或其发布架构名/输出路径不正确。'
        )
    }

    $releasePlatforms = [regex]::Matches($release, '(?m)^\s+- platform:').Count
    Assert-True ($releasePlatforms -eq 3) (
        "Release 应恰好包含 3 个构建目标，实际为：$releasePlatforms。"
    )
    Assert-True (-not $release.Contains('package_arch: win32')) (
        'Release 对外架构名不应继续使用 win32。'
    )
    Assert-True (-not ([regex]::IsMatch($release, '(?m)^\s+platform:\s*Win32\s*$'))) 'Release 应使用 x86 作为 32 位对外平台名。'
    Assert-True (-not ([regex]::IsMatch($release, '(?m)^\s+platform:\s*ARM64\s*$'))) 'Release 不应引用工程不存在的 ARM64 平台。'

    Write-Output 'release.yml 契约检查通过。'
}
