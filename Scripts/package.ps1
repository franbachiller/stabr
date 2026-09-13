<#
    STABR - genera el instalador de Windows.

    1. Compila el plugin en Release (sin copiarlo al sistema).
    2. Lee la version del CMakeLists.txt: una sola fuente de verdad.
    3. Compila Installer\STABR.iss con Inno Setup 6.

    Resultado: build\installer\STABR-<version>-Windows-x64-Setup.exe

    Uso:
        .\Scripts\package.ps1
        .\Scripts\package.ps1 -SkipBuild     # reusa el build existente
#>
param(
    [switch]$SkipBuild
)

$ErrorActionPreference = 'Stop'
$root = Split-Path $PSScriptRoot -Parent

if (-not $SkipBuild) {
    & (Join-Path $PSScriptRoot 'build.ps1') -NoCopy
    if ($LASTEXITCODE -ne 0) { throw "El build fallo (exit $LASTEXITCODE)." }
}

$vst3 = Join-Path $root 'build\STABR_artefacts\Release\VST3\STABR.vst3'
if (-not (Test-Path $vst3)) { throw "No existe $vst3. Compila primero o saca -SkipBuild." }

$manual = Join-Path $root 'docs\manual\STABR-Manual-EN.html'
if (-not (Test-Path $manual)) { throw "Falta el manual en $manual." }

$cmake = Get-Content (Join-Path $root 'CMakeLists.txt') -Raw
if ($cmake -notmatch 'project\(STABR VERSION ([0-9]+\.[0-9]+\.[0-9]+)') { throw 'No encontre la version en CMakeLists.txt.' }
$version = $Matches[1]

$candidates = @(
    (Join-Path ${env:ProgramFiles(x86)} 'Inno Setup 6\ISCC.exe'),
    (Join-Path $env:ProgramFiles 'Inno Setup 6\ISCC.exe'),
    (Join-Path $env:LOCALAPPDATA 'Programs\Inno Setup 6\ISCC.exe')
)
$iscc = $candidates | Where-Object { $_ -and (Test-Path $_) } | Select-Object -First 1

if (-not $iscc) {
    Write-Host ''
    Write-Host 'No encontre Inno Setup 6.' -ForegroundColor Yellow
    Write-Host 'Instalalo desde https://jrsoftware.org/isdl.php y volve a correr este script.'
    exit 1
}

Write-Host "Empaquetando STABR $version con $iscc"
& $iscc "/DAppVersion=$version" (Join-Path $root 'Installer\STABR.iss')
if ($LASTEXITCODE -ne 0) { throw "Inno Setup fallo (exit $LASTEXITCODE)." }

Get-ChildItem (Join-Path $root 'build\installer') -Filter '*.exe' | Select-Object Name, Length, LastWriteTime
