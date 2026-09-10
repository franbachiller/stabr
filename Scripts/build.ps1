<#
    STABR - build en Windows.

    Encuentra CMake (el que viene con Visual Studio Build Tools, sin necesidad
    de instalarlo aparte), configura y compila.

    Uso:
        .\Scripts\build.ps1                 # Release
        .\Scripts\build.ps1 -Config Debug
        .\Scripts\build.ps1 -Clean
        .\Scripts\build.ps1 -Werror         # warnings como errores
#>

[CmdletBinding()]
param(
    [ValidateSet('Debug', 'Release', 'RelWithDebInfo')]
    [string]$Config = 'Release',
    [switch]$Clean,
    [switch]$Werror
)

$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $PSScriptRoot
$buildDir = Join-Path $root 'build'

function Find-CMake {
    $onPath = Get-Command cmake -ErrorAction SilentlyContinue
    if ($onPath) { return $onPath.Source }

    $vswhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
    if (Test-Path $vswhere) {
        $vsPath = & $vswhere -latest -products * -property installationPath
        if ($vsPath) {
            $candidate = Join-Path $vsPath 'Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe'
            if (Test-Path $candidate) { return $candidate }
        }
    }

    throw "No se encontro cmake. Instala CMake o el workload 'Desktop development with C++' de Visual Studio."
}

function Assert-WindowsSdk {
    $key = 'HKLM:\SOFTWARE\WOW6432Node\Microsoft\Windows Kits\Installed Roots'
    $root10 = (Get-ItemProperty -Path $key -Name KitsRoot10 -ErrorAction SilentlyContinue).KitsRoot10

    if (-not $root10 -or -not (Test-Path (Join-Path $root10 'Include'))) {
        Write-Host ''
        Write-Host 'FALTA EL WINDOWS SDK.' -ForegroundColor Red
        Write-Host 'Corre esto en una terminal COMO ADMINISTRADOR:' -ForegroundColor Yellow
        Write-Host ''
        Write-Host '  & "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\setup.exe" modify --installPath "${env:ProgramFiles(x86)}\Microsoft Visual Studio\2022\BuildTools" --add Microsoft.VisualStudio.Workload.VCTools --includeRecommended --quiet --norestart --wait'
        Write-Host ''
        Write-Host 'Ojo: el verbo tiene que ser "modify", no "install". Con "install"'
        Write-Host 'el instalador aborta con "ya se ha instalado" y no agrega nada.'
        Write-Host ''
        throw 'Windows SDK no encontrado.'
    }
}

$cmake = Find-CMake
Assert-WindowsSdk

Write-Host "cmake:  $cmake"
Write-Host "config: $Config"
Write-Host "build:  $buildDir"
Write-Host ''

if ($Clean -and (Test-Path $buildDir)) {
    Write-Host 'Limpiando build/ ...'
    Remove-Item -Recurse -Force $buildDir
}

$werrorFlag = if ($Werror) { 'ON' } else { 'OFF' }

& $cmake -S $root -B $buildDir -G 'Visual Studio 17 2022' -A x64 "-DSTABR_WERROR=$werrorFlag"
if ($LASTEXITCODE -ne 0) { throw "Fallo la configuracion de CMake (exit $LASTEXITCODE)" }

& $cmake --build $buildDir --config $Config --parallel
if ($LASTEXITCODE -ne 0) { throw "Fallo la compilacion (exit $LASTEXITCODE)" }

Write-Host ''
Write-Host 'Build OK.' -ForegroundColor Green

$vst3 = Get-ChildItem -Path $buildDir -Recurse -Filter 'STABR.vst3' -ErrorAction SilentlyContinue | Select-Object -First 1
if ($vst3) { Write-Host "VST3:  $($vst3.FullName)" }

$tests = Get-ChildItem -Path $buildDir -Recurse -Filter 'STABR_Tests.exe' -ErrorAction SilentlyContinue | Select-Object -First 1
if ($tests) { Write-Host "Tests: $($tests.FullName)" }
