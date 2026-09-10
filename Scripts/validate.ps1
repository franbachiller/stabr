<#
    STABR - validacion con pluginval en strictness 10.

    Descarga pluginval a Tools/ la primera vez (no se commitea) y corre la
    validacion sobre el VST3 compilado.

    Uso:
        .\Scripts\validate.ps1
        .\Scripts\validate.ps1 -Config Debug
        .\Scripts\validate.ps1 -Strictness 5    # solo para diagnosticar
#>

[CmdletBinding()]
param(
    [ValidateSet('Debug', 'Release', 'RelWithDebInfo')]
    [string]$Config = 'Release',
    [ValidateRange(1, 10)]
    [int]$Strictness = 10,
    [int]$RepeatCount = 1
)

$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $PSScriptRoot
$toolsDir = Join-Path $root 'Tools'
$pluginval = Join-Path $toolsDir 'pluginval.exe'

if (-not (Test-Path $pluginval)) {
    Write-Host 'pluginval no esta. Descargando de github.com/Tracktion/pluginval ...'
    New-Item -ItemType Directory -Force -Path $toolsDir | Out-Null

    $zip = Join-Path $toolsDir 'pluginval.zip'
    $url = 'https://github.com/Tracktion/pluginval/releases/latest/download/pluginval_Windows.zip'

    Invoke-WebRequest -Uri $url -OutFile $zip
    Expand-Archive -Path $zip -DestinationPath $toolsDir -Force
    Remove-Item $zip -Force

    if (-not (Test-Path $pluginval)) {
        throw "El zip de pluginval no contenia pluginval.exe. Revisa $toolsDir"
    }
}

$buildDir = Join-Path $root 'build'
$vst3 = Get-ChildItem -Path $buildDir -Recurse -Filter 'STABR.vst3' -ErrorAction SilentlyContinue |
        Where-Object { $_.FullName -like "*$Config*" } |
        Select-Object -First 1

if (-not $vst3) {
    $vst3 = Get-ChildItem -Path $buildDir -Recurse -Filter 'STABR.vst3' -ErrorAction SilentlyContinue |
            Select-Object -First 1
}

if (-not $vst3) {
    throw "No se encontro STABR.vst3 en $buildDir. Compila primero con .\Scripts\build.ps1"
}

Write-Host "pluginval:  $pluginval"
Write-Host "plugin:     $($vst3.FullName)"
Write-Host "strictness: $Strictness"
Write-Host ''

$env:STRICTNESS_LEVEL = $Strictness

& $pluginval --validate-in-process `
             --strictness-level $Strictness `
             --repeat $RepeatCount `
             --validate $vst3.FullName

$code = $LASTEXITCODE

Write-Host ''
if ($code -eq 0) {
    Write-Host "pluginval OK (strictness $Strictness)." -ForegroundColor Green
} else {
    Write-Host "pluginval FALLO (exit $code)." -ForegroundColor Red
}

exit $code
