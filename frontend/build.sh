#!/usr/bin/env bash
#########################################################
# File: build.sh
# Date: 30.04.2026
# Author: Morsomus
# Copyright: see /LICENSE
# Description: Automated build and deployment script for Lymalink.
# Usage:
#   ./build.sh debug        - Debug build
#   ./build.sh release      - Release build
#   ./build.sh deploy       - Release build + deploy to DEPLOY_DIR
#########################################################


set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
DEPLOY_DIR="$HOME/Apps/Lymalink"
DESKTOP_FILE="$HOME/.local/share/applications/lymalink.desktop"

##############################################################################

clean() {
    echo "==> Cleaning build directories..."
    rm -rf "$SCRIPT_DIR/build"
    echo "==> Clean done."
}

##############################################################################

build() {
    local MODE="$1"
    local BUILD_DIR="$SCRIPT_DIR/build/$(echo "$MODE" | tr '[:upper:]' '[:lower:]')"

    echo "==> Configuring ($MODE)..."
    mkdir -p "$BUILD_DIR"
    cmake -S "$SCRIPT_DIR" -B "$BUILD_DIR" -DCMAKE_BUILD_TYPE="$MODE"

    echo "==> Building ($MODE)..."
    cmake --build "$BUILD_DIR" --parallel "$(nproc)"

    local BINARY_PATH="$BUILD_DIR/bin/Lymalink"
    if [ -f "$BINARY_PATH" ] && [ -x "$BINARY_PATH" ]; then
        echo "==> Done: $BINARY_PATH (executable)"
    else
        echo "==> WARNING: Expected binary not found at $BINARY_PATH. Check CMakeLists.txt target."
        ls -1 "$BUILD_DIR" | grep -iE '(lymalink|lib)' || true
        exit 1
    fi
}

##############################################################################

deploy() {
    clean
    build Release

    local BUILD_DIR="$SCRIPT_DIR/build/release"
    local BINARY_PATH="$BUILD_DIR/bin/Lymalink"

    echo "==> Deploying to $DEPLOY_DIR..."
    mkdir -p "$DEPLOY_DIR"

    # Copy and strip binary
    cp "$BINARY_PATH" "$DEPLOY_DIR/Lymalink"
    strip "$DEPLOY_DIR/Lymalink"

    # Copy icon
    cp "$SCRIPT_DIR/res/img/BlankBackground_MFC_00002_E.png" "$DEPLOY_DIR/BlankBackground_MFC_00002_E.png"

    # Create desktop entry
    mkdir -p "$(dirname "$DESKTOP_FILE")"
    cat > "$DESKTOP_FILE" <<EOF
[Desktop Entry]
Name=Lymalink
Exec=$DEPLOY_DIR/Lymalink
Icon=$DEPLOY_DIR/BlankBackground_MFC_00002_E.png
Type=Application
Terminal=false
Categories=Game;Utility;
EOF

    update-desktop-database "$DESKTOP_FILE" 2>/dev/null || true

    echo "==> Deployed to:     $DEPLOY_DIR"
    echo "==> Desktop entry:   $DESKTOP_FILE"
}

##############################################################################

dev() {
    clean
    build Debug

    local BUILD_DIR="$SCRIPT_DIR/build/debug"
    local BINARY_PATH="$BUILD_DIR/bin/Lymalink"

    echo "==> Launching..."
    exec "$BINARY_PATH"
}

##############################################################################

case "${1:-}" in
    clean)   clean ;;
    debug)   build Debug ;;
    release) build Release ;;
    deploy)  deploy ;;
    dev)     dev ;;
    *)
        echo "Usage: $0 [clean|debug|release|deploy]"
        echo ""
        echo "  clean    - Remove build/"
        echo "  debug    - Debug build   -> build/debug/"
        echo "  release  - Release build -> build/release/"
        echo "  deploy   - clean + release build + strip + copy to $DEPLOY_DIR"
        echo "  dev      - clean + debug build + launch"
        echo "             + create desktop entry at $DESKTOP_FILE"
        exit 1
        ;;
esac
