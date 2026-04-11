"""
Audio device routing helper for Phone Link call startup/shutdown.

Primary strategy:
- Set system defaults to the desired A/B cable endpoints.
- If SoundVolumeView is available, also force per-app render/capture devices
  for Phone Link related processes and keep reapplying them in the background.

This avoids slow UI automation through the Windows Settings Volume Mixer.
"""

from __future__ import annotations

import json
import logging
import os
import re
import subprocess
import threading
import time
from pathlib import Path
from typing import Optional

log = logging.getLogger(__name__)

_vmr = None
_original_playback: Optional[dict] = None
_original_recording: Optional[dict] = None
_svv_thread: Optional[threading.Thread] = None
_svv_stop_event: Optional[threading.Event] = None

DEFAULT_PHONE_LINK_PROCESSES = (
    "PhoneExperienceHost.exe",
    "CrossDeviceService.exe",
    "CrossDeviceResume.exe",
    "YourPhoneAppProxy.exe",
)

SESSION_HINT_TERMS = (
    "Hands-Free",
    "HF Audio",
    "Phone Link",
    "Your Phone",
    "PhoneExperienceHost",
    "CrossDevice",
    "Z Fold",
    "Galaxy",
)


def _extract_device_label(friendly_name: str) -> str:
    """Extract the part inside parentheses, e.g. '스피커(Realtek HD Audio)' -> 'Realtek HD Audio'."""
    match = re.search(r'\((.+)\)', friendly_name)
    if match:
        return match.group(1)
    return friendly_name


def _routing_mode() -> str:
    mode = os.getenv("AUDIO_ROUTING_MODE", "full").strip().lower()
    if mode not in {"full", "defaults", "none"}:
        return "full"
    return mode


def _get_pycaw_enumerator():
    """Get MMDevice enumerator via comtypes."""
    import comtypes
    from comtypes import GUID
    from pycaw.pycaw import IMMDeviceEnumerator
    return comtypes.CoCreateInstance(
        GUID('{BCDE0395-E52F-467C-8E3D-C4579291692E}'),
        clsctx=comtypes.CLSCTX_ALL,
        interface=IMMDeviceEnumerator,
    )


def _get_policy_config():
    """Get IPolicyConfig for setting default devices (Win7~Win11)."""
    import comtypes
    from comtypes import GUID, HRESULT, COMMETHOD
    from ctypes import c_int, c_short
    from ctypes.wintypes import LPCWSTR

    class IPolicyConfigWin7(comtypes.IUnknown):
        _iid_ = GUID('{F8679F50-850A-41CF-9C72-430F290290C8}')
        _methods_ = [
            # 8 unused vtable stubs (slots 3-10, required for correct offset)
            COMMETHOD([], HRESULT, 'Unused1'),
            COMMETHOD([], HRESULT, 'Unused2'),
            COMMETHOD([], HRESULT, 'Unused3'),
            COMMETHOD([], HRESULT, 'Unused4'),
            COMMETHOD([], HRESULT, 'Unused5'),
            COMMETHOD([], HRESULT, 'Unused6'),
            COMMETHOD([], HRESULT, 'Unused7'),
            COMMETHOD([], HRESULT, 'Unused8'),
            # GetPropertyValue (slot 11)
            COMMETHOD([], HRESULT, 'GetPropertyValue',
                (['in'], LPCWSTR, 'wszDeviceId'),
                (['in'], comtypes.c_void_p, 'pkey'),
                (['in', 'out'], comtypes.c_void_p, 'pv')),
            # SetPropertyValue (slot 12)
            COMMETHOD([], HRESULT, 'SetPropertyValue',
                (['in'], LPCWSTR, 'wszDeviceId'),
                (['in'], comtypes.c_void_p, 'pkey'),
                (['in'], comtypes.c_void_p, 'pv')),
            # SetDefaultEndpoint (slot 13) — THE ONE WE NEED
            COMMETHOD([], HRESULT, 'SetDefaultEndpoint',
                (['in'], LPCWSTR, 'wszDeviceId'),
                (['in'], c_int, 'eRole')),
            # SetEndpointVisibility (slot 14)
            COMMETHOD([], HRESULT, 'SetEndpointVisibility',
                (['in'], LPCWSTR, 'wszDeviceId'),
                (['in'], c_short, 'isVisible')),
        ]

    return comtypes.CoCreateInstance(
        GUID('{870AF99C-171D-4F9E-AF0D-E63DF40C2BC9}'),
        clsctx=comtypes.CLSCTX_ALL,
        interface=IPolicyConfigWin7,
    )


DEVICE_NAME_BLOCKLIST = {"sidetone"}


def _get_default_device(device_type: str) -> Optional[dict]:
    """Get current default device info. device_type: 'Playback' or 'Recording'."""
    try:
        enumerator = _get_pycaw_enumerator()
        flow = 0 if device_type == "Playback" else 1
        dev = enumerator.GetDefaultAudioEndpoint(flow, 0)
        dev_id = dev.GetId()
        from pycaw.pycaw import AudioUtilities
        name = "unknown"
        for d in AudioUtilities.GetAllDevices():
            if d.id == dev_id:  # exact match, not substring
                name = d.FriendlyName
                break
        return {"Name": name, "ID": dev_id}
    except Exception as exc:
        log.warning("Could not get default %s: %s", device_type, exc)
        return None


def _set_default_by_id(device_id: str) -> bool:
    """Set default device for all roles (console + multimedia + communications)."""
    try:
        policy = _get_policy_config()
        for role in (0, 1, 2):  # eConsole, eMultimedia, eCommunications
            policy.SetDefaultEndpoint(device_id, role)
        log.info("Default device set: %s", device_id[:50])
        return True
    except Exception as exc:
        log.error("SetDefaultEndpoint failed: %s", exc)
        return False


def _find_device_id(name_pattern: str, device_type: str) -> Optional[str]:
    """Find device ID by name pattern. device_type: 'Playback' or 'Recording'."""
    try:
        from pycaw.pycaw import AudioUtilities
        for d in AudioUtilities.GetAllDevices():
            fname = d.FriendlyName.lower()
            if any(bl in fname for bl in DEVICE_NAME_BLOCKLIST):
                continue
            if name_pattern.lower() in fname:
                is_render = d.id.startswith("{0.0.0")
                is_capture = d.id.startswith("{0.0.1")
                if device_type == "Playback" and is_render:
                    return d.id
                if device_type == "Recording" and is_capture:
                    return d.id
    except Exception as exc:
        log.warning("Device search failed: %s", exc)
    return None


def _should_use_voicemeeter(output_device_name: str, input_device_name: str) -> bool:
    force = os.getenv("USE_VOICEMEETER", "").strip().lower()
    if force in ("1", "true", "yes", "on"):
        return True
    joined = f"{output_device_name} {input_device_name}".lower()
    return "voicemeeter" in joined


def _ensure_voicemeeter() -> bool:
    global _vmr
    try:
        import voicemeeterlib
    except Exception as exc:
        log.warning("Voicemeeter SDK unavailable: %s", exc)
        return False

    try:
        _vmr = voicemeeterlib.api("banana")
        _vmr.login()
        time.sleep(0.5)
        _vmr.strip[3].B1 = True
        _vmr.strip[3].A1 = True
        _vmr.strip[4].B2 = True
        log.info("Voicemeeter routing enabled")
        return True
    except Exception as exc:
        log.error("Voicemeeter setup failed: %s", exc)
        _vmr = None
        return False


def _close_voicemeeter() -> None:
    global _vmr
    if _vmr is not None:
        try:
            _vmr.logout()
            log.info("Voicemeeter session closed")
        except Exception as exc:
            log.warning("Voicemeeter logout failed: %s", exc)
        _vmr = None


def _phone_link_processes() -> tuple[str, ...]:
    configured = os.getenv("PHONE_LINK_PROCESS_NAMES", "").strip()
    if not configured:
        return DEFAULT_PHONE_LINK_PROCESSES
    return tuple(p.strip() for p in configured.split(",") if p.strip())


def _find_windows_audio_helper() -> Optional[Path]:
    _here = Path(__file__).resolve().parent
    candidates = [
        os.getenv("WINDOWS_AUDIO_HELPER_DLL", "").strip(),
        str(_here / "tools" / "WindowsAudioHelper" / "WindowsAudioHelper.dll"),
    ]
    for candidate in candidates:
        if not candidate:
            continue
        path = Path(candidate)
        if path.exists():
            return path
    return None


def _query_helper_sessions() -> list[dict]:
    helper = _find_windows_audio_helper()
    if helper is None:
        return []
    try:
        result = subprocess.run(
            ["dotnet", str(helper), "sessions", "--json"],
            capture_output=True,
            encoding="utf-8",
            errors="replace",
            timeout=15,
        )
    except Exception as exc:
        log.warning("WindowsAudioHelper sessions failed: %s", exc)
        return []

    if result.returncode != 0:
        stderr = (result.stderr or result.stdout or "").strip()
        if stderr:
            log.warning("WindowsAudioHelper stderr: %s", stderr)
        return []

    try:
        data = json.loads(result.stdout or "[]")
    except json.JSONDecodeError:
        return []
    return data if isinstance(data, list) else []


def _discover_phone_link_targets() -> tuple[str, ...]:
    targets: list[str] = list(_phone_link_processes())
    sessions = _query_helper_sessions()
    for session in sessions:
        process_name = str(session.get("ProcessName") or "").strip()
        process_id = session.get("ProcessId")
        display_name = str(session.get("DisplayName") or "").strip()

        haystack = " ".join([process_name, display_name]).lower()
        if not any(term.lower() in haystack for term in SESSION_HINT_TERMS):
            continue

        if process_name:
            if process_name.lower() not in {t.lower() for t in targets if not t.isdigit()}:
                targets.append(process_name)
        if process_id:
            targets.append(str(process_id))

    # Preserve order while removing duplicates.
    seen: set[str] = set()
    ordered: list[str] = []
    for target in targets:
        key = target.lower()
        if key in seen:
            continue
        seen.add(key)
        ordered.append(target)
    return tuple(ordered)


def _find_svv_exe() -> Optional[Path]:
    _here = Path(__file__).resolve().parent
    configured = os.getenv("SOUNDVOLUMEVIEW_EXE", "").strip()
    candidates = [
        configured,
        # svcl.exe (CLI-only, no GUI conflict) — preferred, shipped in this repo
        str(_here / "tools" / "svcl" / "svcl.exe"),
    ]
    for candidate in candidates:
        if not candidate:
            continue
        path = Path(candidate)
        if path.exists():
            return path
    return None


def _svv(args: list[str]) -> bool:
    exe = _find_svv_exe()
    if exe is None:
        log.warning("SoundVolumeView.exe not found; skipping app routing")
        return False
    try:
        result = subprocess.run(
            [str(exe), *args],
            capture_output=True,
            encoding="utf-8",
            errors="replace",
            timeout=15,
        )
    except Exception as exc:
        log.error("SoundVolumeView failed: %s", exc)
        return False

    if result.returncode != 0:
        stderr = (result.stderr or result.stdout or "").strip()
        if stderr:
            log.warning("SoundVolumeView stderr: %s", stderr)
        return False
    return True


_SYSTEM_PROCESSES = {
    "svchost.exe", "audiodg.exe", "csrss.exe", "dwm.exe",
    "sihost.exe", "taskhostw.exe", "runtimebroker.exe",
    "systemsettings.exe", "searchhost.exe", "explorer.exe",
}

_routed_pids: set[str] = set()
_pre_switch_sessions: set[str] = set()


def _capture_existing_sessions() -> None:
    """Capture process names of currently active audio sessions before default switch."""
    global _pre_switch_sessions
    _pre_switch_sessions.clear()
    try:
        from pycaw.pycaw import AudioUtilities
        sessions = AudioUtilities.GetAllSessions()
        for session in sessions:
            if session.Process:
                _pre_switch_sessions.add(session.Process.name())
    except Exception:
        pass
    log.info("Pre-switch sessions captured: %s", _pre_switch_sessions)


def _get_non_phonelink_sessions() -> list[str]:
    """Get process NAMES of audio sessions that need rerouting."""
    try:
        from pycaw.pycaw import AudioUtilities
    except ImportError:
        log.warning("pycaw not available for session enumeration")
        return []

    exclude_names = {p.lower() for p in _phone_link_processes()} | _SYSTEM_PROCESSES
    names: list[str] = []
    try:
        sessions = AudioUtilities.GetAllSessions()
        for session in sessions:
            if session.Process:
                proc_name = session.Process.name()
                if proc_name.lower() not in exclude_names and proc_name not in _routed_pids:
                    names.append(proc_name)
    except Exception as exc:
        log.warning("Session enumeration failed: %s", exc)
    return names


def _apply_reverse_routing(original_playback_id: str, original_recording_id: str) -> bool:
    """Route non-Phone-Link apps to ORIGINAL devices using Device ID. Skip already-routed."""
    names = _get_non_phonelink_sessions()
    # Also include pre-switch sessions that haven't been routed yet
    for name in _pre_switch_sessions:
        if name not in _routed_pids and name.lower() not in {p.lower() for p in _phone_link_processes()} | _SYSTEM_PROCESSES:
            if name not in names:
                names.append(name)
    any_ok = False
    for name in names:
        ok = _svv(["/SetAppDefault", original_playback_id, "all", name])
        if ok:
            _routed_pids.add(name)
            any_ok = True
            log.info("Routed %s -> original playback", name)
    return any_ok


def _restore_app_routing() -> None:
    """Reset all per-app routing to system defaults."""
    for name in list(_routed_pids):
        _svv(["/SetAppDefault", "DefaultRenderDevice", "all", name])
    _routed_pids.clear()
    _pre_switch_sessions.clear()
    log.info("All per-app routing restored to system defaults")


def _start_app_routing_watch(original_playback_name: str, original_recording_name: str) -> None:
    global _svv_thread, _svv_stop_event

    _stop_app_routing_watch()
    if _find_svv_exe() is None:
        return

    interval = float(os.getenv("PHONE_LINK_ROUTING_REAPPLY_SECONDS", "1.0"))
    _svv_stop_event = threading.Event()
    stop_event = _svv_stop_event

    def _runner() -> None:
        while not stop_event.is_set():
            try:
                _apply_reverse_routing(original_playback_name, original_recording_name)
            except Exception as exc:
                log.warning("Reverse routing reapply failed: %s", exc)
            stop_event.wait(interval)

    _svv_thread = threading.Thread(
        target=_runner,
        daemon=True,
        name="reverse-routing-watch",
    )
    _svv_thread.start()
    log.info("Reverse routing watch started (non-Phone-Link -> %s / %s)",
             original_playback_name, original_recording_name)


def _stop_app_routing_watch() -> None:
    global _svv_thread, _svv_stop_event

    if _svv_stop_event is not None:
        _svv_stop_event.set()
    if _svv_thread is not None and _svv_thread.is_alive():
        _svv_thread.join(timeout=1.0)
    _svv_thread = None
    _svv_stop_event = None


def set_call_routing(
    output_device_name: str = "AO Cable A",
    input_device_name: str = "AO Cable B",
) -> bool:
    global _original_playback, _original_recording

    mode = _routing_mode()
    if mode == "none":
        log.info("Audio routing skipped (AUDIO_ROUTING_MODE=none)")
        return True

    if _should_use_voicemeeter(output_device_name, input_device_name):
        _ensure_voicemeeter()
    else:
        _close_voicemeeter()

    # Guard: don't overwrite originals if already saved (double-call protection)
    if _original_playback is None:
        _original_playback = _get_default_device("Playback")
    if _original_recording is None:
        _original_recording = _get_default_device("Recording")
    log.info("Original playback: %s", _original_playback)
    log.info("Original recording: %s", _original_recording)

    # Capture existing sessions BEFORE switching (for reverse routing)
    _capture_existing_sessions()

    ok = True

    playback_id = _find_device_id(output_device_name, "Playback")
    if playback_id is None:
        log.error("Playback device not found: %s", output_device_name)
        ok = False
    elif not _set_default_by_id(playback_id):
        ok = False
    else:
        log.info("Default playback -> %s", output_device_name)

    recording_id = _find_device_id(input_device_name, "Recording")
    if recording_id is None:
        log.error("Recording device not found: %s", input_device_name)
        # Rollback playback if recording fails
        if _original_playback and "ID" in _original_playback:
            _set_default_by_id(_original_playback["ID"])
            log.info("Rolled back playback due to recording failure")
        ok = False
    elif not _set_default_by_id(recording_id):
        ok = False
    else:
        log.info("Default recording -> %s", input_device_name)

    # Pre-set Phone Link capture to Cable B BEFORE dial
    # This ensures Phone Link opens its capture session on Cable B
    if recording_id:
        for proc in _phone_link_processes():
            _svv(["/SetAppDefault", recording_id, "all", proc])
        log.info("Phone Link capture pre-set -> %s", input_device_name)

    if mode == "full" and _original_playback and _original_recording:
        orig_play_id = _original_playback.get("ID", "")
        orig_rec_id = _original_recording.get("ID", "")
        if orig_play_id:
            time.sleep(0.5)  # Brief wait for session migration
            _apply_reverse_routing(orig_play_id, orig_rec_id)
            _start_app_routing_watch(orig_play_id, orig_rec_id)
        else:
            _stop_app_routing_watch()
    else:
        _stop_app_routing_watch()

    return ok


def restore_routing() -> None:
    global _original_playback, _original_recording

    mode = _routing_mode()
    if mode == "none":
        return

    _stop_app_routing_watch()
    if mode == "full":
        _restore_app_routing()

    if _original_playback and "ID" in _original_playback:
        if _set_default_by_id(_original_playback["ID"]):
            log.info("Playback restored: %s", _original_playback.get("Name"))
    _original_playback = None

    if _original_recording and "ID" in _original_recording:
        if _set_default_by_id(_original_recording["ID"]):
            log.info("Recording restored: %s", _original_recording.get("Name"))
    _original_recording = None

    _close_voicemeeter()
