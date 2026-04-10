/*
 * AO Virtual Cable - Setup Launcher (Setup.exe)
 *
 * Minimal native EXE that:
 *   1. Embeds install-core.ps1 as a resource
 *   2. Extracts it to a temp directory at runtime
 *   3. Launches PowerShell to execute it (elevated if needed)
 *   4. Passes through the exit code
 *   5. Cleans up temp files on exit
 *
 * This hides the PowerShell source from the user-visible package.
 * The .ps1 is only present on disk briefly during execution.
 *
 * Build: compiled as a standard Win32 console/subsystem app.
 * UAC: embedded manifest requests requireAdministrator.
 */

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <shellapi.h>
#include <shlwapi.h>
#include <stdio.h>
#include "resource.h"

#pragma comment(lib, "shlwapi.lib")

// Extract embedded resource to a temp file. Returns path or empty on failure.
static BOOL ExtractResource(HINSTANCE hInst, int resourceId, const wchar_t* destPath)
{
    HRSRC hRes = FindResourceW(hInst, MAKEINTRESOURCEW(resourceId), RT_RCDATA);
    if (!hRes) return FALSE;

    HGLOBAL hGlobal = LoadResource(hInst, hRes);
    if (!hGlobal) return FALSE;

    DWORD size = SizeofResource(hInst, hRes);
    void* data = LockResource(hGlobal);
    if (!data || size == 0) return FALSE;

    HANDLE hFile = CreateFileW(destPath, GENERIC_WRITE, 0, NULL,
        CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hFile == INVALID_HANDLE_VALUE) return FALSE;

    DWORD written = 0;
    BOOL ok = WriteFile(hFile, data, size, &written, NULL);
    CloseHandle(hFile);

    return ok && (written == size);
}

// Determine action: check if AO is already installed
static const wchar_t* DetectAction()
{
    SC_HANDLE scm = OpenSCManagerW(NULL, NULL, SC_MANAGER_CONNECT);
    if (!scm) return L"install";

    SC_HANDLE svc = OpenServiceW(scm, L"AOCableA", SERVICE_QUERY_STATUS);
    CloseServiceHandle(scm);

    if (svc) {
        CloseServiceHandle(svc);
        return L"upgrade";
    }
    return L"install";
}

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE, LPWSTR lpCmdLine, int)
{
    // Parse command line for action override
    const wchar_t* action = NULL;
    int argc = 0;
    LPWSTR* argv = CommandLineToArgvW(GetCommandLineW(), &argc);
    BOOL silent = FALSE;
    BOOL jsonOutput = FALSE;
    BOOL isUninstall = FALSE;

    for (int i = 1; i < argc; i++) {
        if (_wcsicmp(argv[i], L"-Action") == 0 && i + 1 < argc) {
            action = argv[++i];
        }
        else if (_wcsicmp(argv[i], L"-Silent") == 0 || _wcsicmp(argv[i], L"/Silent") == 0) {
            silent = TRUE;
        }
        else if (_wcsicmp(argv[i], L"-JsonOutput") == 0 || _wcsicmp(argv[i], L"/JsonOutput") == 0) {
            jsonOutput = TRUE;
        }
        else if (_wcsicmp(argv[i], L"/uninstall") == 0 || _wcsicmp(argv[i], L"-uninstall") == 0) {
            isUninstall = TRUE;
        }
    }
    LocalFree(argv);

    if (!action) {
        action = isUninstall ? L"uninstall" : DetectAction();
    }

    // Get temp directory
    wchar_t tempDir[MAX_PATH];
    GetTempPathW(MAX_PATH, tempDir);
    PathAppendW(tempDir, L"AOVirtualCable_Setup");
    CreateDirectoryW(tempDir, NULL);

    // Extract install-core.ps1
    wchar_t ps1Path[MAX_PATH];
    wcscpy_s(ps1Path, tempDir);
    PathAppendW(ps1Path, L"install-core.ps1");

    if (!ExtractResource(hInstance, IDR_INSTALL_CORE, ps1Path)) {
        if (!silent) {
            MessageBoxW(NULL, L"Failed to extract installer files.",
                L"AO Virtual Cable", MB_OK | MB_ICONERROR);
        }
        return 50;
    }

    // Copy driver files from EXE directory to temp (they stay alongside EXE)
    // The .ps1 will resolve paths relative to its own location, but
    // drivers/ must be next to the EXE, not in temp.
    // Solution: set scriptDir override via environment variable.
    wchar_t exeDir[MAX_PATH];
    GetModuleFileNameW(NULL, exeDir, MAX_PATH);
    PathRemoveFileSpecW(exeDir);

    // Build PowerShell command line
    wchar_t cmdLine[4096];
    swprintf_s(cmdLine,
        L"powershell.exe -NoProfile -ExecutionPolicy Bypass"
        L" -Command \"$env:AO_PACKAGE_ROOT='%s'; & '%s' -Action %s%s%s\"",
        exeDir, ps1Path, action,
        silent ? L" -Silent" : L"",
        jsonOutput ? L" -JsonOutput" : L"");

    // Launch PowerShell
    STARTUPINFOW si = { sizeof(si) };
    PROCESS_INFORMATION pi = {};

    if (!silent) {
        // Show console window for interactive mode
        si.dwFlags = 0;
    } else {
        // Hidden window for silent mode
        si.dwFlags = STARTF_USESHOWWINDOW;
        si.wShowWindow = SW_HIDE;
    }

    if (!CreateProcessW(NULL, cmdLine, NULL, NULL, FALSE,
        CREATE_NEW_CONSOLE, NULL, exeDir, &si, &pi))
    {
        if (!silent) {
            MessageBoxW(NULL, L"Failed to launch PowerShell.",
                L"AO Virtual Cable", MB_OK | MB_ICONERROR);
        }
        DeleteFileW(ps1Path);
        RemoveDirectoryW(tempDir);
        return 50;
    }

    // Wait for completion
    WaitForSingleObject(pi.hProcess, INFINITE);

    DWORD exitCode = 50;
    GetExitCodeProcess(pi.hProcess, &exitCode);

    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);

    // Cleanup temp
    DeleteFileW(ps1Path);
    RemoveDirectoryW(tempDir);

    return (int)exitCode;
}
