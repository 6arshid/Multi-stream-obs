<#
.SYNOPSIS
  Configures, builds, tests and installs the Universal Multi-Stream OBS plugin on Windows.

.DESCRIPTION
  Requires: Visual Studio 2022 (with C++ workload). CMake is resolved from the Visual
  Studio bundled copy or PATH. The first configure downloads pinned OBS Studio sources,
  pre-built OBS dependencies and Qt 6 (see buildspec.json), then builds libobs and
  obs-frontend-api for Debug and Release. This can take several minutes the first time.

.EXAMPLE
  .\scripts\Build-Windows.ps1
  .\scripts\Build-Windows.ps1 -Configuration Release -Install
  .\scripts\Build-Windows.ps1 -ConfigureOnly
#>
[CmdletBinding()]
param(
    [ValidateSet('Debug', 'RelWithDebInfo', 'Release')]
    [string] $Configuration = 'RelWithDebInfo',

    [switch] $ConfigureOnly,
    [switch] $NoTests,
    [switch] $Install
)

$ErrorActionPreference = 'Stop'

function Find-CMake {
    $vswhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
    if (Test-Path $vswhere) {
        $vsPath = & $vswhere -latest -products * -property installationPath 2>$null | Select-Object -First 1
        if ($vsPath) {
            $cmake = Join-Path $vsPath 'Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe'
            if (Test-Path $cmake) { return $cmake }
        }
    }
    foreach ($edition in @('Community', 'Professional', 'Enterprise', 'BuildTools')) {
        $cmake = "${env:ProgramFiles}\Microsoft Visual Studio\2022\$edition\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe"
        if (Test-Path $cmake) { return $cmake }
    }
    $cmd = Get-Command cmake -ErrorAction SilentlyContinue
    if ($cmd) { return $cmd.Source }
    throw 'CMake was not found. Install Visual Studio 2022 with the C++ workload, or add CMake to PATH.'
}

$ProjectRoot = Split-Path -Parent $PSScriptRoot
Set-Location $ProjectRoot

$CMake = Find-CMake
Write-Host "Using CMake: $CMake"

& $CMake --preset windows-x64
if ($LASTEXITCODE -ne 0) { throw "CMake configure failed ($LASTEXITCODE)" }

if ($ConfigureOnly) { exit 0 }

& $CMake --build --preset windows-x64 --config $Configuration --parallel -- /consoleLoggerParameters:Summary /noLogo
if ($LASTEXITCODE -ne 0) { throw "Build failed ($LASTEXITCODE)" }

if (-not $NoTests) {
    $CTest = Join-Path (Split-Path -Parent $CMake) 'ctest.exe'
    if (-not (Test-Path $CTest)) { $CTest = 'ctest' }
    Push-Location build_x64
    try {
        & $CTest -C $Configuration --output-on-failure
        if ($LASTEXITCODE -ne 0) { throw "Tests failed ($LASTEXITCODE)" }
    } finally {
        Pop-Location
    }
}

if ($Install) {
    $Prefix = Join-Path $ProjectRoot "release\$Configuration"
    & $CMake --install build_x64 --config $Configuration --prefix $Prefix
    if ($LASTEXITCODE -ne 0) { throw "Install failed ($LASTEXITCODE)" }
    Write-Host "Installed to $Prefix"
}

Write-Host 'Done.'
