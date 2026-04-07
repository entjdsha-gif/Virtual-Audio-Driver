/*++
Module Name:
    main.cpp
Abstract:
    Entry point and UI logic for AO Virtual Cable Control Panel tray app.
    - System tray icon with right-click menu
    - Settings dialog for sample rate, latency, and stream monitoring
--*/

#include <windows.h>
#include <commctrl.h>
#include <shellapi.h>
#include <stdio.h>

#include "resource.h"
#include "device.h"

#pragma comment(lib, "comctl32.lib")
#pragma comment(lib, "shell32.lib")

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
UpdateStreamStatus(HWND hDlg, DLG_STATE* state)
{
    AO_STREAM_STATUS status = {};
    WCHAR buf[128];

    // Try device A first, then overlay with device B if available.
    HANDLE hDev = (state->hDevA != INVALID_HANDLE_VALUE) ? state->hDevA : state->hDevB;

    if (hDev != INVALID_HANDLE_VALUE && AoGetStreamStatus(hDev, &status)) {
        FormatEndpointStatus(&status.CableA_Speaker, buf, _countof(buf));
        SetDlgItemTextW(hDlg, IDC_STATUS_A_SPK, buf);

        FormatEndpointStatus(&status.CableA_Mic, buf, _countof(buf));
        SetDlgItemTextW(hDlg, IDC_STATUS_A_MIC, buf);

        FormatEndpointStatus(&status.CableB_Speaker, buf, _countof(buf));
        SetDlgItemTextW(hDlg, IDC_STATUS_B_SPK, buf);

        FormatEndpointStatus(&status.CableB_Mic, buf, _countof(buf));
        SetDlgItemTextW(hDlg, IDC_STATUS_B_MIC, buf);
    }
}

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
        config.InternalRate = 48000;
        config.MaxLatencyMs = 20;

        if (hDev != INVALID_HANDLE_VALUE) {
            AoGetConfig(hDev, &config);
        }

        // Populate sample rate combo box.
        HWND hCombo = GetDlgItem(hDlg, IDC_COMBO_RATE);
        int selIndex = 1; // default to 48000
        for (int i = 0; i < _countof(g_rates); i++) {
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
        WCHAR latBuf[32];
        _snwprintf_s(latBuf, _countof(latBuf), _TRUNCATE, L"%lu ms", config.MaxLatencyMs);
        SetDlgItemTextW(hDlg, IDC_STATIC_LATENCY, latBuf);

        // Start polling timer (1 second).
        SetTimer(hDlg, 1, 1000, NULL);

        // Initial status update.
        UpdateStreamStatus(hDlg, state);

        return TRUE;
    }

    case WM_TIMER:
        if (wParam == 1 && state) {
            UpdateStreamStatus(hDlg, state);
        }
        return TRUE;

    case WM_HSCROLL:
    {
        HWND hSlider = GetDlgItem(hDlg, IDC_SLIDER_LATENCY);
        if ((HWND)lParam == hSlider) {
            LRESULT pos = SendMessageW(hSlider, TBM_GETPOS, 0, 0);
            WCHAR latBuf[32];
            _snwprintf_s(latBuf, _countof(latBuf), _TRUNCATE, L"%ld ms", (long)pos);
            SetDlgItemTextW(hDlg, IDC_STATIC_LATENCY, latBuf);
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
            ULONG rate = (sel >= 0 && sel < (int)_countof(g_rates)) ? g_rates[sel] : 48000;

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
            }
            break;
        }
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
