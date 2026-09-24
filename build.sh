#!/usr/bin/env bash
set -euo pipefail

SRC_ROOT="$(cd "$(dirname "$0")" && pwd)"
BUILD_DIR="$SRC_ROOT/build"

need_cmd() {
    if ! command -v "$1" >/dev/null 2>&1; then
        echo "Missing required tool: $1" 1>&2
        return 1
    fi
    return 0
}

need_cmd cmake
need_cmd pkg-config

export PATH="/opt/trinity/bin:$PATH"
need_cmd tqmoc

mkdir -p -- "$BUILD_DIR"

echo "=== Configuring CMake in Release mode (Aggressive flags + LTO) ==="
cmake -S "$SRC_ROOT" -B "$BUILD_DIR" -DCMAKE_BUILD_TYPE=Release
cmake --build "$BUILD_DIR" -j"$(nproc)"

BIN_PATH="$BUILD_DIR/tdeshare"
if test -x "$BIN_PATH"; then
    if command -v sstrip >/dev/null 2>&1; then
        echo "info: stripping binary with sstrip..."
        sstrip "$BIN_PATH" >/dev/null 2>&1 || true
    else
        echo "info: sstrip not found, using standard strip..."
        strip --strip-all "$BIN_PATH" >/dev/null 2>&1 || true
    fi
    SIZE=$(stat -c%s "$BIN_PATH")
    echo "=== SUCCESS: $BIN_PATH ($SIZE bytes / $(numfmt --to=iec "$SIZE" 2>/dev/null || echo "${SIZE}B")) ==="
fi
