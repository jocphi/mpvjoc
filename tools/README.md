# mpvjoc development tools

This directory contains small development helpers for maintaining mpvjoc.

## Shortcut help audit

The shortcut help overlay shown by `F12` is currently maintained manually in:

```text
ui/MainWindowShortcuts.cpp
```

The audit helper checks whether `Qt::Key_*` shortcut references in that file are
likely documented in the `F12` help text.

### Quick verification

Run the standard shortcut-help verification script from the project root:

```bash
tools/verify_shortcut_help.sh
```

This runs:

```bash
python3 -m py_compile tools/audit_shortcut_help.py
python3 tools/audit_shortcut_help.py --context 140
cmake --build build --target audit-shortcut-help
```

### Strict verification

Strict mode returns a failure if possible missing `F12` help entries are found:

```bash
tools/verify_shortcut_help.sh --strict
```

This is useful after shortcut/help cleanup, or before merging a shortcut-heavy
change.

### Direct audit commands

Run the audit directly:

```bash
python3 tools/audit_shortcut_help.py
```

Show all discovered `Qt::Key_*` references:

```bash
python3 tools/audit_shortcut_help.py --list
```

Show source snippets around findings:

```bash
python3 tools/audit_shortcut_help.py --context 140
```

Fail when findings exist:

```bash
python3 tools/audit_shortcut_help.py --strict --context 140
```

### CMake targets

Non-strict target, for normal development visibility:

```bash
cmake --build build --target audit-shortcut-help
```

Strict target, for cleanup/CI-style checks:

```bash
cmake --build build --target audit-shortcut-help-strict
```

### Notes

The audit is intentionally heuristic. It is designed to catch obvious omissions,
not to replace a future single-source shortcut registry.

If the audit reports a shortcut as missing:

1. Run with source context:

   ```bash
   python3 tools/audit_shortcut_help.py --context 140
   ```

2. Confirm what the shortcut actually does in `ui/MainWindowShortcuts.cpp`.
3. Add or update the appropriate line in `shortcutHelpText()`.
4. Run strict verification again.

Recommended check after adding or changing keyboard shortcuts:

```bash
tools/verify_shortcut_help.sh --strict
```
