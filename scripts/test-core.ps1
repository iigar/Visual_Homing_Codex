param(
    [string]$Configuration = "Debug",
    [switch]$Clean
)

$ErrorActionPreference = "Stop"

$repoRoot = Resolve-Path (Join-Path $PSScriptRoot "..")
$coreDir = Join-Path $repoRoot "core"
$buildDir = Join-Path $coreDir "build"
$devShell = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\2022\BuildTools\Common7\Tools\Launch-VsDevShell.ps1"

if (-not (Test-Path -LiteralPath $devShell)) {
    throw "Visual Studio developer shell not found: $devShell"
}

function Invoke-Checked {
    param(
        [Parameter(Mandatory = $true)]
        [string]$Command,
        [string[]]$Arguments
    )

    & $Command @Arguments
    if ($LASTEXITCODE -ne 0) {
        throw "Command failed with exit code ${LASTEXITCODE}: $Command $Arguments"
    }
}

if ($Clean -and (Test-Path -LiteralPath $buildDir)) {
    $resolved = Resolve-Path -LiteralPath $buildDir
    if (-not ($resolved.Path.StartsWith((Resolve-Path -LiteralPath $coreDir).Path))) {
        throw "Refusing to remove unexpected build path: $($resolved.Path)"
    }
    Remove-Item -LiteralPath $resolved.Path -Recurse -Force
}

& $devShell

Invoke-Checked "cmake" @(
    "-S", $coreDir,
    "-B", $buildDir,
    "-DVISUAL_HOMING_ENABLE_LIVE_MAVLINK_OUTPUT=OFF",
    "-DVISUAL_HOMING_ENABLE_BENCH_PROPS_OFF_LIVE_OUTPUT=OFF",
    "-DVISUAL_HOMING_ATTACH_BENCH_PROPS_OFF_SERIAL_WRITER=OFF"
)
Invoke-Checked "cmake" @("--build", $buildDir, "--config", $Configuration)
Invoke-Checked "ctest" @("--test-dir", $buildDir, "-C", $Configuration, "--output-on-failure")
