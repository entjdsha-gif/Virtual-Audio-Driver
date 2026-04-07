@echo off
REM ============================================================
REM  uninstall.bat — Remove AO Virtual Cable A+B
REM  Run as Administrator.
REM ============================================================
setlocal enabledelayedexpansion

net session >nul 2>&1
if %errorlevel% neq 0 (
    echo [ERROR] Run as Administrator.
    exit /b 1
)

echo Removing AO Virtual Cable devices...

REM -- Find devcon
set DEVCON=
for %%D in (
    "C:\Program Files (x86)\Windows Kits\10\Tools\10.0.26100.0\x64"
    "C:\Program Files (x86)\Windows Kits\10\Tools\10.0.22621.0\x64"
    "C:\Program Files (x86)\Windows Kits\10\Tools\x64"
) do (
    if exist "%%~D\devcon.exe" if "!DEVCON!"=="" set "DEVCON=%%~D\devcon.exe"
)
if "!DEVCON!"=="" (
    where devcon.exe >nul 2>&1 && for /f "delims=" %%P in ('where devcon.exe') do set "DEVCON=%%P"
)

if defined DEVCON (
    "!DEVCON!" remove ROOT\AOCableA >nul 2>&1
    "!DEVCON!" remove ROOT\AOCableB >nul 2>&1
    "!DEVCON!" remove ROOT\AOVirtualCable >nul 2>&1
    "!DEVCON!" remove ROOT\AOVirtualAudio >nul 2>&1
) else (
    pnputil /remove-device /deviceid ROOT\AOCableA >nul 2>&1
    pnputil /remove-device /deviceid ROOT\AOCableB >nul 2>&1
    pnputil /remove-device /deviceid ROOT\AOVirtualCable >nul 2>&1
    pnputil /remove-device /deviceid ROOT\AOVirtualAudio >nul 2>&1
)

sc stop AOCableA >nul 2>&1
sc delete AOCableA >nul 2>&1
sc stop AOCableB >nul 2>&1
sc delete AOCableB >nul 2>&1
sc stop VirtualAudioDriver >nul 2>&1
sc delete VirtualAudioDriver >nul 2>&1

del /f "%SystemRoot%\System32\drivers\aocablea.sys" >nul 2>&1
del /f "%SystemRoot%\System32\drivers\aocableb.sys" >nul 2>&1
del /f "%SystemRoot%\System32\drivers\virtualaudiodriver.sys" >nul 2>&1

echo Done. All AO Virtual Cable devices removed.
endlocal
exit /b 0
