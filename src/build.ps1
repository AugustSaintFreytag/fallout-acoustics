# Builds the plugin and deploys it to the designated MO2 mod folder. 

# usage: .\build.ps1 [-Config Release|Debug|RelWithDebInfo]
param([string]$Config = "RelWithDebInfo")

$ErrorActionPreference = "Stop"
$vs = "C:\Program Files\Microsoft Visual Studio\2022\Community"
$cmake = "$vs\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe"
$ninja = "$vs\Common7\IDE\CommonExtensions\Microsoft\CMake\Ninja\ninja.exe"
$vcvars = "$vs\VC\Auxiliary\Build\vcvarsall.bat"

if (-not (Test-Path $cmake)) { $cmake = "cmake" }

$src = $PSScriptRoot
$build = Join-Path $src "build"
$clangd = Join-Path $build "clangd"

if (-not (Test-Path (Join-Path $build "CMakeCache.txt"))) {
	& $cmake -S $src -B $build -G "Visual Studio 17 2022" -A Win32
	if ($LASTEXITCODE) { exit $LASTEXITCODE }
}

# Configures a Ninja tree for compile_commands.json, which clangd reads via src/.clangd.
# Runs on every build so new source files get indexed.
cmd /c "call `"$vcvars`" x86 >nul 2>nul && `"$cmake`" -S `"$src`" -B `"$clangd`" -G Ninja -DCMAKE_MAKE_PROGRAM=`"$ninja`" -DCMAKE_BUILD_TYPE=$Config -DCMAKE_EXPORT_COMPILE_COMMANDS=ON >nul"
if ($LASTEXITCODE) { Write-Warning "clangd compile database configure failed" }

& $cmake --build $build --config $Config

exit $LASTEXITCODE
