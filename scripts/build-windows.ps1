[CmdletBinding()]
param(
    [ValidateSet('RelWithDebInfo', 'Release', 'Debug', 'MinSizeRel')]
    [string] $Configuration = 'RelWithDebInfo'
)

$ErrorActionPreference = 'Stop'

$RootDir = Resolve-Path (Join-Path $PSScriptRoot '..')
$BuildDir = Join-Path $RootDir 'build'
$InstallPrefix = if ($env:VINYL_REEL_INSTALL_PREFIX) {
    $env:VINYL_REEL_INSTALL_PREFIX
} else {
    Join-Path $RootDir 'release'
}

if (-not $env:VINYL_REEL_OBS_ROOT) {
    throw 'VINYL_REEL_OBS_ROOT must point at a Windows OBS development install.'
}

if (-not (Test-Path $env:VINYL_REEL_OBS_ROOT)) {
    throw "VINYL_REEL_OBS_ROOT does not exist: $env:VINYL_REEL_OBS_ROOT"
}

cmake -S $RootDir -B $BuildDir `
  -DCMAKE_INSTALL_PREFIX="$InstallPrefix" `
  -DVINYL_REEL_OBS_ROOT="$env:VINYL_REEL_OBS_ROOT"

cmake --build $BuildDir --config $Configuration --target install
