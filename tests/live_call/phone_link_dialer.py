"""
Phone Link dial helpers.

Default dialing remains UI-driven through ``phone_link_worker.py``.
An experimental hidden ``ms-phone:calling?...`` direct-dial path can be enabled
with ``PHONE_LINK_DIAL_MODE`` without changing the default production path.
"""

from __future__ import annotations

import json
import logging
import os
import re
from pathlib import Path
import subprocess
import sys
import threading
import time
from typing import Any, Iterable
from urllib.parse import quote

from pywinauto import Desktop
from pywinauto.uia_defines import IUIA

log = logging.getLogger(__name__)

WORKER = os.path.join(os.path.dirname(__file__), "phone_link_worker.py")

# Phone Link shows two top-level windows that share the same title ("휴대폰과
# 연결" / "Phone Link"): the main app window (~940x760) and the small in-call
# overlay (~336x161). We detect the overlay by title + small rectangle, without
# descending into its UIA subtree — walking descendants on a WinUI3 Phone Link
# window was observed to trigger unintended button invokes.
PHONE_LINK_WINDOW_TITLES = ("휴대폰과 연결", "Phone Link")
CALL_OVERLAY_MAX_WIDTH = 600
CALL_OVERLAY_MAX_HEIGHT = 360
# Phone Link main app window (the big 휴대폰과 연결 window with tabs +
# call history + dialer pane) is roughly 940x760 in our setup. Anything
# above this width threshold is treated as "main window" so we can
# distinguish from the small in-call overlay.
PHONE_LINK_MAIN_MIN_WIDTH = 700

# UIA constants for targeted FindFirst probes inside the overlay window.
# FindFirst with a single AutomationId condition is served by the UIA subsystem
# and is safer than a user-space descendants walk, which was observed to touch
# the adjacent "Transfer to mobile device" button as a side effect.
_UIA_AUTOMATION_ID_PROPERTY_ID = 30011
_UIA_TREE_SCOPE_SUBTREE = 7
_UIA_INVOKE_PATTERN_ID = 10000
_MUTE_BUTTON_AUTOMATION_ID = "MuteButton"
_END_CALL_BUTTON_AUTOMATION_ID = "EndCallButton"
# Phone Link main window dialer pane error elements (visible only when
# Phone Link cannot reach the paired phone — i.e. phone-side BT off,
# pairing dropped, or HFP session dead). Identified via UIA dump
# 2026-04-14 (server/tools/phone_link_uia_dump_20260414_052629.txt).
# Detection requires BOTH the action button and one of the text labels
# to be present, so a stray cached element won't false-positive us.
_DIALER_ERROR_BUTTON_AUTOMATION_ID = "DialerPaneErrorActionButton"
_DIALER_ERROR_TITLE_AUTOMATION_ID = "DialerPaneErrorTitle"
_DIALER_ERROR_DESC_AUTOMATION_ID = "DialerPaneErrorDescription"

_LOCALAPPDATA = os.getenv("LOCALAPPDATA") or ""
PACKAGE_LOCALSTATE = (
    Path(_LOCALAPPDATA)
    / "Packages"
    / "Microsoft.YourPhone_8wekyb3d8bbwe"
    / "LocalState"
)
APP_SETTINGS = PACKAGE_LOCALSTATE / "DiagOutputDir" / "AppSettings.txt"
STARTMENU_JSON = PACKAGE_LOCALSTATE / "StartMenu" / "StartMenuCompanion.json"

DIAL_MODE_UI_ONLY = "ui_only"
DIAL_MODE_HIDDEN_URI_FIRST = "hidden_uri_first"
DIAL_MODE_HIDDEN_URI_ONLY = "hidden_uri_only"


def _contains_any(text: str, keywords: Iterable[str]) -> bool:
    value = (text or "").strip().lower()
    return any(keyword.lower() in value for keyword in keywords)


def _current_dial_mode() -> str:
    mode = os.getenv("PHONE_LINK_DIAL_MODE", DIAL_MODE_UI_ONLY).strip().lower()
    if mode not in {
        DIAL_MODE_UI_ONLY,
        DIAL_MODE_HIDDEN_URI_FIRST,
        DIAL_MODE_HIDDEN_URI_ONLY,
    }:
        return DIAL_MODE_UI_ONLY
    return mode


_DEVICE_ID_URL_PATTERN = re.compile(
    r"deviceid=([0-9a-fA-F]{8}-[0-9a-fA-F]{4}-[0-9a-fA-F]{4}-[0-9a-fA-F]{4}-[0-9a-fA-F]{12})"
)

# AppSettings.txt became a single-line JSON blob in recent Phone Link updates
# (previously key=value lines). The correct device id appears as a JSON field:
#   "DeviceId":"<uuid>"
# This pattern matches that exact form, distinguishing it from unrelated
# "BluetoothClassicDeviceId", "TrustedLEDeviceIds", etc. that also contain
# the substring "DeviceId".
_APPSETTINGS_DEVICE_ID_PATTERN = re.compile(
    r'"DeviceId"\s*:\s*"([0-9a-fA-F]{8}-[0-9a-fA-F]{4}-[0-9a-fA-F]{4}-[0-9a-fA-F]{4}-[0-9a-fA-F]{12})"'
)


def _find_device_id_in_tree(obj: Any) -> str | None:
    """Depth-first search for the first deviceid=<UUID> pattern in any string.

    StartMenuCompanion.json has a deeply nested adaptive-card-like shape with
    deviceid embedded inside URL strings at various paths, e.g.
        body[0].columns[1].selectAction.url
        body[9].items[0].columns[1].items[0].items[0].selectAction.url
        companionActions[0].url
    Instead of coding the exact paths (which change with Phone Link updates),
    we walk the entire tree and return the first UUID match we find.
    """
    if isinstance(obj, str):
        match = _DEVICE_ID_URL_PATTERN.search(obj)
        if match:
            return match.group(1)
        return None
    if isinstance(obj, dict):
        for value in obj.values():
            found = _find_device_id_in_tree(value)
            if found:
                return found
        return None
    if isinstance(obj, list):
        for item in obj:
            found = _find_device_id_in_tree(item)
            if found:
                return found
    return None


def _resolve_device_id() -> str | None:
    """Resolve the Phone Link device_id that works in ``ms-phone:calling?``
    URIs.

    Priority:
      1. ``PHONE_LINK_DEVICE_ID`` env var (explicit pin)
      2. ``StartMenuCompanion.json`` — the ``deviceid=<UUID>`` URL parameter
         embedded in the adaptive card tree. This is the id that Phone Link's
         own start-menu tiles use to invoke its internal dial handler, so it
         is guaranteed to match the active dial pipeline.
      3. ``AppSettings.txt`` — ``"DeviceId":"<UUID>"`` JSON field. This is an
         account-level identifier and may NOT match the dial-target id after
         a BT re-pair (2026-04-15 observed divergence). Kept as last-resort
         fallback.
    """
    env_value = os.getenv("PHONE_LINK_DEVICE_ID", "").strip()
    if env_value:
        return env_value

    try:
        if STARTMENU_JSON.exists():
            data = json.loads(
                STARTMENU_JSON.read_text(encoding="utf-8", errors="ignore")
            )
            found = _find_device_id_in_tree(data)
            if found:
                return found
    except Exception as exc:
        log.debug("StartMenuCompanion device id parse failed: %s", exc)

    try:
        if APP_SETTINGS.exists():
            text = APP_SETTINGS.read_text(encoding="utf-8", errors="ignore")
            match = _APPSETTINGS_DEVICE_ID_PATTERN.search(text)
            if match:
                return match.group(1)
    except Exception as exc:
        log.debug("AppSettings device id parse failed: %s", exc)

    return None


def _build_hidden_dial_uri(phone_number: str) -> str | None:
    device_id = _resolve_device_id()
    if not device_id:
        return None
    return (
        "ms-phone:calling?"
        f"deviceid={quote(device_id)}&callingdirectdial=true&targetnumber={quote(phone_number)}"
    )


def _launch_hidden_dial(phone_number: str) -> bool:
    uri = _build_hidden_dial_uri(phone_number)
    if not uri:
        log.warning("Phone Link hidden URI skipped: device id not resolved")
        return False

    try:
        log.info("Phone Link hidden URI dial request: %s", uri)
        os.startfile(uri)
        time.sleep(float(os.getenv("PHONE_LINK_HIDDEN_URI_SETTLE_SECONDS", "1.0")))
        log.info("Phone Link hidden URI launch ok")
        return True
    except Exception as exc:
        log.error("Phone Link hidden URI launch failed: %s", exc)
        return False


def _dial_via_worker(phone_number: str) -> bool:
    log.info("Phone Link dial request: %s", phone_number)
    result = subprocess.run(
        [sys.executable, WORKER, phone_number],
        capture_output=True,
        text=True,
        timeout=15,
    )
    if result.returncode == 0:
        log.info("Phone Link dial success: %s", result.stdout.strip())
        return True

    error_text = result.stderr.strip() or result.stdout.strip()
    log.error("Phone Link dial failed: %s", error_text)
    return False


def dial(phone_number: str) -> bool:
    """Request an outbound Phone Link call.

    The return value means "the dial trigger was accepted", not "the remote party
    answered". Authoritative answered/idle still comes from Android app state or
    higher-level server logic.
    """
    mode = _current_dial_mode()

    try:
        if mode == DIAL_MODE_HIDDEN_URI_ONLY:
            return _launch_hidden_dial(phone_number)

        if mode == DIAL_MODE_HIDDEN_URI_FIRST:
            launched = _launch_hidden_dial(phone_number)
            if launched:
                log.info("Phone Link hidden URI launched; UI fallback kept available")
                time.sleep(float(os.getenv("PHONE_LINK_HIDDEN_URI_FALLBACK_DELAY_SECONDS", "1.5")))

        return _dial_via_worker(phone_number)
    except subprocess.TimeoutExpired:
        log.error("Phone Link dial worker timed out")
        return False
    except Exception as exc:
        log.error("Phone Link dial error: %s", exc)
        return False


def _iter_call_overlays():
    """Yield top-level Phone Link windows that look like the in-call overlay.

    Shape filter: title matches PHONE_LINK_WINDOW_TITLES AND rectangle fits
    within CALL_OVERLAY_MAX_WIDTH x CALL_OVERLAY_MAX_HEIGHT. This excludes
    the main app window (~940x760) but matches the small overlay (~336x161).
    """
    try:
        desktop = Desktop(backend="uia")
        windows = desktop.windows()
    except Exception as exc:
        log.debug("Desktop.windows() failed: %s", exc)
        return
    for window in windows:
        try:
            title = (window.window_text() or "").strip()
        except Exception:
            continue
        if not _contains_any(title, PHONE_LINK_WINDOW_TITLES):
            continue
        try:
            rect = window.rectangle()
        except Exception:
            continue
        width = abs(rect.right - rect.left)
        height = abs(rect.bottom - rect.top)
        if width <= 0 or height <= 0:
            continue
        if width <= CALL_OVERLAY_MAX_WIDTH and height <= CALL_OVERLAY_MAX_HEIGHT:
            yield window


def _iter_main_windows():
    """Yield top-level Phone Link MAIN windows (the big tabbed app window).

    Same title filter as _iter_call_overlays but the size predicate is
    inverted: we want windows wider than PHONE_LINK_MAIN_MIN_WIDTH so
    we don't accidentally pick up the small in-call overlay. No
    descendants walk — caller does targeted FindFirst by AutomationId.
    """
    try:
        desktop = Desktop(backend="uia")
        windows = desktop.windows()
    except Exception as exc:
        log.debug("Desktop.windows() failed: %s", exc)
        return
    for window in windows:
        try:
            title = (window.window_text() or "").strip()
        except Exception:
            continue
        if not _contains_any(title, PHONE_LINK_WINDOW_TITLES):
            continue
        try:
            rect = window.rectangle()
        except Exception:
            continue
        width = abs(rect.right - rect.left)
        if width >= PHONE_LINK_MAIN_MIN_WIDTH:
            yield window


def phone_link_main_dialer_disconnected() -> bool:
    """Return True if Phone Link's main window dialer pane is showing
    its 'cannot connect to mobile device' error state.

    This is the only reliable signal we have for 'phone-side BT off'
    while the PC-side PnP cache still claims the BT Hands-Free device
    is healthy. When Phone Link cannot reach the paired phone the
    dialer pane swaps the keypad for an error block containing:

      AutomationId='DialerPaneErrorTitle'        TextBlock
      AutomationId='DialerPaneErrorDescription'  TextBlock
      AutomationId='DialerPaneErrorActionButton' Button '다시 시도'

    Detection requires BOTH the action button AND one of the text
    labels to be present, so a stray cached element won't trip us.
    Identified via UIA dump on 2026-04-14
    (server/tools/phone_link_uia_dump_20260414_052629.txt).

    Implementation:
      - safe FindFirst-by-AutomationId pattern, NO descendants walk
        (same approach as call_answered/call_hangup, deliberately
        avoiding any risk of touching adjacent buttons).
      - Read-only: no Invoke / no SetFocus / no click.
      - If Phone Link main window is not visible at all, returns
        False — the higher-level preflight check_phone_link_process
        already gates on the process being up.
    """
    try:
        iuia = IUIA().iuia
    except Exception as exc:
        log.debug("phone_link_main_dialer_disconnected: IUIA init failed: %s", exc)
        return False

    try:
        cond_btn = iuia.CreatePropertyCondition(
            _UIA_AUTOMATION_ID_PROPERTY_ID, _DIALER_ERROR_BUTTON_AUTOMATION_ID
        )
        cond_title = iuia.CreatePropertyCondition(
            _UIA_AUTOMATION_ID_PROPERTY_ID, _DIALER_ERROR_TITLE_AUTOMATION_ID
        )
        cond_desc = iuia.CreatePropertyCondition(
            _UIA_AUTOMATION_ID_PROPERTY_ID, _DIALER_ERROR_DESC_AUTOMATION_ID
        )
    except Exception as exc:
        log.debug("phone_link_main_dialer_disconnected: CreatePropertyCondition failed: %s", exc)
        return False

    def _findfirst_present(elem, cond):
        """Wrapper that handles comtypes NULL-pointer return.

        IUIAutomationElement.FindFirst returns a wrapped pointer object
        even when no element matches — the wrapper's underlying pointer
        is NULL but `is None` does NOT catch this. We have to bool-test
        the pointer (comtypes COM wrapper __bool__ returns False for
        NULL) AND defensively try to read a CurrentXxx property; if
        that raises NULL COM pointer access, treat as not found.
        """
        try:
            result = elem.FindFirst(_UIA_TREE_SCOPE_SUBTREE, cond)
        except Exception as exc:
            log.debug("FindFirst raised: %s", exc)
            return None
        if result is None:
            return None
        if not bool(result):
            return None
        try:
            # touch a CurrentXxx to confirm the pointer is real
            _ = result.CurrentName
        except Exception:
            return None
        return result

    try:
        for window in _iter_main_windows():
            element = window.element_info.element
            btn = _findfirst_present(element, cond_btn)
            if btn is None:
                continue
            # Cross-check: at least one text label must also be present
            # in the same window. If only a cached/null shell is there
            # without the label, treat as not-disconnected.
            title = _findfirst_present(element, cond_title)
            desc = _findfirst_present(element, cond_desc)
            if title is not None or desc is not None:
                return True
    except Exception as exc:
        log.debug("phone_link_main_dialer_disconnected: outer iteration failed: %s", exc)
    return False


def call_popup_visible() -> bool:
    """Return True when the Phone Link in-call overlay window is present.

    True for both dialing (ringing) and answered states. Use call_answered()
    when you need to distinguish an actively connected call from a ringing
    outbound dial. Top-level window title + rect only, never walks descendants.
    """
    try:
        for _ in _iter_call_overlays():
            return True
    except Exception as exc:
        log.debug("call_popup_visible failed: %s", exc)
    return False


def call_answered() -> bool:
    """Return True when the call is actively connected (remote party answered).

    Detection: overlay window exists AND its MuteButton is enabled. Phone Link
    keeps the MuteButton disabled (grey) while the outbound call is ringing,
    and switches it to enabled the moment the remote side picks up (verified
    2026-04-13: transition observed within ~0.5s of real answer).

    Implementation uses a single UIA FindFirst by AutomationId='MuteButton'
    on the overlay's subtree — this is one UIA query served inside the UIA
    subsystem, not a user-space descendants walk, so it does not touch the
    adjacent 'Transfer to mobile device' button.
    """
    try:
        iuia = IUIA().iuia
    except Exception as exc:
        log.debug("IUIA init failed: %s", exc)
        return False

    try:
        condition = iuia.CreatePropertyCondition(
            _UIA_AUTOMATION_ID_PROPERTY_ID, _MUTE_BUTTON_AUTOMATION_ID
        )
    except Exception as exc:
        log.debug("CreatePropertyCondition failed: %s", exc)
        return False

    try:
        for overlay in _iter_call_overlays():
            try:
                mute = overlay.element_info.element.FindFirst(
                    _UIA_TREE_SCOPE_SUBTREE, condition
                )
            except Exception as exc:
                log.debug("MuteButton FindFirst failed: %s", exc)
                continue
            if mute is None:
                continue
            try:
                if bool(mute.CurrentIsEnabled):
                    return True
            except Exception as exc:
                log.debug("MuteButton IsEnabled query failed: %s", exc)
                continue
    except Exception as exc:
        log.debug("call_answered failed: %s", exc)
    return False


def call_hangup() -> bool:
    """Click the Phone Link overlay's EndCallButton if present.

    Used by dialing terminal paths (dial_timeout, overlay_vanished,
    stale_overlay cleanup) so the Phone Link session actually tears down
    when the server has already decided to move on. Without this, Phone
    Link's dial/answering UI stays up indefinitely after a no_answer
    even though the DB has already written the terminal state.

    Returns:
        True if an EndCallButton was found and successfully invoked.
        False if no overlay / no button / Invoke failed. Caller should
        treat False as no-op, not error.

    Implementation notes:
    - Uses the same safe targeted UIA FindFirst(AutomationId) approach
      as call_answered(). No descendants walk, so the adjacent
      "모바일 장치로 통화 보내기" button is never touched.
    - Pattern acquisition via pywinauto ButtonWrapper (which internally
      handles the comtypes IUIAutomationInvokePattern cast correctly
      across comtypes versions). The earlier direct GetCurrentPatternAs
      approach returned a raw int on this comtypes build, triggering
      "'int' object has no attribute 'Invoke'" at Invoke() time.
    - Two-stage fallback: ButtonWrapper.invoke() first (proper UIA
      Invoke pattern), click_input() as mouse-click fallback if the
      invoke pattern path fails.
    - Idempotent: calling when no call is active returns False with no
      side effects. Calling twice on the same active call is safe
      because the first Invoke terminates the call and the second
      FindFirst returns nothing.
    """
    try:
        iuia = IUIA().iuia
    except Exception as exc:
        log.debug("call_hangup: IUIA init failed: %s", exc)
        return False

    try:
        condition = iuia.CreatePropertyCondition(
            _UIA_AUTOMATION_ID_PROPERTY_ID, _END_CALL_BUTTON_AUTOMATION_ID
        )
    except Exception as exc:
        log.debug("call_hangup: CreatePropertyCondition failed: %s", exc)
        return False

    try:
        from pywinauto.controls.uia_controls import ButtonWrapper
        from pywinauto.uia_element_info import UIAElementInfo
    except Exception as exc:
        log.debug("call_hangup: pywinauto button wrapper import failed: %s", exc)
        return False

    try:
        for overlay in _iter_call_overlays():
            try:
                end_btn = overlay.element_info.element.FindFirst(
                    _UIA_TREE_SCOPE_SUBTREE, condition
                )
            except Exception as exc:
                log.debug("call_hangup: EndCallButton FindFirst failed: %s", exc)
                continue
            if end_btn is None:
                continue

            try:
                btn_info = UIAElementInfo(end_btn)
                btn_wrapper = ButtonWrapper(btn_info)
            except Exception as exc:
                log.debug("call_hangup: ButtonWrapper construct failed: %s", exc)
                continue

            try:
                btn_wrapper.invoke()
                log.info("call_hangup: EndCallButton invoked via pywinauto ButtonWrapper")
                return True
            except Exception as invoke_exc:
                log.debug(
                    "call_hangup: ButtonWrapper.invoke failed (%s); trying click_input",
                    invoke_exc,
                )

            try:
                btn_wrapper.click_input()
                log.info("call_hangup: EndCallButton clicked via click_input fallback")
                return True
            except Exception as click_exc:
                log.warning(
                    "call_hangup: click_input fallback also failed: %s",
                    click_exc,
                )
                continue
    except Exception as exc:
        log.debug("call_hangup: outer iteration failed: %s", exc)

    return False


def dial_async(phone_number: str) -> None:
    """Fire-and-forget helper kept for compatibility."""
    threading.Thread(target=dial, args=(phone_number,), daemon=True).start()
