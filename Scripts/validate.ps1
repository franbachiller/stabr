<#
    STABR - validacion con pluginval en strictness 10.

    Descarga pluginval a Tools/ la primera vez (no se commitea) y corre la
    validacion sobre el VST3 compilado.

    Los sample rates y block sizes NO son los de fabrica: se fuerzan los del
    brief (hasta 192 kHz, buffers de 32 a 2048) porque el plugin promete
    funcionar en ese rango y la validacion tiene que probarlo.

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
    [int]$RepeatCount = 1,
    [switch]$SkipGuiTests
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
        Where-Object { $_.FullName -like "*$Config*" -and $_.PSIsContainer } |
        Select-Object -First 1

if (-not $vst3) {
    $vst3 = Get-ChildItem -Path $buildDir -Recurse -Filter 'STABR.vst3' -ErrorAction SilentlyContinue |
            Select-Object -First 1
}

if (-not $vst3) {
    throw "No se encontro STABR.vst3 en $buildDir. Compila primero con .\Scripts\build.ps1"
}

$logDir = Join-Path $buildDir 'pluginval'
New-Item -ItemType Directory -Force -Path $logDir | Out-Null
Get-ChildItem $logDir -Filter '*.txt' -ErrorAction SilentlyContinue | Remove-Item -Force

Write-Host "pluginval:  $pluginval"
Write-Host "plugin:     $($vst3.FullName)"
Write-Host "strictness: $Strictness"
Write-Host ''

# Rango del brief, no los defaults de pluginval.
$sampleRates = '44100,48000,88200,96000,176400,192000'
$blockSizes  = '32,64,128,256,512,1024,2048'

$pvArgs = @(
    '--strictness-level', $Strictness
    '--sample-rates',     $sampleRates
    '--block-sizes',      $blockSizes
    '--repeat',           $RepeatCount
    '--timeout-ms',       '120000'
    '--output-dir',       $logDir
    '--verbose'
)

if ($SkipGuiTests) { $pvArgs += '--skip-gui-tests' }

$pvArgs += @('--validate', $vst3.FullName)

# pluginval.exe es un binario de subsistema GUI: si se invoca con "&", PowerShell
# no espera a que termine y $LASTEXITCODE queda vacio. Start-Process -Wait -PassThru
# es la unica forma confiable de obtener el codigo de salida real.
$proc = Start-Process -FilePath $pluginval -ArgumentList $pvArgs -Wait -PassThru -NoNewWindow
$code = $proc.ExitCode

$log = Get-ChildItem $logDir -Filter '*.txt' -ErrorAction SilentlyContinue |
       Sort-Object LastWriteTime -Descending |
       Select-Object -First 1

if ($log) {
    Write-Host "log: $($log.FullName)"
    Write-Host ''

    if ($code -ne 0) {
        # Solo el contexto de los fallos: el log completo en strictness 10 es enorme.
        Get-Content $log.FullName | Select-String -Pattern '\*\*\* FAILED|!!!|Error|exception' -Context 0, 4
    } else {
        Get-Content $log.FullName | Select-String -Pattern 'Testing:|All tests completed|Time taken' |
            Select-Object -Last 20
    }
}

Write-Host ''
if ($code -eq 0) {
    Write-Host "pluginval OK (strictness $Strictness)." -ForegroundColor Green
} else {
    Write-Host "pluginval FALLO (exit $code)." -ForegroundColor Red
}

exit $code
