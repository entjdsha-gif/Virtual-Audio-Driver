@echo off
:: AO Virtual Cable - One-Click Setup
:: Double-click this file to install or upgrade.
:: Requires Administrator privileges (will prompt for elevation).

setlocal

set "SCRIPT_DIR=%~dp0"

:: Detect existing installation
sc query AOCableA >nul 2>&1
if %errorlevel% equ 0 (
    set "ACTION=upgrade"
    echo AO Virtual Cable is installed - upgrading...
) else (
    set "ACTION=install"
    echo Installing AO Virtual Cable...
)

:: Launch PowerShell installer (self-elevating)
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%SCRIPT_DIR%install-core.ps1" -Action %ACTION%

exit /b %errorlevel%
