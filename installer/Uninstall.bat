@echo off
:: AO Virtual Cable - Uninstall
:: Double-click this file to remove AO Virtual Cable.

set "SCRIPT_DIR=%~dp0"
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%SCRIPT_DIR%install-core.ps1" -Action uninstall

exit /b %errorlevel%
