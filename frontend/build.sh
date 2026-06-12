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

resolve_login_home() {
    local login_home=""

    if command -v getent >/dev/null 2>&1 && [ -n "${USER:-}" ]; then
        login_home="$(getent passwd "$USER" | cut -d: -f6)"
    fi

    if [ -n "$login_home" ] && [ "$HOME" != "$login_home" ]; then
        case "$HOME" in
            "$login_home"/snap/*) printf '%s\n' "$login_home"; return ;;
        esac
    fi

    printf '%s\n' "$HOME"
}

path_inside_snap_home() {
    local path="$1"
    local login_home="$2"

    case "$path" in
        "$login_home"/snap/*) return 0 ;;
        *) return 1 ;;
    esac
}

LOGIN_HOME="$(resolve_login_home)"

if [ -n "${XDG_BIN_HOME:-}" ] && ! path_inside_snap_home "$XDG_BIN_HOME" "$LOGIN_HOME"; then
    DEPLOY_BIN_DIR="$XDG_BIN_HOME"
else
    DEPLOY_BIN_DIR="$LOGIN_HOME/.local/bin"
fi

if [ -n "${XDG_DATA_HOME:-}" ] && ! path_inside_snap_home "$XDG_DATA_HOME" "$LOGIN_HOME"; then
    DEPLOY_DATA_DIR="$XDG_DATA_HOME"
else
    DEPLOY_DATA_DIR="$LOGIN_HOME/.local/share"
fi
ICON_DIR="$DEPLOY_DATA_DIR/icons/hicolor/256x256/apps"
DESKTOP_FILE="$DEPLOY_DATA_DIR/applications/lymalink.desktop"

APP_NAME="lymalink"
ICON_NAME="$APP_NAME"
MIN_QT_VERSION="6.8.0"

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

check_qt_version() {
    local qmake_path="$1"
    local qt_version
    local qt_major qt_minor qt_patch
    local min_major min_minor min_patch

    qt_version="$("$qmake_path" -query QT_VERSION)"
    IFS=. read -r qt_major qt_minor qt_patch <<< "$qt_version"
    IFS=. read -r min_major min_minor min_patch <<< "$MIN_QT_VERSION"
    qt_patch="${qt_patch:-0}"
    min_patch="${min_patch:-0}"

    if (( qt_major > min_major ||
        (qt_major == min_major && qt_minor > min_minor) ||
        (qt_major == min_major && qt_minor == min_minor && qt_patch >= min_patch) )); then
        return
    fi

    echo "==> ERROR: Qt $qt_version is too old for the frontend QML runtime." >&2
    echo "==> Build the frontend with Qt >= $MIN_QT_VERSION." >&2
    exit 1
}

##############################################################################

resolve_qmake() {
    local candidate

    for candidate in qmake6 qmake; do
        if command -v "$candidate" >/dev/null 2>&1 && "$candidate" -query QT_INSTALL_PREFIX >/dev/null 2>&1; then
            command -v "$candidate"
            return 0
        fi
    done

    echo "==> ERROR: Could not find working Qt qmake." >&2
    echo "==> Install Qt6 qmake tools, e.g. qt6-base-dev-tools, and verify: qmake6 -v" >&2
    exit 1
}

##############################################################################

validate_qt_version() {
    local qmake_path

    qmake_path="$(resolve_qmake)"
    check_qt_version "$qmake_path"
}

##############################################################################

terminate_process_by_name() {
    local process_name="$1"
    local pid_list=()
    local pid

    if command -v pgrep >/dev/null 2>&1; then
        while IFS= read -r pid; do
            pid_list+=("$pid")
        done < <(pgrep -x "$process_name" 2>/dev/null || true)
    elif command -v ps >/dev/null 2>&1; then
        while IFS= read -r pid; do
            pid_list+=("$pid")
        done < <(ps -eo pid=,comm= | awk -v name="$process_name" '$2 == name { print $1 }')
    fi

    if [ "${#pid_list[@]}" -eq 0 ]; then
        return
    fi

    echo "==> Stopping running $process_name processes: ${pid_list[*]}"
    kill -TERM "${pid_list[@]}" >/dev/null 2>&1 || true

    if command -v pgrep >/dev/null 2>&1; then
        for _ in {1..10}; do
            if ! pgrep -x "$process_name" >/dev/null 2>&1; then
                return
            fi
            sleep 1
        done

        echo "==> Forcing $process_name shutdown."
        pkill -KILL -x "$process_name" >/dev/null 2>&1 || true
        return
    fi

    sleep 1
    pid_list=()
    if command -v ps >/dev/null 2>&1; then
        while IFS= read -r pid; do
            pid_list+=("$pid")
        done < <(ps -eo pid=,comm= | awk -v name="$process_name" '$2 == name { print $1 }')
    fi

    if [ "${#pid_list[@]}" -ne 0 ]; then
        echo "==> Forcing $process_name shutdown."
        kill -KILL "${pid_list[@]}" >/dev/null 2>&1 || true
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

    validate_qt_version

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

    terminate_process_by_name "Lymalink"

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
