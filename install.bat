@echo off
REM ============================================================
REM  install.bat - AO Virtual Cable Installer (wrapper)
REM
REM  This is a thin wrapper around install.ps1.
REM  For full control use: powershell -File install.ps1 -Action install
REM
REM  Usage:
REM    install.bat              - Fresh install (Release)
REM    install.bat upgrade      - Upgrade existing (Release)
REM    install.bat uninstall    - Remove all components
REM    install.bat cleanup      - Remove stale AO Driver Store packages
REM    install.bat debug        - Fresh install (Debug build)
REM    install.bat upgrade debug - Upgrade with Debug build
REM    install.bat upgrade debug autoreboot - Queue resume after reboot and restart automatically
REM ============================================================
setlocal

REM Parse arguments
set ACTION=install
set CONFIG=Release
set EXTRA_ARGS=

:parse_args
if "%~1"=="" goto :done_args
if /i "%~1"=="upgrade"   set ACTION=upgrade& shift & goto :parse_args
if /i "%~1"=="uninstall" set ACTION=uninstall& shift & goto :parse_args
if /i "%~1"=="cleanup"   set ACTION=cleanup& shift & goto :parse_args
if /i "%~1"=="debug"     set CONFIG=Debug& shift & goto :parse_args
if /i "%~1"=="release"   set CONFIG=Release& shift & goto :parse_args
if /i "%~1"=="autoreboot" set EXTRA_ARGS=%EXTRA_ARGS% -AutoReboot& shift & goto :parse_args
shift
goto :parse_args
:done_args

REM Admin check
net session >nul 2>&1
if %errorlevel% neq 0 (
    echo [ERROR] Run as Administrator.
    exit /b 1
)

REM Run PowerShell installer
powershell -ExecutionPolicy Bypass -File "%~dp0install.ps1" -Action %ACTION% -Config %CONFIG% %EXTRA_ARGS%
set RESULT=%errorlevel%

if %RESULT% neq 0 (
    echo.
    echo Installation encountered issues.
    echo Run: powershell -File verify-install.ps1
    echo.
    pause
)

endlocal
exit /b %RESULT%
