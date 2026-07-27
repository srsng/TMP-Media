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

    foreach ($pair in @(
        @('Debug', 'Win32'),
        @('Debug', 'x64'),
        @('Release', 'Win32'),
        @('Release', 'x64')
    )) {
        $configuration = [regex]::Escape($pair[0])
        $platform = [regex]::Escape($pair[1])
        $pattern = "(?ms)- configuration:\s*$configuration\s*\r?\n\s+platform:\s*$platform\s*\r?\n\s+dll_path:"
        Assert-Matches $ci $pattern "CI 缺少矩阵组合：$($pair[0])|$($pair[1])。"
    }

    Assert-True (-not $ci.Contains('ARM64EC')) 'CI 不应构建 TrafficMonitor 当前未发布的 ARM64EC 宿主架构。'

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
    Assert-Matches $release 'packages\[@\].+-ne 2' 'Release 必须校验恰好生成两套发布包。'
    Assert-Matches $release 'gh release create' 'Release 必须使用 GitHub CLI 创建发布。'
    Assert-Matches $release '--verify-tag' 'Release 创建时必须验证标签存在。'
    Assert-Matches $release '--generate-notes' 'Release 必须自动生成发布说明。'

    foreach ($pair in @(
        @('Win32', 'win32'),
        @('x64', 'x64')
    )) {
        $platform = [regex]::Escape($pair[0])
        $packageArchitecture = [regex]::Escape($pair[1])
        $pattern = "(?ms)- platform:\s*$platform\s*\r?\n\s+package_arch:\s*$packageArchitecture\s*\r?\n\s+dll_path:"
        Assert-Matches $release $pattern "Release 缺少构建目标：$($pair[0])。"
    }

    Assert-True (-not $release.Contains('ARM64EC')) 'Release 不应发布 TrafficMonitor 当前未提供的 ARM64EC 宿主架构。'
    Assert-True (-not $release.Contains('arm64ec')) 'Release 不应生成 arm64ec 命名的发布包。'

    Write-Output 'release.yml 契约检查通过。'
}
