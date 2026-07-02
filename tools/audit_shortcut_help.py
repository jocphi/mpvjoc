#!/usr/bin/env python3
from __future__ import annotations
import argparse
import re
import sys
from pathlib import Path

PROJECT = Path(__file__).resolve().parents[1]
SHORTCUTS_CPP = PROJECT / "ui" / "MainWindowShortcuts.cpp"
KEY_ALIAS = {
    "Key_Escape": ["Esc", "Escape"], "Key_Return": ["Enter", "Return"], "Key_Enter": ["Enter", "Return"],
    "Key_Delete": ["Delete", "Del"], "Key_Backspace": ["Backspace"], "Key_Tab": ["Tab"],
    "Key_PageUp": ["PageUp", "PgUp", "Page Up"], "Key_PageDown": ["PageDown", "PgDn", "Page Down"],
    "Key_Home": ["Home"], "Key_End": ["End"], "Key_Up": ["Up"], "Key_Down": ["Down"],
    "Key_Left": ["Left"], "Key_Right": ["Right"], "Key_Plus": ["+", "Plus"], "Key_Minus": ["-", "Minus"],
    "Key_Period": [".", "Period"], "Key_Comma": [",", "Comma"], "Key_Colon": [":", "Colon"],
    "Key_Semicolon": [";", "Semicolon"], "Key_onehalf": ["½", "onehalf", "50%"],

    "Key_Exclam": ["!", "1", "Exclam"],
    "Key_QuoteDbl": ["\"", "2", "QuoteDbl"],
    "Key_NumberSign": ["#", "3", "NumberSign"],
    "Key_currency": ["¤", "4", "currency"],
    "Key_Percent": ["%", "5", "Percent"],
    "Key_Ampersand": ["&", "7", "Ampersand"],}
IGNORED_KEYS = {"Key_Up", "Key_Down", "Key_Left", "Key_Right", "Key_PageUp", "Key_PageDown", "Key_Home", "Key_End"}
KEY_RE = re.compile(r"Qt::(Key_[A-Za-z0-9_]+)")

def find_function_range(text: str, signature: str) -> tuple[int, int] | None:
    start = text.find(signature)
    if start < 0: return None
    open_brace = text.find("{", start)
    if open_brace < 0: return None
    depth = 0
    for i in range(open_brace, len(text)):
        if text[i] == "{": depth += 1
        elif text[i] == "}":
            depth -= 1
            if depth == 0: return start, i + 1
    return None

def aliases_for(key: str) -> list[str]:
    if key in KEY_ALIAS: return KEY_ALIAS[key]
    if re.fullmatch(r"Key_[A-Z]", key): return [key[-1]]
    if re.fullmatch(r"Key_[0-9]", key): return [key[-1]]
    if re.fullmatch(r"Key_F\d+", key): return [key[4:]]
    return [key.replace("Key_", "")]

def collect_key_lines(text: str) -> dict[str, list[int]]:
    result: dict[str, list[int]] = {}
    for line_no, line in enumerate(text.splitlines(), start=1):
        for match in KEY_RE.finditer(line):
            result.setdefault(match.group(1), []).append(line_no)
    return {key: sorted(set(lines)) for key, lines in result.items()}

def format_lines(lines: list[int]) -> str:
    shown = lines[:8]
    suffix = "" if len(lines) <= 8 else f", +{len(lines) - 8} more"
    return ", ".join(str(x) for x in shown) + suffix

def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description="Audit Qt shortcut keys against F12 help text.")
    parser.add_argument("--list", action="store_true", help="List all discovered Qt::Key_* references and line numbers.")
    parser.add_argument("--strict", action="store_true", help="Return 1 when possible missing help entries are found.")
    args = parser.parse_args(argv)
    if not SHORTCUTS_CPP.exists():
        print(f"ERROR: missing {SHORTCUTS_CPP}", file=sys.stderr); return 2
    text = SHORTCUTS_CPP.read_text(encoding="utf-8")
    key_lines = collect_key_lines(text); keys = sorted(key_lines)
    help_range = find_function_range(text, "QString MainWindow::shortcutHelpText()const")
    if help_range is None:
        print("ERROR: could not find shortcutHelpText()", file=sys.stderr); return 2
    help_text = text[help_range[0]:help_range[1]]
    print("Shortcut help audit")
    print("===================")
    print(f"File: {SHORTCUTS_CPP}")
    print(f"Qt::Key references found: {len(keys)}")
    print(f"Ignored navigation keys: {len([k for k in keys if k in IGNORED_KEYS])}")
    print()
    if args.list:
        print("All discovered keys:")
        for key in keys:
            ignored = " (ignored navigation)" if key in IGNORED_KEYS else ""
            print(f"  {key:<18} lines {format_lines(key_lines[key])}{ignored}")
        print()
    missing: list[tuple[str, str, list[int]]] = []
    for key in keys:
        aliases = aliases_for(key)
        if key in IGNORED_KEYS: continue
        if not any(alias in help_text for alias in aliases):
            missing.append((key, ", ".join(aliases), key_lines[key]))
    if missing:
        print("Possibly missing from F12 help:")
        for key, display, used_lines in missing:
            print(f"  {key:<18} lines {format_lines(used_lines):<18} expected text like: {display}")
        print()
        print("Note: This is heuristic. Some keys may be internal or documented with different wording.")
        if args.strict: return 1
        print("Non-strict mode: returning success. Use --strict to make missing entries fail.")
        return 0
    print("No obvious missing F12 help entries found.")
    return 0
if __name__ == "__main__":
    raise SystemExit(main())
