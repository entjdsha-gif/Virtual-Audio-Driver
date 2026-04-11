"""
Phone Link dial worker.

This worker should only drive the Phone Link UI far enough to trigger dialing.
On slower PCs the "PC에서 통화" popup can appear well after the call button is
clicked, so connection confirmation must be handled later by the server/app
state machine instead of hard-failing inside this worker.
"""

from __future__ import annotations

import subprocess
import sys
import time
from pathlib import Path
from typing import Iterable, Optional

from pywinauto import Desktop, mouse
from pywinauto.controls.uiawrapper import UIAWrapper
from pywinauto.keyboard import send_keys


WINDOW_KEYWORDS = ("휴대폰과 연결", "Phone Link")
CALL_BUTTON_KEYWORDS = ("통화", "전화", "call", "dial", "발신")
UNLOCK_PROMPT_KEYWORDS = ("장치에서 잠금 해제", "unlock your device")
CALLS_TAB_AUTO_ID = "CallingNodeAutomationId"
DIAL_VIEW_AUTO_IDS = ("DialPaneGrid", "DialPane", "Keypad", "ButtonCall")
MAX_WINDOW_WAIT_SEC = 8.0
SCAN_INTERVAL_SEC = 0.4
WINDOW_CALL_BUTTON_Y_RATIO = 0.94
WINDOW_CALL_BUTTON_X_RATIOS = (0.97, 0.93, 0.90, 0.86)
DEBUG_LOG = Path(__file__).with_name("runtime_logs").joinpath("phone_link_worker_debug.log")


def _log(message: str) -> None:
    timestamp = time.strftime("%Y-%m-%d %H:%M:%S")
    line = f"{timestamp} [phone_link_worker] {message}"
    print(line, flush=True)
    try:
        DEBUG_LOG.parent.mkdir(parents=True, exist_ok=True)
        with DEBUG_LOG.open("a", encoding="utf-8") as fp:
            fp.write(line + "\n")
    except Exception:
        pass


def _contains_any(text: str, keywords: tuple[str, ...]) -> bool:
    value = (text or "").strip().lower()
    return any(keyword.lower() in value for keyword in keywords)


def _window_matches(text: str) -> bool:
    return _contains_any(text, WINDOW_KEYWORDS)


def _is_call_candidate(ctrl: UIAWrapper) -> bool:
    try:
        if not ctrl.is_enabled() or not ctrl.is_visible():
            return False

        text = (ctrl.window_text() or "").strip()
        if _contains_any(text, CALL_BUTTON_KEYWORDS):
            return True

        auto_id = (getattr(ctrl.element_info, "automation_id", "") or "").strip().lower()
        name = (getattr(ctrl.element_info, "name", "") or "").strip().lower()
        control_type = (getattr(ctrl.element_info, "control_type", "") or "").strip().lower()
        composite = f"{auto_id} {name}"
        if auto_id == "buttoncall":
            return True
        if any(keyword in composite for keyword in ("call", "dial", "phone")):
            return True
        return control_type == "button" and _contains_any(composite, CALL_BUTTON_KEYWORDS)
    except Exception:
        return False


def _enumerate_candidate_buttons(window: UIAWrapper) -> Iterable[UIAWrapper]:
    try:
        descendants = window.descendants()
    except Exception:
        return []
    return [ctrl for ctrl in descendants if _is_call_candidate(ctrl)]


def _describe_ctrl(ctrl: UIAWrapper) -> str:
    try:
        name = (ctrl.window_text() or "").strip()
    except Exception:
        name = ""
    try:
        auto_id = (getattr(ctrl.element_info, "automation_id", "") or "").strip()
    except Exception:
        auto_id = ""
    try:
        control_type = (getattr(ctrl.element_info, "control_type", "") or "").strip()
    except Exception:
        control_type = ""
    return f"name='{name}' auto_id='{auto_id}' type='{control_type}'"


def _find_primary_call_button(window: UIAWrapper) -> Optional[UIAWrapper]:
    try:
        for ctrl in window.descendants():
            try:
                if not ctrl.is_enabled() or not ctrl.is_visible():
                    continue
            except Exception:
                continue
            auto_id = (getattr(ctrl.element_info, "automation_id", "") or "").strip()
            name = (getattr(ctrl.element_info, "name", "") or "").strip()
            control_type = (getattr(ctrl.element_info, "control_type", "") or "").strip().lower()
            if auto_id == "ButtonCall":
                _log(f"primary call button found via auto_id: {_describe_ctrl(ctrl)}")
                return ctrl
            if any(p in name for p in ("통화 전화 번호", "Call phone number", "전화 걸기", "통화하기")):
                _log(f"primary call button found via name pattern: {_describe_ctrl(ctrl)}")
                return ctrl
            if control_type == "button" and name.startswith("통화"):
                _log(f"primary call button found via button name prefix: {_describe_ctrl(ctrl)}")
                return ctrl
    except Exception as exc:
        _log(f"primary call button scan failed: {exc!r}")
        return None
    _log("primary call button not found")
    return None


def _find_calls_tab(window: UIAWrapper) -> Optional[UIAWrapper]:
    try:
        for ctrl in window.descendants(control_type="TabItem"):
            auto_id = (getattr(ctrl.element_info, "automation_id", "") or "").strip()
            name = (getattr(ctrl.element_info, "name", "") or "").strip().lower()
            if auto_id == CALLS_TAB_AUTO_ID:
                _log(f"calls tab found via auto_id: {_describe_ctrl(ctrl)}")
                return ctrl
            if "통화" in name or "calls" in name:
                _log(f"calls tab found via name: {_describe_ctrl(ctrl)}")
                return ctrl
    except Exception as exc:
        _log(f"calls tab scan failed: {exc!r}")
        return None
    _log("calls tab not found")
    return None


def _calls_view_ready(window: UIAWrapper) -> bool:
    try:
        for ctrl in window.descendants():
            auto_id = (getattr(ctrl.element_info, "automation_id", "") or "").strip()
            if auto_id in DIAL_VIEW_AUTO_IDS:
                _log(f"calls view ready via auto_id={auto_id}")
                return True
    except Exception as exc:
        _log(f"calls view ready scan failed: {exc!r}")
        return False
    _log("calls view not ready")
    return False


def _call_button_enabled(window: UIAWrapper) -> bool:
    btn = _find_primary_call_button(window)
    if btn is None:
        return False
    try:
        enabled = btn.is_enabled() and btn.is_visible()
        _log(f"call button enabled={enabled}")
        return enabled
    except Exception as exc:
        _log(f"call button enabled check failed: {exc!r}")
        return False


def _try_button_action(ctrl: UIAWrapper, action: str) -> bool:
    _log(f"trying button action={action}: {_describe_ctrl(ctrl)}")
    try:
        if action == "click_input":
            ctrl.set_focus()
            ctrl.click_input()
        elif action == "mouse_center":
            rect = ctrl.rectangle()
            mid = rect.mid_point()
            mouse.click(button="left", coords=(mid.x, mid.y))
            _log(f"mouse center click at ({mid.x}, {mid.y})")
        elif action == "invoke":
            ctrl.iface_invoke.Invoke()
        elif action == "enter":
            ctrl.type_keys("{ENTER}", set_foreground=True)
        elif action == "space":
            ctrl.type_keys(" ", set_foreground=True)
        else:
            raise RuntimeError(f"unknown action={action}")
        time.sleep(0.2)
        _log(f"button action={action} completed")
        return True
    except Exception as exc:
        _log(f"button action={action} failed: {exc!r}")
        return False


def _try_call_button_sequence(ctrl: UIAWrapper) -> bool:
    for action in ("click_input", "mouse_center", "invoke", "enter", "space"):
        if not _try_button_action(ctrl, action):
            continue
        _log(f"call trigger accepted after action={action}")
        return True
    return False


def _click_window_call_button_fallback(window: UIAWrapper, x_ratio: float) -> bool:
    try:
        _focus_window(window)
        time.sleep(0.15)
        rect = window.rectangle()
        x = int(rect.left + rect.width() * x_ratio)
        y = int(rect.top + rect.height() * WINDOW_CALL_BUTTON_Y_RATIO)
        _log(f"fallback click at window-relative coords ({x}, {y}) ratio={x_ratio}")
        mouse.click(button="left", coords=(x, y))
        time.sleep(0.2)
        return True
    except Exception as exc:
        _log(f"fallback coordinate click failed: {exc!r}")
        return False


def _find_phone_link_window() -> Optional[UIAWrapper]:
    desktop = Desktop(backend="uia")
    deadline = time.monotonic() + MAX_WINDOW_WAIT_SEC
    while time.monotonic() < deadline:
        for window in desktop.windows():
            try:
                title = window.window_text()
                if _window_matches(title):
                    _log(f"phone link window found: {title!r}")
                    return window
            except Exception:
                continue
        time.sleep(SCAN_INTERVAL_SEC)
    _log("phone link window not found")
    return None


def _focus_window(window: UIAWrapper) -> None:
    try:
        window.restore()
    except Exception:
        pass
    try:
        window.set_focus()
    except Exception:
        pass


def _switch_to_calls_tab(window: UIAWrapper) -> None:
    if _calls_view_ready(window):
        return

    tab = _find_calls_tab(window)
    if tab is not None:
        for action in ("click_input", "mouse_center", "invoke"):
            if _try_button_action(tab, action):
                time.sleep(0.4)
                if _calls_view_ready(window):
                    _log(f"switched to calls tab via {action}")
                    return

    _focus_window(window)
    try:
        send_keys("^3")
        time.sleep(0.4)
        if _calls_view_ready(window):
            _log("switched to calls tab via Ctrl+3")
            return
    except Exception as exc:
        _log(f"Ctrl+3 calls tab switch failed: {exc!r}")


def _find_unlock_prompt() -> Optional[UIAWrapper]:
    # Keep this probe intentionally lightweight. Full descendant scans were
    # causing the worker to stall before it ever reached the call-button stage.
    desktop = Desktop(backend="uia")
    for window in desktop.windows():
        try:
            title = (window.window_text() or "").strip()
        except Exception:
            continue
        if _contains_any(title, UNLOCK_PROMPT_KEYWORDS):
            return window
    return None


def _find_search_input(window: UIAWrapper) -> Optional[UIAWrapper]:
    try:
        for ctrl in window.descendants(control_type="Edit"):
            auto_id = (getattr(ctrl.element_info, "automation_id", "") or "").strip().lower()
            name = (getattr(ctrl.element_info, "name", "") or "").strip().lower()
            if any(k in auto_id for k in ("search", "query", "phone", "dial", "number")):
                _log(f"search input found via auto_id: {_describe_ctrl(ctrl)}")
                return ctrl
            if any(k in name for k in ("검색", "search", "전화", "번호", "dial")):
                _log(f"search input found via name: {_describe_ctrl(ctrl)}")
                return ctrl
    except Exception as exc:
        _log(f"search input scan failed: {exc!r}")
    try:
        edits = window.descendants(control_type="Edit")
        if edits:
            _log(f"search input falling back to first Edit: {_describe_ctrl(edits[0])}")
            return edits[0]
    except Exception as exc:
        _log(f"search input fallback failed: {exc!r}")
    _log("search input not found")
    return None


def main() -> None:
    if len(sys.argv) < 2:
        print("USAGE: python phone_link_worker.py [phone_number]")
        sys.exit(1)

    phone_number = sys.argv[1]
    _log(f"dial start phone_number={phone_number}")

    window = _find_phone_link_window()
    if window is None:
        _log("launching Phone Link via ms-phone-link:")
        subprocess.Popen("start ms-phone-link:", shell=True)
        time.sleep(2.0)
        window = _find_phone_link_window()

    if window is None:
        _log("ERROR: Phone Link window not found")
        print("ERROR: Phone Link window not found")
        sys.exit(1)

    _focus_window(window)
    _switch_to_calls_tab(window)
    time.sleep(0.3)

    search_input = _find_search_input(window)
    if search_input is not None:
        try:
            search_input.set_focus()
            search_input.click_input()
            time.sleep(0.2)
            send_keys("^a{DEL}", pause=0.05)
            time.sleep(0.1)
            send_keys(phone_number, pause=0.05)
            _log("phone number typed into search input")
            time.sleep(0.7)
            send_keys("{ENTER}")
            _log("ENTER sent to confirm number")
            time.sleep(0.35)
        except Exception as exc:
            _log(f"search input typing failed: {exc!r}")
    else:
        try:
            send_keys(phone_number, pause=0.05)
            time.sleep(0.7)
            send_keys("{ENTER}")
            _log("phone number typed via window fallback and ENTER sent")
            time.sleep(0.35)
        except Exception as exc:
            _log(f"window fallback typing failed: {exc!r}")

    _focus_window(window)
    time.sleep(0.15)
    _log("moving to explicit call button stage")
    primary = _find_primary_call_button(window)
    if primary is not None:
        if _try_call_button_sequence(primary):
            print(f"OK: call started for {phone_number} (via ButtonCall)")
            sys.exit(0)
    else:
        candidates = list(_enumerate_candidate_buttons(window))
        _log(f"candidate call buttons found={len(candidates)}")
        for ctrl in candidates:
            if _try_call_button_sequence(ctrl):
                print(f"OK: call started for {phone_number} (via candidate button)")
                sys.exit(0)
        try:
            send_keys("{ENTER}")
            _log("fallback ENTER sent from window focus")
            print(f"OK: call started for {phone_number} (via fallback ENTER)")
            sys.exit(0)
        except Exception as exc:
            _log(f"fallback ENTER failed: {exc!r}")

    for x_ratio in WINDOW_CALL_BUTTON_X_RATIOS:
        if _click_window_call_button_fallback(window, x_ratio=x_ratio):
            print(f"OK: call started for {phone_number} (via coordinate x={x_ratio})")
            sys.exit(0)

    if _find_unlock_prompt() is not None:
        _log("ERROR: device unlock required")
        print("ERROR: device_unlock_required")
        sys.exit(2)

    _log("ERROR: call trigger failed")
    print("ERROR: call trigger failed")
    sys.exit(1)


if __name__ == "__main__":
    main()
