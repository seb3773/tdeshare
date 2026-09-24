#!/usr/bin/env python3
"""
krusader_action.py - Helper for installing/uninstalling tdeshare action in Krusader
"""
import sys
import os
import glob
import xml.etree.ElementTree as ET

ACTION_NAME = "tdeshare_folder"
ACTION_XML = """ <action name="tdeshare_folder" >
  <title>Share with Samba...</title>
  <tooltip>Manage Samba network share for this folder</tooltip>
  <icon>network</icon>
  <category>Network</category>
  <description>Share this folder via Samba with tdeshare</description>
  <command>tdeshare %aCurrent%</command>
  <defaultshortcut></defaultshortcut>
 </action>"""

def get_target_files():
    targets = []
    homes = glob.glob("/home/*") + ["/root"]
    for h in homes:
        if os.path.isdir(h):
            targets.append(os.path.join(h, ".trinity", "share", "apps", "krusader", "useractions.xml"))
            targets.append(os.path.join(h, ".local", "share", "krusader", "useractions.xml"))
    return targets

def install_action():
    for p in get_target_files():
        parent = os.path.dirname(p)
        home_dir = p.split("/.trinity")[0] if "/.trinity" in p else p.split("/.local")[0]
        
        # Only install if the user already has a Trinity or Krusader config
        if os.path.exists(parent) or os.path.exists(os.path.join(home_dir, ".trinity")):
            try:
                os.makedirs(parent, exist_ok=True)
                content = ""
                if os.path.exists(p) and os.path.getsize(p) > 0:
                    try:
                        tree = ET.parse(p)
                        root = tree.getroot()
                        for act in list(root.findall('action')):
                            if act.get('name') == ACTION_NAME:
                                root.remove(act)
                        root.append(ET.fromstring(ACTION_XML.strip()))
                        content = ET.tostring(root, encoding='utf-8').decode('utf-8')
                    except Exception:
                        content = f'<?xml version="1.0" encoding="UTF-8" ?>\n<!DOCTYPE KrusaderUserActions>\n<KrusaderUserActions>\n{ACTION_XML.strip()}\n</KrusaderUserActions>\n'
                else:
                    content = f'<?xml version="1.0" encoding="UTF-8" ?>\n<!DOCTYPE KrusaderUserActions>\n<KrusaderUserActions>\n{ACTION_XML.strip()}\n</KrusaderUserActions>\n'

                with open(p, 'w', encoding='utf-8') as f:
                    f.write(content)

                # Ensure proper ownership matches the home directory
                if os.path.exists(home_dir):
                    hst = os.stat(home_dir)
                    os.chown(p, hst.st_uid, hst.st_gid)
                    if os.path.exists(parent):
                        os.chown(parent, hst.st_uid, hst.st_gid)
            except Exception:
                pass

def uninstall_action():
    for p in get_target_files():
        if os.path.exists(p) and os.path.getsize(p) > 0:
            try:
                tree = ET.parse(p)
                root = tree.getroot()
                removed = False
                for act in list(root.findall('action')):
                    if act.get('name') == ACTION_NAME:
                        root.remove(act)
                        removed = True
                if removed:
                    content = ET.tostring(root, encoding='utf-8').decode('utf-8')
                    with open(p, 'w', encoding='utf-8') as f:
                        f.write(content)
            except Exception:
                pass

if __name__ == "__main__":
    if len(sys.argv) > 1 and sys.argv[1] == "--uninstall":
        uninstall_action()
    else:
        install_action()
