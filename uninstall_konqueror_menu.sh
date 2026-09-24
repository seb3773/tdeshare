#!/usr/bin/env bash
set -euo pipefail

echo "=== Uninstalling Konqueror, Dolphin & Krusader 'Share...' Actions ==="

# 1. Clean Trinity servicemenus
for d in "$HOME/.trinity/share/apps/konqueror/servicemenus" \
         "$HOME/.trinity/share/apps/d3lphin/servicemenus" \
         "$HOME/.trinity/share/apps/dolphin/servicemenus"; do
    f="$d/tdeshare.desktop"
    if [ -f "$f" ]; then
        rm -f "$f"
        echo "Removed $f"
    fi
done

# 2. Clean KF5 servicemenu
KF5_F="$HOME/.local/share/kservices5/ServiceMenus/tdeshare.desktop"
if [ -f "$KF5_F" ]; then
    rm -f "$KF5_F"
    echo "Removed $KF5_F"
fi

# 3. Clean Krusader useractions
python3 - << 'EOF'
import os
import xml.etree.ElementTree as ET

action_name = "tdeshare_folder"
targets = [
    os.path.expanduser("~/.trinity/share/apps/krusader/useractions.xml"),
    os.path.expanduser("~/.local/share/krusader/useractions.xml")
]

for p in targets:
    if os.path.exists(p) and os.path.getsize(p) > 0:
        try:
            tree = ET.parse(p)
            root = tree.getroot()
            removed = False
            for act in list(root.findall('action')):
                if act.get('name') == action_name:
                    root.remove(act)
                    removed = True
            if removed:
                content = ET.tostring(root, encoding='utf-8').decode('utf-8')
                with open(p, 'w', encoding='utf-8') as f:
                    f.write(content)
                print(f"Removed UserAction from: {p}")
        except Exception:
            pass
EOF

# 4. Clean system-wide if any
for d in "/opt/trinity/share/apps/konqueror/servicemenus" \
         "/opt/trinity/share/apps/d3lphin/servicemenus" \
         "/opt/trinity/share/apps/dolphin/servicemenus"; do
    f="$d/tdeshare.desktop"
    if [ -f "$f" ]; then
        sudo rm -f "$f"
        echo "Removed $f"
    fi
done

if [ -f "/usr/share/kservices5/ServiceMenus/tdeshare.desktop" ]; then
    sudo rm -f "/usr/share/kservices5/ServiceMenus/tdeshare.desktop"
    echo "Removed /usr/share/kservices5/ServiceMenus/tdeshare.desktop"
fi

# 5. Refresh caches non-destructively
if command -v /opt/trinity/bin/tdebuildsycoca >/dev/null 2>&1; then
    /opt/trinity/bin/tdebuildsycoca --checkstamps >/dev/null 2>&1 || true
fi
if command -v /usr/bin/kbuildsycoca5 >/dev/null 2>&1; then
    /usr/bin/kbuildsycoca5 --checkstamps >/dev/null 2>&1 || true
fi

echo "Done!"
