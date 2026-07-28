# TrafficMonitorMedia 本机开发命令。
# 通过 scripts/Get-VsToolchain.ps1 动态定位 Visual Studio C++/MFC 工具链。

set shell := ["powershell.exe", "-NoProfile", "-Command"]
set script-interpreter := ["powershell.exe", "-NoProfile", "-ExecutionPolicy", "Bypass", "-File"]

build_plugin_script := "scripts\\Build-Plugin.ps1"
vs_toolchain_script := "scripts\\Get-VsToolchain.ps1"

# 构建插件；可选参数：just build Debug x64。
[script]
build configuration="Release" platform="x64":
    $buildScript = (Resolve-Path -LiteralPath '{{ build_plugin_script }}').Path
    & $buildScript -Configuration '{{ configuration }}' -Platform '{{ platform }}'
    exit $LASTEXITCODE

# 删除已知生成物后重新构建，强制重新编译全部源文件。
[script]
rebuild configuration="Release" platform="x64":
    & just clean
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

    & just build '{{ configuration }}' '{{ platform }}'
    exit $LASTEXITCODE

# 检查 DLL 是否存在，并验证 TrafficMonitor 所需导出。
[script]
verify configuration="Release" platform="x64":
    $msbuildPlatform = if ('{{ platform }}' -eq 'x86') { 'Win32' } else { '{{ platform }}' }
    $outputDirectory = if ($msbuildPlatform -eq 'Win32') {
        Join-Path $PWD 'TrafficMonitorMedia\\bin\\{{ configuration }}'
    }
    else {
        Join-Path $PWD "TrafficMonitorMedia\\bin\\$msbuildPlatform\\{{ configuration }}"
    }
    $dll = Join-Path $outputDirectory 'TrafficMonitorMedia.dll'
    if (-not (Test-Path -LiteralPath $dll -PathType Leaf)) {
        throw "未找到 DLL：$dll。请先执行 just build {{ configuration }} {{ platform }}。"
    }

    $toolchainScript = (Resolve-Path -LiteralPath '{{ vs_toolchain_script }}').Path
    $toolchain = & $toolchainScript
    $dumpbin = $toolchain.Dumpbin

    $exports = & $dumpbin /exports $dll 2>&1
    if ($LASTEXITCODE -ne 0) {
        $exports
        exit $LASTEXITCODE
    }
    if (-not ($exports | Select-String -Quiet -Pattern 'TMPluginGetInstance')) {
        throw 'DLL 未导出 TMPluginGetInstance。'
    }

    Write-Output "已验证：$dll 导出 TMPluginGetInstance。"

# 构建后验证 DLL 与导出。
[script]
check configuration="Release" platform="x64":
    $buildScript = (Resolve-Path -LiteralPath '{{ build_plugin_script }}').Path
    & $buildScript -Configuration '{{ configuration }}' -Platform '{{ platform }}' -Verify
    exit $LASTEXITCODE

# 仅删除本工程已知的生成物。
[script]
clean:
    $root = (Resolve-Path -LiteralPath '.').Path
    $targets = @(
        'TrafficMonitorMedia\bin',
        'TrafficMonitorMedia\Release',
        'TrafficMonitorMedia\TrafficM.1C2173CA',
        'bin',
        '.vs'
    )

    foreach ($relative in $targets) {
        $target = [System.IO.Path]::GetFullPath((Join-Path $root $relative))
        if (-not $target.StartsWith($root + [System.IO.Path]::DirectorySeparatorChar, [System.StringComparison]::OrdinalIgnoreCase)) {
            throw "拒绝删除工作区外路径：$target"
        }
        if (Test-Path -LiteralPath $target) {
            Remove-Item -LiteralPath $target -Recurse -Force
            Write-Output "已清理：$relative"
        }
    }
