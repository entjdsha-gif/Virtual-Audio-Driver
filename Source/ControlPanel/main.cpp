/*++
Module Name:
    main.cpp
Abstract:
    Entry point and UI logic for AO Virtual Cable Control Panel tray app.
    M3: Configuration, stream monitoring, channel mode selection,
    self-test, driver diagnostics, and device restart workflow.
--*/

#include <windows.h>
#include <commctrl.h>
#include <shellapi.h>
#include <setupapi.h>
#include <cfgmgr32.h>
#include <stdio.h>
#include <shlwapi.h>

#include "resource.h"
#include "device.h"

#pragma comment(lib, "comctl32.lib")
#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "shlwapi.lib")
#pragma comment(lib, "setupapi.lib")
#pragma comment(lib, "cfgmgr32.lib")

// ---------------------------------------------------------------------------
// Constants
// ---------------------------------------------------------------------------

static const WCHAR g_szWindowClass[] = L"AOVirtualCableControlPanel";
static const WCHAR g_szMutexName[]   = L"Global\\AOVirtualCableControlPanelMutex";
static const UINT  WM_TRAYICON      = WM_APP + 1;

static const UINT g_rates[] = { 44100, 48000, 96000, 192000 };
static const WCHAR* g_rateLabels[] = {
    L"44100 Hz", L"48000 Hz", L"96000 Hz", L"192000 Hz"
};

static const UINT g_channelModes[] = { 8, 16 };
static const WCHAR* g_channelLabels[] = {
    L"8 channels", L"16 channels"
};

static const ULONG g_defaultRate       = 48000;
static const ULONG g_defaultLatencyMs  = 20;
static const ULONG g_defaultChannels   = 8;

// ---------------------------------------------------------------------------
// Globals
// ---------------------------------------------------------------------------

static HINSTANCE g_hInst;
static HWND      g_hWndHidden;
static HMENU     g_hTrayMenu;

// ---------------------------------------------------------------------------
// Forward declarations
// ---------------------------------------------------------------------------

static LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);
static INT_PTR CALLBACK SettingsDlgProc(HWND, UINT, WPARAM, LPARAM);
static void    AddTrayIcon(HWND hWnd);
static void    RemoveTrayIcon(HWND hWnd);
static void    ShowSettingsDialog(HWND hWndParent);
static void    FormatEndpointStatus(const AO_ENDPOINT_STATUS* ep, WCHAR* buf, int cch);

// ---------------------------------------------------------------------------
// WinMain
// ---------------------------------------------------------------------------

int WINAPI
wWinMain(
    _In_ HINSTANCE hInstance,
    _In_opt_ HINSTANCE /*hPrevInstance*/,
    _In_ LPWSTR /*lpCmdLine*/,
    _In_ int /*nCmdShow*/)
{
    // Single instance check.
    HANDLE hMutex = CreateMutexW(NULL, TRUE, g_szMutexName);
    if (GetLastError() == ERROR_ALREADY_EXISTS) {
        if (hMutex) {
            CloseHandle(hMutex);
        }
        return 0;
    }

    g_hInst = hInstance;

    // Enable common controls (trackbar support).
    INITCOMMONCONTROLSEX icc = {};
    icc.dwSize = sizeof(icc);
    icc.dwICC  = ICC_BAR_CLASSES;
    InitCommonControlsEx(&icc);

    // Register hidden window class.
    WNDCLASSEXW wc = {};
    wc.cbSize        = sizeof(wc);
    wc.lpfnWndProc   = WndProc;
    wc.hInstance      = hInstance;
    wc.lpszClassName  = g_szWindowClass;
    RegisterClassExW(&wc);

    // Create message-only hidden window.
    g_hWndHidden = CreateWindowExW(
        0, g_szWindowClass, L"", 0,
        0, 0, 0, 0,
        HWND_MESSAGE, NULL, hInstance, NULL);

    if (!g_hWndHidden) {
        if (hMutex) {
            ReleaseMutex(hMutex);
            CloseHandle(hMutex);
        }
        return 1;
    }

    // Build tray context menu.
    g_hTrayMenu = CreatePopupMenu();
    AppendMenuW(g_hTrayMenu, MF_STRING, ID_TRAY_SETTINGS, L"Settings...");
    AppendMenuW(g_hTrayMenu, MF_SEPARATOR, 0, NULL);
    AppendMenuW(g_hTrayMenu, MF_STRING, ID_TRAY_EXIT, L"Exit");

    // Add tray icon.
    AddTrayIcon(g_hWndHidden);

    // Message loop.
    MSG msg;
    while (GetMessageW(&msg, NULL, 0, 0) > 0) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    if (g_hTrayMenu) {
        DestroyMenu(g_hTrayMenu);
    }

    if (hMutex) {
        ReleaseMutex(hMutex);
        CloseHandle(hMutex);
    }

    return (int)msg.wParam;
}

// ---------------------------------------------------------------------------
// Tray icon helpers
// ---------------------------------------------------------------------------

static void
AddTrayIcon(HWND hWnd)
{
    NOTIFYICONDATAW nid = {};
    nid.cbSize           = sizeof(nid);
    nid.hWnd             = hWnd;
    nid.uID              = IDI_TRAYICON;
    nid.uFlags           = NIF_ICON | NIF_MESSAGE | NIF_TIP;
    nid.uCallbackMessage = WM_TRAYICON;
    nid.hIcon            = LoadIcon(NULL, IDI_APPLICATION);
    wcscpy_s(nid.szTip, L"AO Virtual Cable");
    Shell_NotifyIconW(NIM_ADD, &nid);
}

static void
RemoveTrayIcon(HWND hWnd)
{
    NOTIFYICONDATAW nid = {};
    nid.cbSize = sizeof(nid);
    nid.hWnd   = hWnd;
    nid.uID    = IDI_TRAYICON;
    Shell_NotifyIconW(NIM_DELETE, &nid);
}

// ---------------------------------------------------------------------------
// Hidden window procedure
// ---------------------------------------------------------------------------

static LRESULT CALLBACK
WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    switch (msg) {
    case WM_TRAYICON:
        switch (LOWORD(lParam)) {
        case WM_RBUTTONUP:
        {
            POINT pt;
            GetCursorPos(&pt);
            SetForegroundWindow(hWnd);
            TrackPopupMenu(g_hTrayMenu, TPM_RIGHTALIGN | TPM_BOTTOMALIGN,
                           pt.x, pt.y, 0, hWnd, NULL);
            PostMessage(hWnd, WM_NULL, 0, 0);
            break;
        }
        case WM_LBUTTONDBLCLK:
            ShowSettingsDialog(hWnd);
            break;
        }
        return 0;

    case WM_COMMAND:
        switch (LOWORD(wParam)) {
        case ID_TRAY_SETTINGS:
            ShowSettingsDialog(hWnd);
            break;
        case ID_TRAY_EXIT:
            DestroyWindow(hWnd);
            break;
        }
        return 0;

    case WM_DESTROY:
        RemoveTrayIcon(hWnd);
        PostQuitMessage(0);
        return 0;
    }

    return DefWindowProcW(hWnd, msg, wParam, lParam);
}

// ---------------------------------------------------------------------------
// Settings dialog
// ---------------------------------------------------------------------------

static void
ShowSettingsDialog(HWND hWndParent)
{
    DialogBoxParamW(g_hInst, MAKEINTRESOURCEW(IDD_SETTINGS),
                    hWndParent, SettingsDlgProc, 0);
}

// Per-dialog state stored in window user data.
typedef struct _DLG_STATE {
    HANDLE hDevA;
    HANDLE hDevB;
    ULONG  initialChannelMode;  // channel mode at dialog open
} DLG_STATE;

static void
FormatEndpointStatus(const AO_ENDPOINT_STATUS* ep, WCHAR* buf, int cch)
{
    if (!ep->Active) {
        _snwprintf_s(buf, cch, _TRUNCATE, L"Inactive");
    } else {
        _snwprintf_s(buf, cch, _TRUNCATE,
                     L"Active  %lu Hz / %lu-bit / %lu ch",
                     ep->SampleRate, ep->BitsPerSample, ep->Channels);
    }
}

static void
SetLatencyLabel(HWND hDlg, ULONG latencyMs)
{
    WCHAR latBuf[32];
    _snwprintf_s(latBuf, _countof(latBuf), _TRUNCATE, L"%lu ms", latencyMs);
    SetDlgItemTextW(hDlg, IDC_STATIC_LATENCY, latBuf);
}

static void
ApplyDefaultSelections(HWND hDlg)
{
    HWND hRateCombo = GetDlgItem(hDlg, IDC_COMBO_RATE);
    HWND hSlider = GetDlgItem(hDlg, IDC_SLIDER_LATENCY);
    HWND hChCombo = GetDlgItem(hDlg, IDC_COMBO_CHANNELS);

    for (int i = 0; i < (int)_countof(g_rates); i++) {
        if (g_rates[i] == g_defaultRate) {
            SendMessageW(hRateCombo, CB_SETCURSEL, (WPARAM)i, 0);
            break;
        }
    }

    SendMessageW(hSlider, TBM_SETPOS, TRUE, (LPARAM)g_defaultLatencyMs);
    SetLatencyLabel(hDlg, g_defaultLatencyMs);

    for (int i = 0; i < (int)_countof(g_channelModes); i++) {
        if (g_channelModes[i] == g_defaultChannels) {
            SendMessageW(hChCombo, CB_SETCURSEL, (WPARAM)i, 0);
            break;
        }
    }
}

static void
MergeStreamStatus(AO_STREAM_STATUS* dest, const AO_STREAM_STATUS* src)
{
    if (src->CableA_Speaker.Active) dest->CableA_Speaker = src->CableA_Speaker;
    if (src->CableA_Mic.Active)     dest->CableA_Mic = src->CableA_Mic;
    if (src->CableB_Speaker.Active) dest->CableB_Speaker = src->CableB_Speaker;
    if (src->CableB_Mic.Active)     dest->CableB_Mic = src->CableB_Mic;
}

static void
UpdateStreamStatus(HWND hDlg, DLG_STATE* state)
{
    AO_STREAM_STATUS status = {};
    AO_STREAM_STATUS partial = {};
    WCHAR buf[128];

    if (state->hDevA != INVALID_HANDLE_VALUE && AoGetStreamStatus(state->hDevA, &partial)) {
        MergeStreamStatus(&status, &partial);
    }

    ZeroMemory(&partial, sizeof(partial));
    if (state->hDevB != INVALID_HANDLE_VALUE && AoGetStreamStatus(state->hDevB, &partial)) {
        MergeStreamStatus(&status, &partial);
    }

    FormatEndpointStatus(&status.CableA_Speaker, buf, _countof(buf));
    SetDlgItemTextW(hDlg, IDC_STATUS_A_SPK, buf);

    FormatEndpointStatus(&status.CableA_Mic, buf, _countof(buf));
    SetDlgItemTextW(hDlg, IDC_STATUS_A_MIC, buf);

    FormatEndpointStatus(&status.CableB_Speaker, buf, _countof(buf));
    SetDlgItemTextW(hDlg, IDC_STATUS_B_SPK, buf);

    FormatEndpointStatus(&status.CableB_Mic, buf, _countof(buf));
    SetDlgItemTextW(hDlg, IDC_STATUS_B_MIC, buf);
}

// ---------------------------------------------------------------------------
// Runtime state display (from GET_CONFIG + registry)
// ---------------------------------------------------------------------------

static void
FormatRuntimeLine(HANDLE hDev, const WCHAR* regPath, const WCHAR* label, WCHAR* buf, int cch)
{
    if (hDev == INVALID_HANDLE_VALUE) {
        _snwprintf_s(buf, cch, _TRUNCATE, L"%s: not connected", label);
        return;
    }

    AO_CONFIG config = {};
    if (!AoGetConfig(hDev, &config)) {
        _snwprintf_s(buf, cch, _TRUNCATE, L"%s: GET_CONFIG failed", label);
        return;
    }

    ULONG regMaxCh = 8;
    HKEY hKey = NULL;
    if (RegOpenKeyExW(HKEY_LOCAL_MACHINE, regPath, 0, KEY_READ, &hKey) == ERROR_SUCCESS) {
        DWORD val = 0, sz = sizeof(val);
        if (RegQueryValueExW(hKey, L"MaxChannelCount", NULL, NULL, (LPBYTE)&val, &sz) == ERROR_SUCCESS) {
            regMaxCh = val;
        }
        RegCloseKey(hKey);
    }

    _snwprintf_s(buf, cch, _TRUNCATE,
                 L"%s: Rate=%lu  Lat=%lu ms  Bits=%lu  Ch=%lu  MaxCh(reg)=%lu",
                 label, config.InternalRate, config.MaxLatencyMs,
                 config.InternalBits, config.InternalChannels, regMaxCh);
}

static void
UpdateRuntimeState(HWND hDlg, DLG_STATE* state)
{
    WCHAR buf[256];

    FormatRuntimeLine(state->hDevA,
                      L"SYSTEM\\CurrentControlSet\\Services\\AOCableA\\Parameters",
                      L"A", buf, _countof(buf));
    SetDlgItemTextW(hDlg, IDC_RUNTIME_STATE_A, buf);

    FormatRuntimeLine(state->hDevB,
                      L"SYSTEM\\CurrentControlSet\\Services\\AOCableB\\Parameters",
                      L"B", buf, _countof(buf));
    SetDlgItemTextW(hDlg, IDC_RUNTIME_STATE_B, buf);
}

// ---------------------------------------------------------------------------
// Driver info display
// ---------------------------------------------------------------------------

static void
FormatDriverFileInfo(const WCHAR* fileName, WCHAR* buf, int cch)
{
    WCHAR path[MAX_PATH];
    WCHAR sysRoot[MAX_PATH] = {};
    GetEnvironmentVariableW(L"SystemRoot", sysRoot, _countof(sysRoot));
    _snwprintf_s(path, _countof(path), _TRUNCATE,
                 L"%s\\System32\\drivers\\%s",
                 sysRoot, fileName);

    WIN32_FILE_ATTRIBUTE_DATA fad = {};
    if (!GetFileAttributesExW(path, GetFileExInfoStandard, &fad)) {
        _snwprintf_s(buf, cch, _TRUNCATE, L"%s: not found", fileName);
        return;
    }

    ULARGE_INTEGER fileSize;
    fileSize.LowPart  = fad.nFileSizeLow;
    fileSize.HighPart = fad.nFileSizeHigh;

    SYSTEMTIME st;
    FileTimeToSystemTime(&fad.ftLastWriteTime, &st);

    _snwprintf_s(buf, cch, _TRUNCATE,
                 L"%s   %llu KB   %04u-%02u-%02u %02u:%02u",
                 fileName,
                 fileSize.QuadPart / 1024,
                 st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute);
}

static void
UpdateDriverInfo(HWND hDlg)
{
    WCHAR buf[256];
    FormatDriverFileInfo(L"aocablea.sys", buf, _countof(buf));
    SetDlgItemTextW(hDlg, IDC_DRIVER_INFO_A, buf);

    FormatDriverFileInfo(L"aocableb.sys", buf, _countof(buf));
    SetDlgItemTextW(hDlg, IDC_DRIVER_INFO_B, buf);
}

// ---------------------------------------------------------------------------
// Device restart via pnputil (elevated)
// ---------------------------------------------------------------------------

// ---------------------------------------------------------------------------
// Find device instance ID by matching hardware ID property.
// Enumerates all present ROOT devices, compares SPDRP_HARDWAREID
// against hwIdTarget (e.g. "ROOT\\AOCableA"), returns the instance ID.
// ---------------------------------------------------------------------------
static BOOL
FindDeviceInstanceId(const WCHAR* hwIdTarget, WCHAR* instanceId, DWORD cchInstanceId)
{
    HDEVINFO hDevInfo = SetupDiGetClassDevsW(
        NULL, L"ROOT", NULL, DIGCF_ALLCLASSES | DIGCF_PRESENT);
    if (hDevInfo == INVALID_HANDLE_VALUE) {
        return FALSE;
    }

    SP_DEVINFO_DATA devInfoData = {};
    devInfoData.cbSize = sizeof(devInfoData);
    BOOL found = FALSE;

    for (DWORD idx = 0;
         SetupDiEnumDeviceInfo(hDevInfo, idx, &devInfoData);
         idx++)
    {
        // Get hardware ID multi-sz property.
        WCHAR hwIds[512] = {};
        DWORD propType = 0;
        if (!SetupDiGetDeviceRegistryPropertyW(
                hDevInfo, &devInfoData, SPDRP_HARDWAREID,
                &propType, (PBYTE)hwIds, sizeof(hwIds), NULL))
        {
            continue;
        }

        // Walk the multi-sz list and compare each string.
        for (const WCHAR* p = hwIds; *p; p += wcslen(p) + 1) {
            if (_wcsicmp(p, hwIdTarget) == 0) {
                found = SetupDiGetDeviceInstanceIdW(
                    hDevInfo, &devInfoData,
                    instanceId, cchInstanceId, NULL);
                break;
            }
        }
        if (found) break;
    }

    SetupDiDestroyDeviceInfoList(hDevInfo);
    return found;
}

// ---------------------------------------------------------------------------
// Restart a device via pnputil using its actual instance ID.
// Returns TRUE only if pnputil exits with code 0.
// ---------------------------------------------------------------------------
static BOOL
RestartDeviceElevated(HWND hWndParent, const WCHAR* hwIdPrefix)
{
    // Look up the real instance ID (e.g. "ROOT\AOCABLEA\0000").
    WCHAR instanceId[256] = {};
    if (!FindDeviceInstanceId(hwIdPrefix, instanceId, _countof(instanceId))) {
        return FALSE;
    }

    WCHAR args[512];
    _snwprintf_s(args, _countof(args), _TRUNCATE,
                 L"/restart-device \"%s\"", instanceId);

    SHELLEXECUTEINFOW sei = {};
    sei.cbSize       = sizeof(sei);
    sei.fMask        = SEE_MASK_NOCLOSEPROCESS;
    sei.hwnd         = hWndParent;
    sei.lpVerb       = L"runas";
    sei.lpFile       = L"pnputil.exe";
    sei.lpParameters = args;
    sei.nShow        = SW_HIDE;

    if (!ShellExecuteExW(&sei)) {
        return FALSE;
    }

    if (!sei.hProcess) {
        return FALSE;
    }

    DWORD waitResult = WaitForSingleObject(sei.hProcess, 15000);
    DWORD exitCode = (DWORD)-1;
    GetExitCodeProcess(sei.hProcess, &exitCode);
    CloseHandle(sei.hProcess);

    // Timeout or non-zero exit = failure.
    if (waitResult != WAIT_OBJECT_0 || exitCode != 0) {
        return FALSE;
    }

    return TRUE;
}

static void
DoSetChannelAndRestart(HWND hDlg, DLG_STATE* state)
{
    HWND hCombo = GetDlgItem(hDlg, IDC_COMBO_CHANNELS);
    int sel = (int)SendMessageW(hCombo, CB_GETCURSEL, 0, 0);
    ULONG channels = (sel >= 0 && sel < (int)_countof(g_channelModes))
                     ? g_channelModes[sel] : 8;

    // Write channel count to registry via IOCTL on each available device.
    BOOL setA = TRUE, setB = TRUE;
    BOOL hasA = (state->hDevA != INVALID_HANDLE_VALUE);
    BOOL hasB = (state->hDevB != INVALID_HANDLE_VALUE);

    if (hasA) setA = AoSetMaxChannels(state->hDevA, channels);
    if (hasB) setB = AoSetMaxChannels(state->hDevB, channels);

    // Report per-device IOCTL result.
    if ((!hasA || setA) && (!hasB || setB)) {
        // All accessible devices succeeded.
    } else {
        WCHAR msg[256];
        _snwprintf_s(msg, _countof(msg), _TRUNCATE,
                     L"Failed to write MaxChannelCount:\n  Cable A: %s\n  Cable B: %s",
                     hasA ? (setA ? L"OK" : L"FAILED") : L"not present",
                     hasB ? (setB ? L"OK" : L"FAILED") : L"not present");
        MessageBoxW(hDlg, msg, L"AO Virtual Cable", MB_ICONERROR | MB_OK);
        return;
    }

    // Confirm restart.
    int answer = MessageBoxW(hDlg,
        L"MaxChannelCount has been written to registry.\n"
        L"The audio device must be restarted for the change to take effect.\n\n"
        L"Restart device now? (requires elevation)",
        L"AO Virtual Cable", MB_ICONQUESTION | MB_YESNO);

    if (answer != IDYES) {
        return;
    }

    // Close device handles before restart.
    if (state->hDevA != INVALID_HANDLE_VALUE) {
        CloseHandle(state->hDevA);
        state->hDevA = INVALID_HANDLE_VALUE;
    }
    if (state->hDevB != INVALID_HANDLE_VALUE) {
        CloseHandle(state->hDevB);
        state->hDevB = INVALID_HANDLE_VALUE;
    }

    // Restart each cable device independently, track per-device result.
    SetDlgItemTextW(hDlg, IDC_RUNTIME_STATE_A, L"Restarting devices...");
    SetDlgItemTextW(hDlg, IDC_RUNTIME_STATE_B, L"");
    UpdateWindow(hDlg);

    BOOL rstA = FALSE, rstB = FALSE;
    if (hasA) rstA = RestartDeviceElevated(hDlg, L"ROOT\\AOCableA");
    if (hasB) rstB = RestartDeviceElevated(hDlg, L"ROOT\\AOCableB");

    // Wait for devices to come back online.
    Sleep(2000);

    // Re-open device handles.
    state->hDevA = AoOpenDevice(0);
    state->hDevB = AoOpenDevice(1);

    // Refresh all displays.
    UpdateRuntimeState(hDlg, state);
    UpdateStreamStatus(hDlg, state);
    UpdateDriverInfo(hDlg);

    // Verify channel mode on BOTH devices independently.
    AO_CONFIG cfgA = {}, cfgB = {};
    BOOL verifyA = FALSE, verifyB = FALSE;

    if (state->hDevA != INVALID_HANDLE_VALUE && AoGetConfig(state->hDevA, &cfgA)) {
        verifyA = (cfgA.InternalChannels == channels);
    }
    if (state->hDevB != INVALID_HANDLE_VALUE && AoGetConfig(state->hDevB, &cfgB)) {
        verifyB = (cfgB.InternalChannels == channels);
    }

    // Update channel combo from whatever device is available.
    ULONG actualCh = (state->hDevA != INVALID_HANDLE_VALUE) ? cfgA.InternalChannels
                   : (state->hDevB != INVALID_HANDLE_VALUE) ? cfgB.InternalChannels : 0;
    state->initialChannelMode = actualCh;
    for (int i = 0; i < (int)_countof(g_channelModes); i++) {
        if (g_channelModes[i] == actualCh) {
            SendMessageW(GetDlgItem(hDlg, IDC_COMBO_CHANNELS), CB_SETCURSEL, (WPARAM)i, 0);
        }
    }

    // Build per-device result message.
    WCHAR result[512];
    _snwprintf_s(result, _countof(result), _TRUNCATE,
                 L"Restart results:\n"
                 L"  Cable A: restart=%s  reopen=%s  channels=%s\n"
                 L"  Cable B: restart=%s  reopen=%s  channels=%s",
                 hasA ? (rstA ? L"OK" : L"FAILED") : L"n/a",
                 (state->hDevA != INVALID_HANDLE_VALUE) ? L"OK" : L"FAILED",
                 (state->hDevA != INVALID_HANDLE_VALUE) ? (verifyA ? L"OK" : L"MISMATCH") : L"--",
                 hasB ? (rstB ? L"OK" : L"FAILED") : L"n/a",
                 (state->hDevB != INVALID_HANDLE_VALUE) ? L"OK" : L"FAILED",
                 (state->hDevB != INVALID_HANDLE_VALUE) ? (verifyB ? L"OK" : L"MISMATCH") : L"--");

    BOOL allOk = (!hasA || (rstA && verifyA)) && (!hasB || (rstB && verifyB));
    MessageBoxW(hDlg, result, L"AO Virtual Cable",
                allOk ? MB_ICONINFORMATION : MB_ICONWARNING);
}

// ---------------------------------------------------------------------------
// Self-test: IOCTL connectivity + SET/GET roundtrip
// ---------------------------------------------------------------------------

static void
DoSelfTest(HWND hDlg, DLG_STATE* state)
{
    WCHAR report[1024] = {};
    WCHAR line[256];
    int pass = 0, fail = 0;

    // Test 1: Device accessibility
    HANDLE hTestA = AoOpenDevice(0);
    HANDLE hTestB = AoOpenDevice(1);

    if (hTestA != INVALID_HANDLE_VALUE) {
        wcscat_s(report, L"[PASS] Cable A device opened\r\n");
        pass++;
    } else {
        wcscat_s(report, L"[FAIL] Cable A device not accessible\r\n");
        fail++;
    }

    if (hTestB != INVALID_HANDLE_VALUE) {
        wcscat_s(report, L"[PASS] Cable B device opened\r\n");
        pass++;
    } else {
        wcscat_s(report, L"[FAIL] Cable B device not accessible\r\n");
        fail++;
    }

    // Test 2: GET_CONFIG on each accessible device
    HANDLE devices[] = { hTestA, hTestB };
    const WCHAR* names[] = { L"Cable A", L"Cable B" };

    for (int i = 0; i < 2; i++) {
        if (devices[i] == INVALID_HANDLE_VALUE) continue;

        AO_CONFIG config = {};
        if (AoGetConfig(devices[i], &config)) {
            _snwprintf_s(line, _countof(line), _TRUNCATE,
                         L"[PASS] %s GET_CONFIG: Rate=%lu Bits=%lu Ch=%lu\r\n",
                         names[i], config.InternalRate, config.InternalBits, config.InternalChannels);
            wcscat_s(report, line);
            pass++;
        } else {
            _snwprintf_s(line, _countof(line), _TRUNCATE,
                         L"[FAIL] %s GET_CONFIG failed\r\n", names[i]);
            wcscat_s(report, line);
            fail++;
        }
    }

    // Test 3: GET_CONFIG value sanity check (non-destructive, no SET)
    for (int i = 0; i < 2; i++) {
        if (devices[i] == INVALID_HANDLE_VALUE) continue;

        AO_CONFIG cfg = {};
        if (AoGetConfig(devices[i], &cfg)) {
            BOOL sane = (cfg.InternalRate >= 8000 && cfg.InternalRate <= 192000 &&
                         cfg.InternalBits > 0 && cfg.InternalBits <= 32 &&
                         cfg.InternalChannels >= 1 && cfg.InternalChannels <= 16);
            if (sane) {
                _snwprintf_s(line, _countof(line), _TRUNCATE,
                             L"[PASS] %s config values sane\r\n", names[i]);
                pass++;
            } else {
                _snwprintf_s(line, _countof(line), _TRUNCATE,
                             L"[FAIL] %s config values out of range\r\n", names[i]);
                fail++;
            }
            wcscat_s(report, line);
        }
    }

    // Test 4: GET_STREAM_STATUS
    for (int i = 0; i < 2; i++) {
        if (devices[i] == INVALID_HANDLE_VALUE) continue;

        AO_STREAM_STATUS st = {};
        if (AoGetStreamStatus(devices[i], &st)) {
            _snwprintf_s(line, _countof(line), _TRUNCATE,
                         L"[PASS] %s GET_STREAM_STATUS OK\r\n", names[i]);
            pass++;
        } else {
            _snwprintf_s(line, _countof(line), _TRUNCATE,
                         L"[FAIL] %s GET_STREAM_STATUS failed\r\n", names[i]);
            fail++;
        }
        wcscat_s(report, line);
    }

    // Clean up test handles (separate from dialog state handles).
    if (hTestA != INVALID_HANDLE_VALUE) CloseHandle(hTestA);
    if (hTestB != INVALID_HANDLE_VALUE) CloseHandle(hTestB);

    // Summary
    _snwprintf_s(line, _countof(line), _TRUNCATE,
                 L"\r\nResult: %d passed, %d failed", pass, fail);
    wcscat_s(report, line);

    MessageBoxW(hDlg, report, L"AO Virtual Cable - Self-Test",
                (fail == 0) ? MB_ICONINFORMATION : MB_ICONWARNING);
}

// ---------------------------------------------------------------------------
// Dialog procedure
// ---------------------------------------------------------------------------

static INT_PTR CALLBACK
SettingsDlgProc(HWND hDlg, UINT msg, WPARAM wParam, LPARAM lParam)
{
    DLG_STATE* state = (DLG_STATE*)GetWindowLongPtrW(hDlg, GWLP_USERDATA);

    switch (msg) {
    case WM_INITDIALOG:
    {
        state = (DLG_STATE*)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, sizeof(DLG_STATE));
        if (!state) {
            EndDialog(hDlg, 0);
            return TRUE;
        }
        SetWindowLongPtrW(hDlg, GWLP_USERDATA, (LONG_PTR)state);

        state->hDevA = AoOpenDevice(0);
        state->hDevB = AoOpenDevice(1);

        if (state->hDevA == INVALID_HANDLE_VALUE &&
            state->hDevB == INVALID_HANDLE_VALUE)
        {
            MessageBoxW(hDlg,
                        L"Could not open any AO Virtual Cable device.\n"
                        L"Make sure the driver is installed and running.",
                        L"AO Virtual Cable",
                        MB_ICONWARNING | MB_OK);
        }

        // Read current config from whichever device is available.
        HANDLE hDev = (state->hDevA != INVALID_HANDLE_VALUE) ? state->hDevA : state->hDevB;

        AO_CONFIG config = {};
        config.InternalRate = g_defaultRate;
        config.MaxLatencyMs = g_defaultLatencyMs;
        config.InternalChannels = g_defaultChannels;

        if (hDev != INVALID_HANDLE_VALUE) {
            AoGetConfig(hDev, &config);
        }

        // Populate sample rate combo box.
        HWND hCombo = GetDlgItem(hDlg, IDC_COMBO_RATE);
        int selIndex = 1; // default to 48000
        for (int i = 0; i < (int)_countof(g_rates); i++) {
            SendMessageW(hCombo, CB_ADDSTRING, 0, (LPARAM)g_rateLabels[i]);
            if (g_rates[i] == config.InternalRate) {
                selIndex = i;
            }
        }
        SendMessageW(hCombo, CB_SETCURSEL, (WPARAM)selIndex, 0);

        // Setup latency slider (range 5..100).
        HWND hSlider = GetDlgItem(hDlg, IDC_SLIDER_LATENCY);
        SendMessageW(hSlider, TBM_SETRANGE, TRUE, MAKELPARAM(5, 100));
        SendMessageW(hSlider, TBM_SETTICFREQ, 5, 0);
        SendMessageW(hSlider, TBM_SETPOS, TRUE, (LPARAM)config.MaxLatencyMs);

        // Update latency label.
        SetLatencyLabel(hDlg, config.MaxLatencyMs);

        // Populate channel mode combo box.
        HWND hChCombo = GetDlgItem(hDlg, IDC_COMBO_CHANNELS);
        int chSelIndex = 0;
        for (int i = 0; i < (int)_countof(g_channelModes); i++) {
            SendMessageW(hChCombo, CB_ADDSTRING, 0, (LPARAM)g_channelLabels[i]);
            if (g_channelModes[i] == config.InternalChannels) {
                chSelIndex = i;
            }
        }
        SendMessageW(hChCombo, CB_SETCURSEL, (WPARAM)chSelIndex, 0);
        state->initialChannelMode = config.InternalChannels;

        // Update runtime state, driver info.
        UpdateRuntimeState(hDlg, state);
        UpdateDriverInfo(hDlg);

        // Start polling timer (1 second).
        SetTimer(hDlg, 1, 1000, NULL);

        // Initial status update.
        UpdateStreamStatus(hDlg, state);

        return TRUE;
    }

    case WM_TIMER:
        if (wParam == 1 && state) {
            UpdateStreamStatus(hDlg, state);
            UpdateRuntimeState(hDlg, state);
        }
        return TRUE;

    case WM_HSCROLL:
    {
        HWND hSlider = GetDlgItem(hDlg, IDC_SLIDER_LATENCY);
        if ((HWND)lParam == hSlider) {
            LRESULT pos = SendMessageW(hSlider, TBM_GETPOS, 0, 0);
            SetLatencyLabel(hDlg, (ULONG)pos);
        }
        return TRUE;
    }

    case WM_COMMAND:
        switch (LOWORD(wParam)) {
        case IDAPPLY:
        {
            if (!state) break;

            // Read sample rate from combo.
            HWND hCombo = GetDlgItem(hDlg, IDC_COMBO_RATE);
            int sel = (int)SendMessageW(hCombo, CB_GETCURSEL, 0, 0);
            ULONG rate = (sel >= 0 && sel < (int)_countof(g_rates)) ? g_rates[sel] : g_defaultRate;

            // Read latency from slider.
            HWND hSlider = GetDlgItem(hDlg, IDC_SLIDER_LATENCY);
            ULONG latency = (ULONG)SendMessageW(hSlider, TBM_GETPOS, 0, 0);

            // Apply to both devices.
            BOOL okA = TRUE, okB = TRUE;

            if (state->hDevA != INVALID_HANDLE_VALUE) {
                okA = AoSetInternalRate(state->hDevA, rate) &&
                      AoSetMaxLatency(state->hDevA, latency);
            }
            if (state->hDevB != INVALID_HANDLE_VALUE) {
                okB = AoSetInternalRate(state->hDevB, rate) &&
                      AoSetMaxLatency(state->hDevB, latency);
            }

            if (!okA || !okB) {
                MessageBoxW(hDlg,
                            L"Failed to apply some settings.\n"
                            L"The driver may not support this configuration.",
                            L"AO Virtual Cable",
                            MB_ICONWARNING | MB_OK);
            } else {
                // Refresh runtime state to show updated values.
                UpdateRuntimeState(hDlg, state);
            }
            break;
        }

        case IDC_BTN_DEFAULTS:
            ApplyDefaultSelections(hDlg);
            break;

        case IDC_BTN_RESTART:
            if (state) {
                DoSetChannelAndRestart(hDlg, state);
            }
            break;

        case IDC_BTN_SELFTEST:
            if (state) {
                DoSelfTest(hDlg, state);
            }
            break;

        case IDCANCEL:
            EndDialog(hDlg, 0);
            break;
        }
        return TRUE;

    case WM_CLOSE:
        EndDialog(hDlg, 0);
        return TRUE;

    case WM_DESTROY:
        KillTimer(hDlg, 1);
        if (state) {
            if (state->hDevA != INVALID_HANDLE_VALUE) {
                CloseHandle(state->hDevA);
            }
            if (state->hDevB != INVALID_HANDLE_VALUE) {
                CloseHandle(state->hDevB);
            }
            HeapFree(GetProcessHeap(), 0, state);
            SetWindowLongPtrW(hDlg, GWLP_USERDATA, 0);
        }
        return TRUE;
    }

    return FALSE;
}
