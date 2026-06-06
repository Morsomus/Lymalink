#!/usr/bin/env bash
#########################################################
# File: build.sh
# Date: 2026-05-22
# Author: Morsomus
# Copyright: see /LICENSE
# Description: Automated build and deployment script for Lymalinkd.
# Usage:
#   ./build.sh clean        - Clean build directory
#   ./build.sh debug        - Debug build
#   ./build.sh release      - Release build
#   ./build.sh test         - Clean + Debug build + Test
#   ./build.sh test --silent - Clean + Debug build + Test with failures only
#   ./build.sh deploy       - Clean + Release + strip + install service
#   ./build.sh deploy --debug - Clean + Debug + install service
#   ./build.sh start        - Start lymalinkd systemd user service
#   ./build.sh stop         - Stop lymalinkd systemd user service
#   ./build.sh restart      - Restart lymalinkd systemd user service
#   ./build.sh status       - Show lymalinkd service status
#   ./build.sh logs         - Follow lymalinkd journal output
#   ./build.sh uninstall    - Remove service, binary, bundled sounds and test icon
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

resolve_xdg_data_home() {
    local login_home="$1"

    if [ -n "${XDG_DATA_HOME:-}" ] && ! path_inside_snap_home "$XDG_DATA_HOME" "$login_home"; then
        printf '%s\n' "$XDG_DATA_HOME"
    else
        printf '%s\n' "$login_home/.local/share"
    fi
}

resolve_xdg_config_home() {
    local login_home="$1"

    if [ -n "${XDG_CONFIG_HOME:-}" ] && ! path_inside_snap_home "$XDG_CONFIG_HOME" "$login_home"; then
        printf '%s\n' "$XDG_CONFIG_HOME"
    else
        printf '%s\n' "$login_home/.config"
    fi
}

LOGIN_HOME="$(resolve_login_home)"
DATA_HOME="$(resolve_xdg_data_home "$LOGIN_HOME")"
CONFIG_HOME="$(resolve_xdg_config_home "$LOGIN_HOME")"

# When enabled, build output is diverted to /tmp (RAMfs)
BUILD_TO_TMP=0

SERVICE_NAME="lymalinkd"

NLOHMANN_VERSION="3.12.0"
NLOHMANN_DIR="$SCRIPT_DIR/src/nlohmann"
INSTALL_BIN_DIR="$LOGIN_HOME/.local/bin"
INSTALL_SERVICE_DIR="$CONFIG_HOME/systemd/user"
SERVICE_FILE="$INSTALL_SERVICE_DIR/${SERVICE_NAME}.service"
INSTALL_SOUND_DIR="$DATA_HOME/Lymalink/sounds"
INSTALL_DATA_DIR="$DATA_HOME/Lymalink"
INSTALL_TEST_ICON_PATH="$INSTALL_DATA_DIR/64x64-lymalink-test-icon.png"
TEST_ICON_SOURCE="$SCRIPT_DIR/../frontend/res/img/64x64-lymalink-test-icon.png"
_get_build_root() {
    if [ "$BUILD_TO_TMP" -eq 1 ] 2>/dev/null; then
        echo "/tmp/lymalinkd-build"
    else
        echo "$SCRIPT_DIR/build"
    fi
}

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

_install_binary() {
    local MODE_LOWER="$1"
    local BUILD_ROOT
    BUILD_ROOT="$(_get_build_root)"
    local BINARY_PATH="$BUILD_ROOT/$MODE_LOWER/bin/$SERVICE_NAME"

    if [ "$MODE_LOWER" = "release" ]; then
        echo "==> Stripping binary..."
        strip "$BINARY_PATH"
    fi

    echo "==> Installing binary to $INSTALL_BIN_DIR/$SERVICE_NAME..."
    mkdir -p "$INSTALL_BIN_DIR"
    cp "$BINARY_PATH" "$INSTALL_BIN_DIR/$SERVICE_NAME"
    chmod 755 "$INSTALL_BIN_DIR/$SERVICE_NAME"
}

clean() {
    local BUILD_ROOT
    BUILD_ROOT="$(_get_build_root)"
    echo "==> Cleaning build directories..."
    make -C "$SCRIPT_DIR" BUILD_ROOT="$BUILD_ROOT" clean
    echo "==> Clean done."
}

##############################################################################

_require_command() {
    if ! command -v "$1" >/dev/null 2>&1; then
        echo "==> ERROR: Required command not found: $1"
        exit 1
    fi
}

##############################################################################

_install_nlohmann() {
    if [ -f "$NLOHMANN_DIR/json.hpp" ]; then
        echo "==> nlohmann/json found at $NLOHMANN_DIR"
        return 0
    fi

    echo "==> nlohmann/json not found. Downloading version $NLOHMANN_VERSION..."

    local HEADER_URL="https://github.com/nlohmann/json/releases/download/v${NLOHMANN_VERSION}/json.hpp"

    _require_command wget

    mkdir -p "$NLOHMANN_DIR"

    echo "==> Downloading nlohmann/json $NLOHMANN_VERSION..."
    wget -q --show-progress -O "$NLOHMANN_DIR/json.hpp" "$HEADER_URL"

    echo "==> nlohmann/json successfully installed to $NLOHMANN_DIR"
}

##############################################################################

_make_jobs() {
    local jobs=""

    if command -v nproc >/dev/null 2>&1; then
        jobs="$(nproc)"
    elif command -v getconf >/dev/null 2>&1; then
        jobs="$(getconf _NPROCESSORS_ONLN 2>/dev/null || true)"
    fi

    case "$jobs" in
        ''|*[!0-9]*)
            jobs=1
            ;;
    esac

    if [ "$jobs" -lt 1 ]; then
        jobs=1
    fi

    printf '%s\n' "$jobs"
}

##############################################################################

build() {
    local MODE="$1"
    local MODE_LOWER
    MODE_LOWER="$(echo "$MODE" | tr '[:upper:]' '[:lower:]')"
    local BUILD_ROOT
    BUILD_ROOT="$(_get_build_root)"
    local BUILD_DIR="$BUILD_ROOT/${MODE_LOWER}"
    local MAKE_JOBS
    MAKE_JOBS="$(_make_jobs)"

    echo "==> Building backend ($MODE_LOWER)..."
    _install_nlohmann
    make -j"$MAKE_JOBS" -C "$SCRIPT_DIR" BUILD="$MODE_LOWER" BUILD_ROOT="$BUILD_ROOT"

    local BINARY_PATH="$BUILD_DIR/bin/$SERVICE_NAME"
    if [ -f "$BINARY_PATH" ] && [ -x "$BINARY_PATH" ]; then
        echo "==> Done: $BINARY_PATH (executable)"
    else
        echo "==> ERROR: Expected binary not found at $BINARY_PATH"
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

    echo "==> Starting deploy..."
    clean
    build "$MODE"
    _install_binary "$MODE_LOWER"
    _install_sounds
    _install_test_icon
    _install_service

    if _service_is_active; then
        echo "==> Service is running, restarting..."
        systemctl --user restart "$SERVICE_NAME"
    else
        echo "==> Enabling and starting service..."
        # systemctl --user enable "$SERVICE_NAME"
        systemctl --user start "$SERVICE_NAME"
    fi

    echo ""
    systemctl --user status "$SERVICE_NAME" --no-pager || true
    echo ""
    echo "==> Deploy done."
    echo "    Binary:       $INSTALL_BIN_DIR/$SERVICE_NAME"
    echo "    Sounds:       $INSTALL_SOUND_DIR"
    echo "    Test icon:    $INSTALL_TEST_ICON_PATH"
    echo "    Service:      $SERVICE_FILE"
    echo "    Logs:         journalctl --user -u $SERVICE_NAME -f"
}

##############################################################################

_install_sounds() {
    echo "==> Installing achievement sounds to $INSTALL_SOUND_DIR..."
    rm -rf "$INSTALL_SOUND_DIR"
    mkdir -p "$INSTALL_SOUND_DIR"
    rm -f "$INSTALL_DATA_DIR/.backend-sounds" "$INSTALL_DATA_DIR/.installer-sounds"

    if compgen -G "$SCRIPT_DIR/res/*.ogg" > /dev/null; then
        cp "$SCRIPT_DIR"/res/*.ogg "$INSTALL_SOUND_DIR/"
        chmod 644 "$INSTALL_SOUND_DIR"/*.ogg
    else
        echo "==> No achievement sounds found under $SCRIPT_DIR/res"
    fi
}

##############################################################################

_install_test_icon() {
    echo "==> Installing test icon to $INSTALL_TEST_ICON_PATH..."
    mkdir -p "$INSTALL_DATA_DIR"

    if [ -f "$TEST_ICON_SOURCE" ]; then
        cp "$TEST_ICON_SOURCE" "$INSTALL_TEST_ICON_PATH"
        chmod 644 "$INSTALL_TEST_ICON_PATH"
    else
        echo "==> WARNING: Test icon not found at $TEST_ICON_SOURCE"
    fi
}

##############################################################################

_install_service() {
    echo "==> Installing systemd user service..."
    mkdir -p "$INSTALL_SERVICE_DIR"

    # Write the service file
    cat > "$SERVICE_FILE" <<EOF
[Unit]
Description=Lymalink Backend Daemon
After=default.target

[Service]
ExecStart=${INSTALL_BIN_DIR}/${SERVICE_NAME}
Restart=on-failure
RestartSec=5
Type=notify
StandardOutput=journal
StandardError=journal

[Install]
WantedBy=default.target
EOF

    echo "==> Service file written to $SERVICE_FILE"
    systemctl --user daemon-reload
    echo "==> systemd user daemon reloaded."
}

##############################################################################

uninstall_service() {
    echo "==> Uninstalling $SERVICE_NAME..."

    if _service_is_active; then
        echo "==> Stopping service..."
        systemctl --user stop "$SERVICE_NAME"
    else
        echo "==> Service not running."
    fi

    if _service_is_enabled; then
        echo "==> Disabling service..."
        systemctl --user disable "$SERVICE_NAME"
    else
        echo "==> Service not enabled."
    fi

    if [ -f "$SERVICE_FILE" ]; then
        echo "==> Removing service file: $SERVICE_FILE"
        rm -f "$SERVICE_FILE"
    else
        echo "==> Service file not found: $SERVICE_FILE"
    fi

    echo "==> Reloading systemd user daemon..."
    systemctl --user daemon-reload

    if [ -f "$INSTALL_BIN_DIR/$SERVICE_NAME" ]; then
        echo "==> Removing binary: $INSTALL_BIN_DIR/$SERVICE_NAME"
        rm -f "$INSTALL_BIN_DIR/$SERVICE_NAME"
    else
        echo "==> Binary not found: $INSTALL_BIN_DIR/$SERVICE_NAME"
    fi

    echo "==> Removing sounds: $INSTALL_SOUND_DIR"
    rm -rf "$INSTALL_SOUND_DIR"
    rm -f "$INSTALL_DATA_DIR/.backend-sounds" "$INSTALL_DATA_DIR/.installer-sounds"

    if [ -f "$INSTALL_TEST_ICON_PATH" ]; then
        echo "==> Removing test icon: $INSTALL_TEST_ICON_PATH"
        rm -f "$INSTALL_TEST_ICON_PATH"
    else
        echo "==> Test icon not found: $INSTALL_TEST_ICON_PATH"
    fi

    rmdir "$INSTALL_SOUND_DIR" "$INSTALL_DATA_DIR" >/dev/null 2>&1 || true

    echo "==> Uninstall done."
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

    local BUILD_ROOT
    BUILD_ROOT="$(_get_build_root)"
    local MAKE_JOBS
    MAKE_JOBS="$(_make_jobs)"

    clean
    echo "==> Building tests (debug)..."
    make -j"$MAKE_JOBS" -C "$SCRIPT_DIR" BUILD="debug" BUILD_ROOT="$BUILD_ROOT" "$BUILD_ROOT/debug/tests/SQLiteManagerTests"

    echo "==> Running tests..."
    if [ "$TEST_OUTPUT_MODE" = "--silent" ]; then
        "$BUILD_ROOT/debug/tests/SQLiteManagerTests"
    else
        "$BUILD_ROOT/debug/tests/SQLiteManagerTests" --success
    fi
}

##############################################################################

_service_is_active() {
    systemctl --user is-active --quiet "$SERVICE_NAME" 2>/dev/null
}

##############################################################################

_service_is_enabled() {
    systemctl --user is-enabled --quiet "$SERVICE_NAME" 2>/dev/null
}

##############################################################################

svc_start() {
    echo "==> Starting $SERVICE_NAME..."
    systemctl --user start "$SERVICE_NAME"
    systemctl --user status "$SERVICE_NAME" --no-pager || true
}

svc_stop() {
    echo "==> Stopping $SERVICE_NAME..."
    systemctl --user stop "$SERVICE_NAME"
}

svc_restart() {
    echo "==> Restarting $SERVICE_NAME..."
    systemctl --user restart "$SERVICE_NAME"
    systemctl --user status "$SERVICE_NAME" --no-pager || true
}

svc_status() {
    systemctl --user status "$SERVICE_NAME" --no-pager || true
}

svc_logs() {
    echo "==> Following logs for $SERVICE_NAME (Ctrl+C to exit)..."
    journalctl --user -u "$SERVICE_NAME" -f
}

##############################################################################
case "${1:-}" in
    clean)     _require_no_extra_args clean "${@:2}"; clean ;;
    debug)     _require_no_extra_args debug "${@:2}"; build Debug ;;
    release)   _require_no_extra_args release "${@:2}"; build Release ;;
    test)      run_tests "${@:2}" ;;
    deploy)    deploy "${@:2}" ;;
    start)     _require_no_extra_args start "${@:2}"; svc_start ;;
    stop)      _require_no_extra_args stop "${@:2}"; svc_stop ;;
    restart)   _require_no_extra_args restart "${@:2}"; svc_restart ;;
    status)    _require_no_extra_args status "${@:2}"; svc_status ;;
    logs)      _require_no_extra_args logs "${@:2}"; svc_logs ;;
    uninstall) _require_no_extra_args uninstall "${@:2}"; uninstall_service ;;
    *)
        echo "Usage: $0 [command]"
        echo ""
        echo "  Build commands:"
        echo "    clean          Remove build directory"
        echo "    debug          Debug build"
        echo "    release        Release build"
        echo "    test           Clean + debug build + test (full verbosity)"
        echo "    test --silent  Clean + debug build + test (failures only)"
        echo "    deploy         Clean + Release + strip + install + (re)start service"
        echo "    deploy --debug Clean + Debug + install + (re)start service"
        echo ""
        echo "  Service commands:"
        echo "    start          Start lymalinkd service"
        echo "    stop           Stop lymalinkd service"
        echo "    restart        Restart lymalinkd service"
        echo "    status         Show service status"
        echo "    logs           Follow service journal output"
        echo "    uninstall      Remove service, binary, bundled sounds and test icon"
        exit 1
        ;;
esac
