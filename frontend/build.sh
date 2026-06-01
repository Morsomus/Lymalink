#!/usr/bin/env bash
#########################################################
# File: build.sh
# Date: 2026-04-30
# Author: Morsomus
# Copyright: see /LICENSE
# Description: Automated build and deployment script for Lymalink.
# Usage:
#   ./build.sh debug        - Debug build
#   ./build.sh release      - Release build
#   ./build.sh clean        - Clean build directory
#   ./build.sh deploy       - Clean + Release build + deploy
#   ./build.sh deploy --debug - Clean + Debug build + deploy
#   ./build.sh uninstall    - Remove deployed binary, desktop entry, icon
#   ./build.sh dev          - Clean + Debug build + Execute
#   ./build.sh test         - Clean + Debug build + Test
#   ./build.sh test --silent - Clean + Debug build + Test with failures only
#########################################################

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

DEPLOY_BIN_DIR="${XDG_BIN_HOME:-$HOME/.local/bin}"
DEPLOY_DATA_DIR="${XDG_DATA_HOME:-$HOME/.local/share}"
ICON_DIR="$DEPLOY_DATA_DIR/icons/hicolor/256x256/apps"
DESKTOP_FILE="$DEPLOY_DATA_DIR/applications/lymalink.desktop"

APP_NAME="lymalink"
ICON_NAME="$APP_NAME"

# When enabled, build output is diverted to /tmp (RAMfs)
BUILD_TO_TMP=0

##############################################################################

_require_no_extra_args() {
    local COMMAND="$1"
    shift

    if [ "$#" -gt 0 ]; then
        echo "==> ERROR: Too many options for $COMMAND."
        echo "==> Usage: $0 $COMMAND"
        exit 1
    fi
}

##############################################################################

clean() {
    echo "==> Cleaning build directories..."

    if [ "$BUILD_TO_TMP" -eq 1 ] 2>/dev/null; then
        local BUILD_DIR="/tmp/lymalink-build"
    else
        local BUILD_DIR="$SCRIPT_DIR/build"
    fi

    rm -rf "$BUILD_DIR"
    echo "==> Clean done."
}

##############################################################################

build() {
    local MODE="$1"
    local MODE_LOWER
    MODE_LOWER="$(echo "$MODE" | tr '[:upper:]' '[:lower:]')"

    if [ "$BUILD_TO_TMP" -eq 1 ] 2>/dev/null; then
        local BUILD_DIR="/tmp/lymalink-build/${MODE_LOWER}"
    else
        local BUILD_DIR="$SCRIPT_DIR/build/${MODE_LOWER}"
    fi

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
    local DEPLOY_OPTION="${1:-}"
    local MODE="Release"
    local MODE_LOWER="release"

    if [ "$#" -gt 1 ]; then
        echo "==> ERROR: Too many deploy options."
        echo "==> Usage: $0 deploy [--debug]"
        exit 1
    fi

    if [ "$DEPLOY_OPTION" = "--debug" ]; then
        MODE="Debug"
        MODE_LOWER="debug"
    elif [ -n "$DEPLOY_OPTION" ]; then
        echo "==> ERROR: Unknown deploy option: $DEPLOY_OPTION"
        echo "==> Usage: $0 deploy [--debug]"
        exit 1
    fi

    clean
    build "$MODE"

    if [ "$BUILD_TO_TMP" -eq 1 ] 2>/dev/null; then
        local BUILD_DIR="/tmp/lymalink-build/${MODE_LOWER}"
    else
        local BUILD_DIR="$SCRIPT_DIR/build/${MODE_LOWER}"
    fi

    local BINARY_PATH="$BUILD_DIR/bin/Lymalink"

    # Copy and strip release binary
    mkdir -p "$DEPLOY_BIN_DIR"
    cp "$BINARY_PATH" "$DEPLOY_BIN_DIR/Lymalink"
    if [ "$MODE" = "Release" ]; then
        strip "$DEPLOY_BIN_DIR/Lymalink"
    fi

    # Copy icon
    mkdir -p "$ICON_DIR"
    cp "$SCRIPT_DIR/res/img/BlankBackground_MFC_00002_E_256x256.png" "$ICON_DIR/${ICON_NAME}.png"
    if command -v gtk-update-icon-cache &>/dev/null; then
        gtk-update-icon-cache -f -t "$DEPLOY_DATA_DIR/icons/hicolor" 2>/dev/null || true
    fi

    # Desktop entry -> ~/.local/share/applications/
    mkdir -p "$(dirname "$DESKTOP_FILE")"
    cat > "$DESKTOP_FILE" <<EOF
[Desktop Entry]
Name=Lymalink
Exec=$DEPLOY_BIN_DIR/Lymalink
Icon=$ICON_NAME
StartupWMClass=Lymalink
Type=Application
Terminal=false
Categories=Game;Utility;
EOF

    update-desktop-database "$(dirname "$DESKTOP_FILE")" 2>/dev/null || true

    echo "==> Binary:          $DEPLOY_BIN_DIR/Lymalink"
    echo "==> Icon:            $ICON_DIR/${ICON_NAME}.png"
    echo "==> Desktop entry:   $DESKTOP_FILE"
}

##############################################################################

uninstall() {
    echo "==> Uninstalling $APP_NAME..."

    if [ -f "$DEPLOY_BIN_DIR/Lymalink" ]; then
        echo "==> Removing binary: $DEPLOY_BIN_DIR/Lymalink"
        rm -f "$DEPLOY_BIN_DIR/Lymalink"
    else
        echo "==> Binary not found: $DEPLOY_BIN_DIR/Lymalink"
    fi

    if [ -f "$DESKTOP_FILE" ]; then
        echo "==> Removing desktop entry: $DESKTOP_FILE"
        rm -f "$DESKTOP_FILE"
    else
        echo "==> Desktop entry not found: $DESKTOP_FILE"
    fi

    if [ -f "$ICON_DIR/${ICON_NAME}.png" ]; then
        echo "==> Removing icon: $ICON_DIR/${ICON_NAME}.png"
        rm -f "$ICON_DIR/${ICON_NAME}.png"
    else
        echo "==> Icon not found: $ICON_DIR/${ICON_NAME}.png"
    fi

    if command -v gtk-update-icon-cache &>/dev/null; then
        gtk-update-icon-cache -f -t "$DEPLOY_DATA_DIR/icons/hicolor" 2>/dev/null || true
    fi
    update-desktop-database "$(dirname "$DESKTOP_FILE")" 2>/dev/null || true

    echo "==> Uninstall done."
}

##############################################################################

dev() {
    clean
    build Debug

    if [ "$BUILD_TO_TMP" -eq 1 ] 2>/dev/null; then
        local BINARY_PATH="/tmp/lymalink-build/debug/bin/Lymalink"
    else
        local BINARY_PATH="$SCRIPT_DIR/build/debug/bin/Lymalink"
    fi

    echo "==> Launching..."
    exec "$BINARY_PATH"
}

##############################################################################

run_tests() {
    local TEST_OUTPUT_MODE="${1:-}"

    if [ "$#" -gt 1 ]; then
        echo "==> ERROR: Too many test options."
        echo "==> Usage: $0 test [--silent]"
        exit 1
    fi

    if [ -n "$TEST_OUTPUT_MODE" ] && [ "$TEST_OUTPUT_MODE" != "--silent" ]; then
        echo "==> ERROR: Unknown test option: $TEST_OUTPUT_MODE"
        echo "==> Usage: $0 test [--silent]"
        exit 1
    fi

    clean
    build Debug

    if [ "$BUILD_TO_TMP" -eq 1 ] 2>/dev/null; then
        local BUILD_DIR="/tmp/lymalink-build/debug"
    else
        local BUILD_DIR="$SCRIPT_DIR/build/debug"
    fi

    echo "==> Running tests..."
    if [ "$TEST_OUTPUT_MODE" = "--silent" ]; then
        ctest --test-dir "$BUILD_DIR" --output-on-failure
    else
        ctest --test-dir "$BUILD_DIR" --verbose
    fi
}

##############################################################################

case "${1:-}" in
    clean)   _require_no_extra_args clean "${@:2}"; clean ;;
    debug)   _require_no_extra_args debug "${@:2}"; build Debug ;;
    release) _require_no_extra_args release "${@:2}"; build Release ;;
    deploy)  deploy "${@:2}" ;;
    uninstall) _require_no_extra_args uninstall "${@:2}"; uninstall ;;
    dev)     _require_no_extra_args dev "${@:2}"; dev ;;
    test)    run_tests "${@:2}" ;;
    *)
        echo "Usage: $0 [clean|debug|release|deploy|uninstall|dev|test]"
        echo ""
        echo "  clean          - Remove build/"
        echo "  debug          - Debug build   -> build/debug/"
        echo "  release        - Release build -> build/release/"
        echo "  deploy         - clean + release build + strip"
        echo "  deploy --debug - clean + debug build without strip"
        echo "                   + binary    -> $DEPLOY_BIN_DIR/"
        echo "                   + icon      -> $ICON_DIR/"
        echo "                   + desktop   -> $DESKTOP_FILE"
        echo "  uninstall      - remove deployed binary, icon and desktop entry"
        echo "  dev            - clean + debug build + launch"
        echo "  test           - clean + debug build + test (full verbosity)"
        echo "  test --silent  - clean + debug build + test (failures only)"
        exit 1
        ;;
esac
