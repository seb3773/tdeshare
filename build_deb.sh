#!/usr/bin/env bash
set -euo pipefail

PKG_NAME="tdeshare"
PKG_VERSION="${1:-1.0}"
PKG_MAINTAINER="seb3773"
PKG_SECTION="admin"
PKG_PRIORITY="optional"

SRC_ROOT="$(cd "$(dirname "$0")" && pwd)"
ARCH="$(dpkg --print-architecture)"
BUILD_DIR="$SRC_ROOT/build"
PKGROOT="$BUILD_DIR/pkgroot"
PKGTMP="$BUILD_DIR/pkgtmp"

need_cmd() {
	command -v "$1" >/dev/null 2>&1 || {
		echo "error: missing required command: $1" >&2
		exit 1
	}
}

need_cmd cmake
need_cmd pkg-config
need_cmd dpkg-deb
need_cmd strip
need_cmd sed
need_cmd awk
need_cmd du
need_cmd install

mkdir -p -- "$BUILD_DIR"
rm -rf -- "$PKGTMP"
mkdir -p -- "$PKGTMP"

echo "=== Building tdeshare binary for package v${PKG_VERSION} (${ARCH}) ==="
"$SRC_ROOT/build.sh" "$PKG_VERSION"

BIN_PATH="$BUILD_DIR/tdeshare"
if test ! -x "$BIN_PATH"; then
	echo "error: missing built binary: $BIN_PATH" >&2
	exit 1
fi

# Stage filesystem layout
rm -rf -- "$PKGROOT"
mkdir -p -- \
	"$PKGROOT/DEBIAN" \
	"$PKGROOT/usr/bin" \
	"$PKGROOT/usr/share/applications" \
	"$PKGROOT/usr/share/tdeshare/servicemenus" \
	"$PKGROOT/usr/share/tdeshare/scripts"

# Install binary
install -m 0755 "$BIN_PATH" "$PKGROOT/usr/bin/tdeshare"

# Install desktop entry
install -m 0644 "$SRC_ROOT/data/tdeshare.desktop" "$PKGROOT/usr/share/applications/tdeshare.desktop"

# Install service menu templates and helper scripts
install -m 0644 "$SRC_ROOT/data/tdeshare_konqueror.desktop" "$PKGROOT/usr/share/tdeshare/servicemenus/tdeshare_konqueror.desktop"
install -m 0644 "$SRC_ROOT/data/tdeshare_kf5.desktop" "$PKGROOT/usr/share/tdeshare/servicemenus/tdeshare_kf5.desktop"
install -m 0755 "$SRC_ROOT/data/krusader_action.py" "$PKGROOT/usr/share/tdeshare/scripts/krusader_action.py"

# Strip/sstrip staged binary
STAGED_BIN="$PKGROOT/usr/bin/tdeshare"
if command -v sstrip >/dev/null 2>&1; then
	echo "info: stripping staged binary with sstrip"
	sstrip "$STAGED_BIN" >/dev/null 2>&1 || true
else
	echo "info: sstrip not found, using strip --strip-all"
	strip --strip-all "$STAGED_BIN" >/dev/null 2>&1 || true
fi

# Specify runtime dependencies
DEPENDS="libtqt3-mt-trinity (>= 4:14.0.0) | libtqt3-mt, libtdecore14-trinity (>= 4:14.0.0) | tdelibs-trinity, samba, samba-common-bin, python3"
RECOMMENDS="tdesudo | kdesu"
SUGGESTS="konqueror-trinity | dolphin-trinity | krusader"

# Debian control file
INSTALLED_SIZE_KB="$(du -sk "$PKGROOT/usr" | awk '{print $1}')"
cat > "$PKGROOT/DEBIAN/control" <<EOF
Package: $PKG_NAME
Version: $PKG_VERSION
Section: $PKG_SECTION
Priority: $PKG_PRIORITY
Architecture: $ARCH
Maintainer: $PKG_MAINTAINER
Installed-Size: $INSTALLED_SIZE_KB
Depends: $DEPENDS
Recommends: $RECOMMENDS
Suggests: $SUGGESTS
Description: Samba Network Shares Manager for Trinity Desktop (TDE)
 A native TQt3/TDE graphical utility that allows users to easily share local
 directories over Samba without directly modifying /etc/samba/smb.conf.
 Features multi-user ACL management, live filesystem permission diagnostics,
 global Samba options, and deep file manager integration for Konqueror,
 Dolphin, d3lphin, and Krusader.
EOF

# postinst: detect file managers, deploy actions, and refresh caches
cat > "$PKGROOT/DEBIAN/postinst" <<'EOF'
#!/bin/sh
set -e

SERVICE_TDE="/usr/share/tdeshare/servicemenus/tdeshare_konqueror.desktop"
SERVICE_KF5="/usr/share/tdeshare/servicemenus/tdeshare_kf5.desktop"

# 1. Konqueror detection & integration
if [ -x /opt/trinity/bin/konqueror ] || [ -x /usr/bin/konqueror ] || [ -d /opt/trinity/share/apps/konqueror ] || dpkg-query -W -f='${Status}' konqueror-trinity 2>/dev/null | grep -q "ok installed"; then
    mkdir -p /opt/trinity/share/apps/konqueror/servicemenus
    install -m 0644 "$SERVICE_TDE" /opt/trinity/share/apps/konqueror/servicemenus/tdeshare.desktop
    if [ -d /usr/share/apps/konqueror ]; then
        mkdir -p /usr/share/apps/konqueror/servicemenus
        install -m 0644 "$SERVICE_TDE" /usr/share/apps/konqueror/servicemenus/tdeshare.desktop
    fi
fi

# 2. Dolphin & d3lphin detection & integration
if [ -x /opt/trinity/bin/d3lphin ] || [ -x /opt/trinity/bin/dolphin ] || [ -d /opt/trinity/share/apps/d3lphin ] || [ -d /opt/trinity/share/apps/dolphin ] || dpkg-query -W -f='${Status}' dolphin-trinity 2>/dev/null | grep -q "ok installed"; then
    mkdir -p /opt/trinity/share/apps/d3lphin/servicemenus
    install -m 0644 "$SERVICE_TDE" /opt/trinity/share/apps/d3lphin/servicemenus/tdeshare.desktop
    mkdir -p /opt/trinity/share/apps/dolphin/servicemenus
    install -m 0644 "$SERVICE_TDE" /opt/trinity/share/apps/dolphin/servicemenus/tdeshare.desktop
fi
if [ -d /usr/share/kservices5/ServiceMenus ] || [ -x /usr/bin/dolphin ]; then
    mkdir -p /usr/share/kservices5/ServiceMenus
    install -m 0644 "$SERVICE_KF5" /usr/share/kservices5/ServiceMenus/tdeshare.desktop
fi

# 3. Krusader detection & integration
if [ -x /usr/bin/krusader ] || [ -x /opt/trinity/bin/krusader ] || [ -d /opt/trinity/share/apps/krusader ] || [ -d /usr/share/krusader ] || dpkg-query -W -f='${Status}' krusader 2>/dev/null | grep -q "ok installed" || dpkg-query -W -f='${Status}' krusader-trinity 2>/dev/null | grep -q "ok installed"; then
    if [ -x /usr/share/tdeshare/scripts/krusader_action.py ] && command -v python3 >/dev/null 2>&1; then
        python3 /usr/share/tdeshare/scripts/krusader_action.py --install || true
    fi
fi

# 4. Refresh Trinity and system desktop caches
if [ -x /opt/trinity/bin/tdebuildsycoca ]; then
    /opt/trinity/bin/tdebuildsycoca --checkstamps >/dev/null 2>&1 || true
fi
if command -v kbuildsycoca5 >/dev/null 2>&1; then
    kbuildsycoca5 --checkstamps >/dev/null 2>&1 || true
fi
if command -v update-desktop-database >/dev/null 2>&1; then
    update-desktop-database -q /usr/share/applications >/dev/null 2>&1 || true
fi
if command -v gtk-update-icon-cache >/dev/null 2>&1; then
    gtk-update-icon-cache -f -t /usr/share/icons/hicolor >/dev/null 2>&1 || true
fi

exit 0
EOF
chmod 0755 "$PKGROOT/DEBIAN/postinst"

# prerm: clean up file manager integrations before removing files
cat > "$PKGROOT/DEBIAN/prerm" <<'EOF'
#!/bin/sh
set -e

if [ "$1" = "remove" ] || [ "$1" = "purge" ] || [ "$1" = "deconfigure" ]; then
    # 1. Clean service menus
    rm -f /opt/trinity/share/apps/konqueror/servicemenus/tdeshare.desktop \
          /usr/share/apps/konqueror/servicemenus/tdeshare.desktop \
          /opt/trinity/share/apps/d3lphin/servicemenus/tdeshare.desktop \
          /opt/trinity/share/apps/dolphin/servicemenus/tdeshare.desktop \
          /usr/share/kservices5/ServiceMenus/tdeshare.desktop

    # 2. Clean Krusader useractions
    if [ -x /usr/share/tdeshare/scripts/krusader_action.py ] && command -v python3 >/dev/null 2>&1; then
        python3 /usr/share/tdeshare/scripts/krusader_action.py --uninstall || true
    fi
fi

exit 0
EOF
chmod 0755 "$PKGROOT/DEBIAN/prerm"

# postrm: refresh caches on package removal/purge
cat > "$PKGROOT/DEBIAN/postrm" <<'EOF'
#!/bin/sh
set -e

if [ -x /opt/trinity/bin/tdebuildsycoca ]; then
    /opt/trinity/bin/tdebuildsycoca --checkstamps >/dev/null 2>&1 || true
fi
if command -v kbuildsycoca5 >/dev/null 2>&1; then
    kbuildsycoca5 --checkstamps >/dev/null 2>&1 || true
fi
if command -v update-desktop-database >/dev/null 2>&1; then
    update-desktop-database -q /usr/share/applications >/dev/null 2>&1 || true
fi
if command -v gtk-update-icon-cache >/dev/null 2>&1; then
    gtk-update-icon-cache -f -t /usr/share/icons/hicolor >/dev/null 2>&1 || true
fi

exit 0
EOF
chmod 0755 "$PKGROOT/DEBIAN/postrm"

OUT_DEB="$SRC_ROOT/${PKG_NAME}_${PKG_VERSION}_${ARCH}.deb"
rm -f -- "$OUT_DEB"

dpkg-deb --build "$PKGROOT" "$OUT_DEB" >/dev/null

echo "=================================================="
echo " Debian package successfully built: $OUT_DEB"
echo " Size: $(ls -lh "$OUT_DEB" | awk '{print $5}')"
echo "=================================================="
exit 0
