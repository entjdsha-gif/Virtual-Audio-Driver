@echo off
REM ============================================================
REM  postbuild.bat - Legacy postbuild (Main.vcxproj only)
REM
REM  CableA.vcxproj and CableB.vcxproj produce aocablea.sys
REM  and aocableb.sys directly — no postbuild copy needed.
REM
REM  This script is only used by Main.vcxproj (unified build).
REM ============================================================
setlocal

set PLATFORM=%~1
set CONFIG=%~2

if "%PLATFORM%"=="" (
    echo [PostBuild] ERROR: Platform not specified. Usage: postbuild.bat x64 Release
    exit /b 1
)
if "%CONFIG%"=="" (
    echo [PostBuild] ERROR: Configuration not specified. Usage: postbuild.bat x64 Release
    exit /b 1
)

echo [PostBuild] Platform=%PLATFORM%  Config=%CONFIG%
echo [PostBuild] NOTE: For separate Cable A/B builds, use CableA.vcxproj and CableB.vcxproj.
echo [PostBuild] Done.
exit /b 0
