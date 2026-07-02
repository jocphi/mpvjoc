#!/usr/bin/env python3
"""
audit_shortcut_help.py

Development helper for mpvjoc.

Scans ui/MainWindowShortcuts.cpp and reports Qt::Key_* values used in shortcut
handling that may not be documented in shortcutHelpText()/F12 help.

This is intentionally heuristic; it is meant to catch obvious omissions, not to
replace a future single-source shortcut registry.
"""
from __future__ import annotations

import re
import sys
from pathlib import Path

PROJECT = Path(__file__).resolve().parents[1]
SHORTCUTS_CPP = PROJECT / "ui" / "MainWindowShortcuts.cpp"

KEY_ALIAS = {
    "Key_Escape": ["Esc", "Escape"],
    "Key_Return": ["Enter", "Return"],
    "Key_Enter": ["Enter", "Return"],
    "Key_Delete": ["Delete", "Del"],
    "Key_Backspace": ["Backspace"],
    "Key_Tab": ["Tab"],
    "Key_PageUp": ["PageUp", "PgUp", "Page Up"],
    "Key_PageDown": ["PageDown", "PgDn", "Page Down"],
    "Key_Home": ["Home"],
    "Key_End": ["End"],
    "Key_Up": ["Up"],
    "Key_Down": ["Down"],
    "Key_Left": ["Left"],
    "Key_Right": ["Right"],
    "Key_Plus": ["+", "Plus"],
    "Key_Minus": ["-", "Minus"],
    "Key_Period": [".", "Period"],
    "Key_Comma": [",", "Comma"],
    "Key_Colon": [":", "Colon"],
    "Key_Semicolon": [";", "Semicolon"],
    "Key_onehalf": ["½", "onehalf", "50%"],
}

IGNORED_KEYS = {
    "Key_Up", "Key_Down", "Key_Left", "Key_Right", "Key_PageUp", "Key_PageDown", "Key_Home", "Key_End",
}


def find_function_range(text: str, signature: str) -> tuple[int, int] | None:
    start = text.find(signature)
    if start < 0:
        return None
    open_brace = text.find("{", start)
    if open_brace < 0:
        return None
    depth = 0
    in_string = False
    in_char = False
    escape = False
    for i in range(open_brace, len(text)):
        ch = text[i]
        if in_string:
            if escape:
                escape = False
            elif ch == "\":
                escape = True
            elif ch == '"':
                in_string = False
            continue
        if in_char:
            if escape:
                escape = False
            elif ch == "\":
                escape = True
            elif ch == "'":
                in_char = False
            continue
        if ch == '"':
            in_string = True
            continue
        if ch == "'":
            in_char = True
            continue
        if ch == "{":
            depth += 1
        elif ch == "}":
            depth -= 1
            if depth == 0:
                return start, i + 1
    return None


def aliases_for(key: str) -> list[str]:
    if key in KEY_ALIAS:
        return KEY_ALIAS[key]
    if re.fullmatch(r"Key_[A-Z]", key):
        return [key[-1]]
    if re.fullmatch(r"Key_[0-9]", key):
        return [key[-1]]
    if re.fullmatch(r"Key_F\d+", key):
        return [key[4:]]
    return [key.replace("Key_", "")]


def main() -> int:
    if not SHORTCUTS_CPP.exists():
        print(f"ERROR: missing {SHORTCUTS_CPP}", file=sys.stderr)
        return 2

    text = SHORTCUTS_CPP.read_text(encoding="utf-8")
    keys = sorted(set(re.findall(r"Qt::(Key_[A-Za-z0-9_]+)", text)))
    help_range = find_function_range(text, "QString MainWindow::shortcutHelpText()const")
    if help_range is None:
        print("ERROR: could not find shortcutHelpText()", file=sys.stderr)
        return 2
    help_text = text[help_range[0]:help_range[1]]

    missing: list[tuple[str, str]] = []
    ignored: list[str] = []
    for key in keys:
        aliases = aliases_for(key)
        if key in IGNORED_KEYS:
            ignored.append(key)
            continue
        documented = any(alias in help_text for alias in aliases)
        if not documented:
            missing.append((key, ", ".join(aliases)))

    print("Shortcut help audit")
    print("===================")
    print(f"File: {SHORTCUTS_CPP}")
    print(f"Qt::Key references found: {len(keys)}")
    print(f"Ignored navigation keys: {len(ignored)}")
    print()

    if missing:
        print("Possibly missing from F12 help:")
        for key, display in missing:
            print(f"  {key:<18} expected text like: {display}")
        print()
        print("Note: This is heuristic. Some keys may be internal or documented with different wording.")
        return 1

    print("No obvious missing F12 help entries found.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
