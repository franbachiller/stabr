@echo off
setlocal EnableDelayedExpansion

REM ============================================================
REM  STABR - compilar e instalar
REM
REM  Doble clic y listo: se eleva solo (pide UAC), compila en
REM  Release y copia el VST3 a C:\Program Files\Common Files\VST3
REM
REM  Necesita permisos de administrador SOLO por esa copia: escribir
REM  en Program Files no se puede sin elevar.
REM ============================================================

title STABR - compilar e instalar

REM --- Ubicar el repo: este .bat vive en <repo>\Scripts\ ------
set "REPO=%~dp0.."
REM Si se lo copio al escritorio, el repo esta en la ruta de al lado.
if not exist "%REPO%\CMakeLists.txt" set "REPO=%~dp0STABR"
if not exist "%REPO%\CMakeLists.txt" set "REPO=%USERPROFILE%\Desktop\STABR"

if not exist "%REPO%\CMakeLists.txt" (
    echo.
    echo  ERROR: no encuentro el proyecto STABR.
    echo  Busque en:
    echo    %~dp0..
    echo    %~dp0STABR
    echo    %USERPROFILE%\Desktop\STABR
    echo.
    echo  Mové este archivo al escritorio, al lado de la carpeta STABR.
    echo.
    pause
    exit /b 1
)

REM --- Elevarse si no somos admin -----------------------------
net session >nul 2>&1
if %errorlevel% neq 0 (
    echo Pidiendo permisos de administrador...
    powershell -NoProfile -Command "Start-Process -FilePath '%~f0' -Verb RunAs"
    exit /b
)

cd /d "%REPO%"

echo.
echo  ============================================
echo   STABR - compilando en Release
echo   %REPO%
echo  ============================================
echo.
echo  La primera vez tarda varios minutos.
echo.

powershell -NoProfile -ExecutionPolicy Bypass -File "%REPO%\Scripts\build.ps1" -Config Release
set "BUILDCODE=%errorlevel%"

echo.
if "%BUILDCODE%"=="0" (
    echo  ============================================
    echo   LISTO. El VST3 quedo instalado en:
    echo   C:\Program Files\Common Files\VST3\STABR.vst3
    echo  ============================================
    echo.
    echo  Abri tu DAW y rescanea los plugins.
) else (
    echo  ============================================
    echo   FALLO ^(codigo %BUILDCODE%^)
    echo  ============================================
    echo.
    echo  Causa mas comun: tenes el DAW abierto con STABR cargado,
    echo  y el archivo .vst3 queda bloqueado. Cerra el DAW y proba
    echo  de nuevo.
)

echo.
pause
