#!/usr/bin/env bash
set -euo pipefail

SRC_ROOT="$(cd "$(dirname "$0")" && pwd)"
SERVICE_DESKTOP="$SRC_ROOT/data/tdeshare_konqueror.desktop"
KF5_DESKTOP="$SRC_ROOT/data/tdeshare_kf5.desktop"
BIN_PATH="$SRC_ROOT/build/tdeshare"

SYSTEM_MODE=0
if test "${1:-}" = "--system" || test "${1:-}" = "-s"; then
    SYSTEM_MODE=1
fi

echo "=== Installing Konqueror, Dolphin & Krusader 'Share...' Actions ==="

# 1. Ensure binary exists
if [ ! -f "$BIN_PATH" ]; then
    echo "Notice: tdeshare binary not found in build directory. Building now..."
    "$SRC_ROOT/build.sh"
fi

# 2. Install binary to PATH if not already installed in system
if [ "$SYSTEM_MODE" -eq 1 ]; then
    echo "Installing system-wide (/opt/trinity and /usr/local/bin)..."
    sudo install -m 755 "$BIN_PATH" /usr/local/bin/tdeshare
    for d in /opt/trinity/share/apps/konqueror/servicemenus \
             /opt/trinity/share/apps/d3lphin/servicemenus \
             /opt/trinity/share/apps/dolphin/servicemenus; do
        sudo install -d "$d"
        sudo install -m 644 "$SERVICE_DESKTOP" "$d/tdeshare.desktop"
    done
    if [ -d /usr/share/kservices5/ServiceMenus ]; then
        sudo install -m 644 "$KF5_DESKTOP" /usr/share/kservices5/ServiceMenus/tdeshare.desktop
    fi
    echo "Successfully installed system-wide!"
else
    # User-level installation (no root needed)
    USER_BIN_DIR="$HOME/.local/bin"
    mkdir -p "$USER_BIN_DIR"

    # Always ensure ~/.local/bin/tdeshare points to the current build binary
    ln -sf "$BIN_PATH" "$USER_BIN_DIR/tdeshare"
    echo "Linked tdeshare binary to $USER_BIN_DIR/tdeshare"

    # 2a. Install for Konqueror and Dolphin (d3lphin)
    for d in "$HOME/.trinity/share/apps/konqueror/servicemenus" \
             "$HOME/.trinity/share/apps/d3lphin/servicemenus" \
             "$HOME/.trinity/share/apps/dolphin/servicemenus"; do
        mkdir -p "$d"
        cp "$SERVICE_DESKTOP" "$d/tdeshare.desktop"
        echo "Installed service menu to: $d/tdeshare.desktop"
    done

    # 2b. Install for KF5 applications (including KF5 Krusader / Dolphin)
    KF5_USER_MENU="$HOME/.local/share/kservices5/ServiceMenus"
    mkdir -p "$KF5_USER_MENU"
    cp "$KF5_DESKTOP" "$KF5_USER_MENU/tdeshare.desktop"
    echo "Installed KF5 service menu to: $KF5_USER_MENU/tdeshare.desktop"

    # 2c. Programmatically insert UserAction into Krusader ActionMan
    python3 - << 'EOF'
import os
import xml.etree.ElementTree as ET

action_name = "tdeshare_folder"
action_xml = '''
 <action name="tdeshare_folder" >
  <title>Share with Samba...</title>
  <tooltip>Manage Samba network share for this folder</tooltip>
  <icon>network</icon>
  <category>Network</category>
  <description>Share this folder via Samba with tdeshare</description>
  <command>tdeshare %aCurrent%</command>
  <defaultshortcut></defaultshortcut>
 </action>
'''.strip()

targets = [
    os.path.expanduser("~/.trinity/share/apps/krusader/useractions.xml"),
    os.path.expanduser("~/.local/share/krusader/useractions.xml")
]

for p in targets:
    os.makedirs(os.path.dirname(p), exist_ok=True)
    if os.path.exists(p) and os.path.getsize(p) > 0:
        try:
            tree = ET.parse(p)
            root = tree.getroot()
            for act in list(root.findall('action')):
                if act.get('name') == action_name:
                    root.remove(act)
            root.append(ET.fromstring(action_xml))
            content = ET.tostring(root, encoding='utf-8').decode('utf-8')
        except Exception:
            content = f'<?xml version="1.0" encoding="UTF-8" ?>\n<!DOCTYPE KrusaderUserActions>\n<KrusaderUserActions>\n{action_xml}\n</KrusaderUserActions>\n'
    else:
        content = f'<?xml version="1.0" encoding="UTF-8" ?>\n<!DOCTYPE KrusaderUserActions>\n<KrusaderUserActions>\n{action_xml}\n</KrusaderUserActions>\n'

    with open(p, 'w', encoding='utf-8') as f:
        f.write(content)
    print(f"Installed Krusader UserAction to: {p}")
EOF
fi

# 3. Update Trinity & KF5 Sycoca caches non-destructively
if command -v /opt/trinity/bin/tdebuildsycoca >/dev/null 2>&1; then
    /opt/trinity/bin/tdebuildsycoca --checkstamps >/dev/null 2>&1 || true
fi
if command -v /usr/bin/kbuildsycoca5 >/dev/null 2>&1; then
    /usr/bin/kbuildsycoca5 --checkstamps >/dev/null 2>&1 || true
fi

echo ""
echo "Done! Actions successfully installed for Konqueror, Dolphin, and Krusader."
