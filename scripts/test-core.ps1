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

if ($Clean -and (Test-Path -LiteralPath $buildDir)) {
    $resolved = Resolve-Path -LiteralPath $buildDir
    if (-not ($resolved.Path.StartsWith((Resolve-Path -LiteralPath $coreDir).Path))) {
        throw "Refusing to remove unexpected build path: $($resolved.Path)"
    }
    Remove-Item -LiteralPath $resolved.Path -Recurse -Force
}

& $devShell

cmake -S $coreDir -B $buildDir
cmake --build $buildDir --config $Configuration
ctest --test-dir $buildDir -C $Configuration --output-on-failure
