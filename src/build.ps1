# Builds the plugin and deploys it to the designated MO2 mod folder. 

# usage: .\build.ps1 [-Config Release|Debug|RelWithDebInfo]
param([string]$Config = "RelWithDebInfo")

$ErrorActionPreference = "Stop"
$cmake = "C:\Program Files\Microsoft Visual Studio\2022\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe"

if (-not (Test-Path $cmake)) { $cmake = "cmake" }

$src = $PSScriptRoot
$build = Join-Path $src "build"

if (-not (Test-Path (Join-Path $build "CMakeCache.txt"))) {
	& $cmake -S $src -B $build -G "Visual Studio 17 2022" -A Win32
	if ($LASTEXITCODE) { exit $LASTEXITCODE }
}
& $cmake --build $build --config $Config

exit $LASTEXITCODE
