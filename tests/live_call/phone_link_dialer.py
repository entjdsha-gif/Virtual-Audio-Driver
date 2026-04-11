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
from pathlib import Path
import subprocess
import sys
import threading
import time
from typing import Iterable
from urllib.parse import quote

from pywinauto import Desktop

log = logging.getLogger(__name__)

WORKER = os.path.join(os.path.dirname(__file__), "phone_link_worker.py")
CALL_POPUP_KEYWORDS = ("PC에서 통화", "Calling from PC")
CALL_POPUP_DESCENDANT_KEYWORDS = (
    "음소거",
    "종료",
    "모바일 장치로 전송",
    "Mute",
    "End",
    "Transfer to mobile device",
)

PACKAGE_LOCALSTATE = Path(
    os.path.expandvars(r"%LOCALAPPDATA%\Packages\Microsoft.YourPhone_8wekyb3d8bbwe\LocalState")
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


def _resolve_device_id() -> str | None:
    env_value = os.getenv("PHONE_LINK_DEVICE_ID", "").strip()
    if env_value:
        return env_value

    try:
        if APP_SETTINGS.exists():
            text = APP_SETTINGS.read_text(encoding="utf-8", errors="ignore")
            for line in text.splitlines():
                if "DeviceId" not in line:
                    continue
                if "=" not in line:
                    continue
                candidate = line.split("=", 1)[1].strip().strip('"')
                if candidate:
                    return candidate
    except Exception as exc:
        log.debug("AppSettings device id parse failed: %s", exc)

    try:
        if STARTMENU_JSON.exists():
            data = json.loads(STARTMENU_JSON.read_text(encoding="utf-8", errors="ignore"))
            values = data.values() if isinstance(data, dict) else data
            for item in values:
                if not isinstance(item, dict):
                    continue
                url = str(item.get("url") or "")
                marker = "deviceid="
                lower = url.lower()
                if marker not in lower:
                    continue
                start = lower.index(marker) + len(marker)
                rest = url[start:]
                candidate = rest.split("&", 1)[0]
                if candidate:
                    return candidate
    except Exception as exc:
        log.debug("StartMenuCompanion device id parse failed: %s", exc)

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


def call_popup_visible() -> bool:
    """Return True only when the live Phone Link call popup is visible."""
    try:
        desktop = Desktop(backend="uia")
        for window in desktop.windows():
            try:
                title = window.window_text()
                if _contains_any(title, CALL_POPUP_KEYWORDS):
                    return True

                element_text = str(window.element_info)
                if "PhoneExperienceHost.exe" not in element_text:
                    continue

                child_texts: list[str] = []
                for child in window.descendants():
                    try:
                        text = (child.window_text() or "").strip()
                    except Exception:
                        continue
                    if text:
                        child_texts.append(text)

                if child_texts and _contains_any(
                    " | ".join(child_texts), CALL_POPUP_DESCENDANT_KEYWORDS
                ):
                    log.debug("call popup detected by descendants")
                    return True
            except Exception:
                continue
    except Exception as exc:
        log.debug("call_popup_visible failed: %s", exc)
    return False


def dial_async(phone_number: str) -> None:
    """Fire-and-forget helper kept for compatibility."""
    threading.Thread(target=dial, args=(phone_number,), daemon=True).start()
