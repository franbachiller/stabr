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
    [switch]$Werror,
    # No copiar el VST3 a C:\Program Files\Common Files\VST3 al terminar.
    # Necesario si un DAW tiene el plugin cargado (el archivo queda bloqueado)
    # o si la shell no tiene permisos de escritura ahi.
    [switch]$NoCopy
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
        $pf86 = ${env:ProgramFiles(x86)}
        $setup = Join-Path $pf86 'Microsoft Visual Studio\Installer\setup.exe'
        $vsBt  = Join-Path $pf86 'Microsoft Visual Studio\2022\BuildTools'

        Write-Host ''
        Write-Host 'FALTA EL WINDOWS SDK.' -ForegroundColor Red
        Write-Host 'Corre esto en una terminal COMO ADMINISTRADOR:' -ForegroundColor Yellow
        Write-Host ''
        Write-Host "  & `"$setup`" modify --installPath `"$vsBt`" --add Microsoft.VisualStudio.Workload.VCTools --includeRecommended --passive --norestart"
        Write-Host ''
        Write-Host 'Tres detalles que hacen fallar esto:' -ForegroundColor Yellow
        Write-Host '  1. El verbo tiene que ser "modify", no "install". Con "install" el'
        Write-Host '     instalador aborta con "ya se ha instalado", no agrega nada y'
        Write-Host '     winget igual reporta exito.'
        Write-Host '  2. El "&" del principio es obligatorio. Sin el, PowerShell trata la'
        Write-Host '     ruta entre comillas como una cadena y tira ParserError en "modify".'
        Write-Host '  3. NO uses --wait: no existe en el instalador 4.10 y aborta con'
        Write-Host '     exit 87. Sin --wait el comando vuelve enseguida y la instalacion'
        Write-Host '     sigue en segundo plano; --passive te muestra el progreso.'
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
$copyFlag   = if ($NoCopy) { 'OFF' } else { 'ON' }

# Se pasan siempre explicitos para que sobrescriban lo que haya quedado en la
# cache de CMake de un build anterior.
& $cmake -S $root -B $buildDir -G 'Visual Studio 17 2022' -A x64 `
         "-DSTABR_WERROR=$werrorFlag" "-DSTABR_COPY_PLUGIN=$copyFlag"
if ($LASTEXITCODE -ne 0) { throw "Fallo la configuracion de CMake (exit $LASTEXITCODE)" }

& $cmake --build $buildDir --config $Config --parallel
if ($LASTEXITCODE -ne 0) { throw "Fallo la compilacion (exit $LASTEXITCODE)" }

Write-Host ''
Write-Host 'Build OK.' -ForegroundColor Green

$vst3 = Get-ChildItem -Path $buildDir -Recurse -Filter 'STABR.vst3' -ErrorAction SilentlyContinue | Select-Object -First 1
if ($vst3) { Write-Host "VST3:  $($vst3.FullName)" }

$tests = Get-ChildItem -Path $buildDir -Recurse -Filter 'STABR_Tests.exe' -ErrorAction SilentlyContinue | Select-Object -First 1
if ($tests) { Write-Host "Tests: $($tests.FullName)" }
