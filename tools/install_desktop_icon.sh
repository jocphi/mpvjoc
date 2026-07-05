#!/usr/bin/env bash
set -euo pipefail

project_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
app_name="mpvjoc"
exe="${project_dir}/build/mpvjoc"
icon_src="${project_dir}/assets/mpvjoc.svg"
icon_dir="${HOME}/.local/share/icons/hicolor/scalable/apps"
desktop_dir="${HOME}/.local/share/applications"
desktop_file="${desktop_dir}/${app_name}.desktop"

if [[ ! -x "$exe" ]]; then
    echo "ERROR: executable not found or not executable: $exe" >&2
    echo "Build first with:" >&2
    echo "  cmake --build build" >&2
    exit 1
fi

if [[ ! -f "$icon_src" ]]; then
    echo "ERROR: icon not found: $icon_src" >&2
    exit 1
fi

mkdir -p "$icon_dir" "$desktop_dir"
cp "$icon_src" "${icon_dir}/${app_name}.svg"

cat > "$desktop_file" <<EOF
[Desktop Entry]
Type=Application
Name=mpvjoc
GenericName=Media playlist review player
Comment=Qt/libmpv playlist-oriented media player
Exec=${exe}
Icon=${app_name}
Terminal=false
Categories=AudioVideo;Player;Video;
StartupNotify=true
StartupWMClass=${app_name}
EOF

chmod 0644 "$desktop_file"

echo "Installed icon: ${icon_dir}/${app_name}.svg"
echo "Installed launcher: ${desktop_file}"
echo
if command -v kbuildsycoca6 >/dev/null 2>&1; then
    echo "Refreshing KDE service cache..."
    kbuildsycoca6 >/dev/null 2>&1 || true
fi

echo "Next steps:"
echo "  1. Start mpvjoc from the launcher once, or run: ${exe}"
echo "  2. If Plasma still shows a generic icon, log out/in or restart plasmashell."
