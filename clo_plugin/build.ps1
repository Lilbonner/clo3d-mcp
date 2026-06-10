# Builds the CLO MCP plugin (Release, the only config CLO loads).
#
#   .\build.ps1 -SdkDir C:\path\to\CLO_SDK_v7.3.240        # full plugin
#   .\build.ps1 -StubOnly                                   # clo_mcp_test.exe, no SDK needed
#
# Prereqs: Visual Studio 2022 (MSVC C++), CMake >= 3.20, Qt 5.15.x msvc2019_64.
# Windows PowerShell 5.1 compatible.
param(
    [string]$SdkDir = $env:CLO_SDK_DIR,
    [string]$QtDir,
    [switch]$StubOnly
)
$ErrorActionPreference = "Stop"

$src = $PSScriptRoot
$build = Join-Path $src "build-msvc"

if (-not $QtDir) {
    foreach ($c in @("C:\Qt\5.15.2\msvc2019_64", "C:\Qt\5.15.16\msvc2019_64")) {
        if (Test-Path $c) { $QtDir = $c; break }
    }
}
if (-not $QtDir -or -not (Test-Path $QtDir)) {
    throw "Qt 5.15 (msvc2019_64) not found. Pass -QtDir, e.g.: .\build.ps1 -QtDir C:\Qt\5.15.2\msvc2019_64"
}

$cmakeArgs = @("-S", $src, "-B", $build, "-G", "Visual Studio 17 2022", "-A", "x64",
               "-DCMAKE_PREFIX_PATH=$($QtDir -replace '\\','/')")

if (-not $StubOnly) {
    if (-not $SdkDir) {
        $guess = Join-Path $HOME "clo_sdk\CLO_SDK_v7.3.240"
        if (Test-Path $guess) { $SdkDir = $guess }
    }
    if (-not $SdkDir -or -not (Test-Path $SdkDir)) {
        throw ("CLO SDK not found. Download CLO_SDK_v7.3.240_WIN.zip from developer.clo3d.com, " +
               "unzip it, then: .\build.ps1 -SdkDir C:\path\to\CLO_SDK_v7.3.240  " +
               "(or build the SDK-less stub: .\build.ps1 -StubOnly)")
    }
    $cmakeArgs += "-DCLO_SDK_DIR=$($SdkDir -replace '\\','/')"
}

Write-Host "[build] configuring (Qt: $QtDir$(if ($SdkDir) { ", SDK: $SdkDir" }))"
cmake @cmakeArgs
if ($LASTEXITCODE -ne 0) { throw "CMake configure failed" }

$target = "clo_mcp_plugin"
if ($StubOnly) { $target = "clo_mcp_test" }
cmake --build $build --config Release --target $target
if ($LASTEXITCODE -ne 0) { throw "Build failed" }

$out = Join-Path $build "Release\$target$(if ($StubOnly) { '.exe' } else { '.dll' })"
Write-Host ""
Write-Host "[build] OK -> $out"
if (-not $StubOnly) { Write-Host "[build] next: .\install.ps1" }
