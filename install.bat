@echo off
REM ============================================================
REM  install.bat - AO Virtual Cable A+B installer
REM  Run as Administrator.
REM
REM  Flow:
REM    1. Admin check + tool discovery
REM    2. Save current default audio devices
REM    3. Remove old devices + clean OEM packages
REM    4. Stop services + copy .sys (reboot if locked)
REM    5. Sign .sys + create/sign .cat
REM    6. devcon install Cable A + Cable B
REM    7. Restore original default audio devices
REM ============================================================
setlocal enabledelayedexpansion

set PKGDIR_A=%~dp0Source\Main\x64\Release\CableA
set PKGDIR_B=%~dp0Source\Main\x64\Release\CableB
set CERTNAME=AO Audio Test

REM ============================================================
REM  Step 0: Admin check
REM ============================================================
net session >nul 2>&1
if %errorlevel% neq 0 (
    echo [ERROR] Run as Administrator.
    exit /b 1
)

echo ============================================================
echo  AO Virtual Cable Installer
echo ============================================================
echo.

REM ============================================================
REM  Step 1: Verify files + find tools
REM ============================================================
echo [1/7] Checking files and tools...

for %%F in ("%PKGDIR_A%\aocablea.sys" "%PKGDIR_A%\aocablea.inf" "%PKGDIR_B%\aocableb.sys" "%PKGDIR_B%\aocableb.inf") do (
    if not exist %%F (
        echo [ERROR] Missing: %%F
        exit /b 1
    )
)
fc /b "%PKGDIR_A%\aocablea.sys" "%PKGDIR_B%\aocableb.sys" >nul 2>&1
if %errorlevel% equ 0 (
    echo [ERROR] aocablea.sys and aocableb.sys are IDENTICAL!
    exit /b 1
)
echo       Files OK.

set SIGNTOOL=
set INF2CAT=
set DEVCON=

for %%D in (
    "C:\Program Files (x86)\Windows Kits\10\bin\10.0.26100.0\x64"
    "C:\Program Files (x86)\Windows Kits\10\bin\10.0.22621.0\x64"
) do (
    if exist "%%~D\signtool.exe" if "!SIGNTOOL!"=="" set "SIGNTOOL=%%~D\signtool.exe"
)
for %%D in (
    "C:\Program Files (x86)\Windows Kits\10\bin\10.0.26100.0\x86"
    "C:\Program Files (x86)\Windows Kits\10\bin\10.0.22621.0\x86"
    "C:\Program Files (x86)\Windows Kits\10\bin\10.0.26100.0\x64"
) do (
    if exist "%%~D\inf2cat.exe" if "!INF2CAT!"=="" set "INF2CAT=%%~D\inf2cat.exe"
)
for %%D in (
    "C:\Program Files (x86)\Windows Kits\10\Tools\10.0.26100.0\x64"
    "C:\Program Files (x86)\Windows Kits\10\Tools\10.0.22621.0\x64"
) do (
    if exist "%%~D\devcon.exe" if "!DEVCON!"=="" set "DEVCON=%%~D\devcon.exe"
)
if "!SIGNTOOL!"=="" where signtool.exe >nul 2>&1 && for /f "delims=" %%P in ('where signtool.exe') do set "SIGNTOOL=%%P"
if "!DEVCON!"=="" where devcon.exe >nul 2>&1 && for /f "delims=" %%P in ('where devcon.exe') do set "DEVCON=%%P"

if "!SIGNTOOL!"=="" (
    echo [ERROR] signtool.exe not found.
    exit /b 1
)
echo       signtool: !SIGNTOOL!
if "!INF2CAT!"=="" ( echo       inf2cat:  not found - skip .cat ) else ( echo       inf2cat:  !INF2CAT! )
if "!DEVCON!"=="" ( echo       devcon:   not found - use pnputil ) else ( echo       devcon:   !DEVCON! )

REM ============================================================
REM  Step 2: Save current default audio devices
REM ============================================================
echo.
echo [2/7] Saving current default audio devices...

REM Save default render + capture device names (before Cable install steals them)
for /f "usebackq delims=" %%L in (`powershell -NoProfile -Command ^
    "Get-AudioDevice -Playback 2>$null | Select-Object -ExpandProperty Name 2>$null"`) do (
    set "ORIG_RENDER=%%L"
)
for /f "usebackq delims=" %%L in (`powershell -NoProfile -Command ^
    "Get-AudioDevice -Recording 2>$null | Select-Object -ExpandProperty Name 2>$null"`) do (
    set "ORIG_CAPTURE=%%L"
)

if defined ORIG_RENDER (
    echo       Render:  !ORIG_RENDER!
) else (
    echo       Render:  ^(could not detect - will skip restore^)
)
if defined ORIG_CAPTURE (
    echo       Capture: !ORIG_CAPTURE!
) else (
    echo       Capture: ^(could not detect - will skip restore^)
)

REM ============================================================
REM  Step 3: Remove old devices + clean OEM packages
REM ============================================================
echo.
echo [3/7] Removing old devices and cleaning OEM packages...

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
echo       Old devices removed.

REM Clean stale OEM driver packages (aocable*)
echo       Cleaning OEM driver packages...
for /f "tokens=1" %%P in ('pnputil /enum-drivers 2^>nul ^| findstr /i "oem"') do (
    pnputil /enum-drivers 2>nul | findstr /i /c:"%%P" /c:"aocable" >nul 2>&1
    REM Need to match oem*.inf that contain aocable
)
REM More reliable: enumerate and grep
powershell -NoProfile -Command ^
    "$pkgs = pnputil /enum-drivers 2>$null;" ^
    "$lines = $pkgs -split [Environment]::NewLine;" ^
    "$curOem = $null;" ^
    "foreach ($l in $lines) {" ^
    "  if ($l -match '(oem\d+\.inf)') { $curOem = $Matches[1] }" ^
    "  if ($curOem -and $l -match 'aocable') {" ^
    "    Write-Host \"       Removing stale package: $curOem\";" ^
    "    pnputil /delete-driver $curOem /force 2>$null | Out-Null;" ^
    "    $curOem = $null" ^
    "  }" ^
    "}"
echo       OEM cleanup done.

REM ============================================================
REM  Step 4: Stop services + copy .sys
REM ============================================================
echo.
echo [4/7] Stopping services and copying drivers...

sc stop AOCableA >nul 2>&1
sc stop AOCableB >nul 2>&1
sc stop VirtualAudioDriver >nul 2>&1

REM Poll for services to stop (max 10 seconds) instead of fixed timeout
set /a WAIT_COUNT=0
:WAIT_SERVICES
sc query AOCableA 2>nul | findstr /i "RUNNING" >nul 2>&1 && goto :STILL_RUNNING
sc query AOCableB 2>nul | findstr /i "RUNNING" >nul 2>&1 && goto :STILL_RUNNING
goto :SERVICES_STOPPED
:STILL_RUNNING
set /a WAIT_COUNT+=1
if !WAIT_COUNT! GEQ 20 (
    echo       [WARN] Services did not stop within 10 seconds.
    goto :SERVICES_STOPPED
)
ping -n 1 -w 500 127.255.255.255 >nul 2>&1
goto :WAIT_SERVICES
:SERVICES_STOPPED

set COPY_FAILED=0

copy /Y "%PKGDIR_A%\aocablea.sys" "%SystemRoot%\System32\drivers\aocablea.sys" >nul 2>&1
if !errorlevel! neq 0 set COPY_FAILED=1

copy /Y "%PKGDIR_B%\aocableb.sys" "%SystemRoot%\System32\drivers\aocableb.sys" >nul 2>&1
if !errorlevel! neq 0 set COPY_FAILED=1

if !COPY_FAILED! equ 1 (
    echo.
    echo [ERROR] Driver .sys is locked by the OS.
    echo         The old driver is still loaded in memory.
    echo.
    echo         Please REBOOT first, then run install.bat again.
    echo         Command: shutdown /r /t 0
    echo.
    exit /b 1
)
echo       Drivers copied to System32\drivers.

REM ============================================================
REM  Step 5: Sign .sys + create/sign .cat
REM ============================================================
echo.
echo [5/7] Signing drivers...

for %%F in ("%PKGDIR_A%\aocablea.sys" "%PKGDIR_B%\aocableb.sys") do (
    "!SIGNTOOL!" sign /v /s My /n "%CERTNAME%" /fd SHA256 "%%~F" >nul 2>&1
)
echo       .sys files signed.

if not defined INF2CAT goto skip_cat

del /f "%PKGDIR_A%\aocablea.cat" >nul 2>&1
del /f "%PKGDIR_B%\aocableb.cat" >nul 2>&1
"!INF2CAT!" /driver:"%PKGDIR_A%" /os:10_X64 /uselocaltime >nul 2>&1
"!INF2CAT!" /driver:"%PKGDIR_B%" /os:10_X64 /uselocaltime >nul 2>&1
if exist "%PKGDIR_A%\aocablea.cat" "!SIGNTOOL!" sign /v /s My /n "%CERTNAME%" /fd SHA256 "%PKGDIR_A%\aocablea.cat" >nul 2>&1
if exist "%PKGDIR_B%\aocableb.cat" "!SIGNTOOL!" sign /v /s My /n "%CERTNAME%" /fd SHA256 "%PKGDIR_B%\aocableb.cat" >nul 2>&1
echo       cat files created and signed.
goto done_cat

:skip_cat
echo       Skipped cat - inf2cat not found.

:done_cat

REM ============================================================
REM  Step 6: Test signing + Install drivers
REM ============================================================
echo.
echo [6/7] Installing drivers...

bcdedit /enum {current} 2>nul | findstr /i "testsigning.*Yes" >nul 2>&1
if %errorlevel% neq 0 bcdedit /set testsigning on >nul 2>&1

if not defined DEVCON goto install_pnputil

echo       Installing AO Cable A...
"!DEVCON!" install "%PKGDIR_A%\aocablea.inf" ROOT\AOCableA
if !errorlevel! neq 0 echo [WARN] Cable A may have failed.

echo       Installing AO Cable B...
"!DEVCON!" install "%PKGDIR_B%\aocableb.inf" ROOT\AOCableB
if !errorlevel! neq 0 echo [WARN] Cable B may have failed.
goto install_done

:install_pnputil
echo       Installing AO Cable A via pnputil...
pnputil /add-driver "%PKGDIR_A%\aocablea.inf" /install
echo       Installing AO Cable B via pnputil...
pnputil /add-driver "%PKGDIR_B%\aocableb.inf" /install

:install_done
echo       Both drivers installed.

REM ============================================================
REM  Step 7: Restore default audio devices
REM ============================================================
echo.
echo [7/7] Restoring default audio devices...

REM Wait for Windows to finish enumerating new endpoints
timeout /t 3 /nobreak >nul

REM Try AudioDeviceCmdlets first (Install-Module AudioDeviceCmdlets)
REM Fallback: use nircmd or manual instruction
powershell -NoProfile -Command ^
    "$mod = Get-Module -ListAvailable AudioDeviceCmdlets 2>$null;" ^
    "if (-not $mod) {" ^
    "  Write-Host '       AudioDeviceCmdlets not installed.';" ^
    "  Write-Host '       Install: Install-Module AudioDeviceCmdlets -Force';" ^
    "  Write-Host '       Then re-run install.bat or set defaults manually.';" ^
    "  exit 0" ^
    "}" ^
    "Import-Module AudioDeviceCmdlets;" ^
    "$renderName = $env:ORIG_RENDER;" ^
    "$captureName = $env:ORIG_CAPTURE;" ^
    "if ($renderName) {" ^
    "  $dev = Get-AudioDevice -List | Where-Object { $_.Type -eq 'Playback' -and $_.Name -like \"*$renderName*\" } | Select-Object -First 1;" ^
    "  if ($dev) { Set-AudioDevice -ID $dev.ID | Out-Null; Write-Host \"       Render restored: $renderName\" }" ^
    "  else { Write-Host \"       [WARN] Render device '$renderName' not found\" }" ^
    "}" ^
    "if ($captureName) {" ^
    "  $dev = Get-AudioDevice -List | Where-Object { $_.Type -eq 'Recording' -and $_.Name -like \"*$captureName*\" } | Select-Object -First 1;" ^
    "  if ($dev) { Set-AudioDevice -ID $dev.ID | Out-Null; Write-Host \"       Capture restored: $captureName\" }" ^
    "  else { Write-Host \"       [WARN] Capture device '$captureName' not found\" }" ^
    "}"

REM Fallback: also set via registry hint (NeverSetAsDefault should handle this,
REM but RAW mode may bypass it on some Windows builds)
echo.
echo       If Cable is still default, manually change in:
echo       Settings ^> System ^> Sound ^> Output/Input device

REM ============================================================
REM  Done
REM ============================================================
echo.
echo ============================================================
echo  Installation complete!
echo ============================================================
echo.
echo  Devices installed:
echo    - AO Cable A  (Output / Input)
echo    - AO Cable B  (Output / Input)
echo.

if defined NEED_REBOOT (
    echo  [!] Test signing enabled. REBOOT REQUIRED.
    echo      shutdown /r /t 0
    echo.
)

endlocal
exit /b 0
