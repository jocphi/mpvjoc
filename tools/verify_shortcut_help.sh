#!/usr/bin/env bash
set -euo pipefail

usage() {
    cat <<'EOF'
Usage: tools/verify_shortcut_help.sh [--strict]

Runs shortcut-help audit verification from the project root.

Default mode:
  - python3 -m py_compile tools/audit_shortcut_help.py
  - python3 tools/audit_shortcut_help.py --context 140
  - cmake --build build --target audit-shortcut-help

Strict mode adds:
  - python3 tools/audit_shortcut_help.py --strict --context 140
  - cmake --build build --target audit-shortcut-help-strict
EOF
}

strict=0
for arg in "$@"; do
    case "$arg" in
        --strict)
            strict=1
            ;;
        -h|--help)
            usage
            exit 0
            ;;
        *)
            echo "ERROR: unknown argument: $arg" >&2
            usage >&2
            exit 2
            ;;
    esac
done

script_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
project_root="$(cd "$script_dir/.." && pwd)"
cd "$project_root"

if [[ ! -f tools/audit_shortcut_help.py ]]; then
    echo "ERROR: missing tools/audit_shortcut_help.py" >&2
    exit 2
fi

if [[ ! -d build ]]; then
    echo "ERROR: missing build directory. Configure/build the project first." >&2
    exit 2
fi

echo "[1/3] Syntax-checking shortcut audit tool"
python3 -m py_compile tools/audit_shortcut_help.py

echo
echo "[2/3] Running shortcut audit with source context"
python3 tools/audit_shortcut_help.py --context 140

echo
echo "[3/3] Running CMake non-strict shortcut audit target"
cmake --build build --target audit-shortcut-help

if [[ "$strict" -eq 1 ]]; then
    echo
    echo "[strict 1/2] Running strict shortcut audit with source context"
    python3 tools/audit_shortcut_help.py --strict --context 140

    echo
    echo "[strict 2/2] Running CMake strict shortcut audit target"
    cmake --build build --target audit-shortcut-help-strict
fi

echo
echo "Shortcut help verification completed."
