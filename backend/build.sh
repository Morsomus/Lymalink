#!/usr/bin/env bash
#########################################################
# File: build.sh
# Date: 22.05.2026
# Author: Morsomus
# Copyright: see /LICENSE
# Description: Automated build and deployment script for Lymalinkd.
# Usage:
#   ./build.sh clean        - Clean build directory
#   ./build.sh debug        - Debug build
#   ./build.sh release      - Release build
#   ./build.sh deploy       - Clean + Release + strip + install service
#   ./build.sh start        - Start lymalinkd systemd user service
#   ./build.sh stop         - Stop lymalinkd systemd user service
#   ./build.sh restart      - Restart lymalinkd systemd user service
#   ./build.sh status       - Show lymalinkd service status
#   ./build.sh logs         - Follow lymalinkd journal output
#   ./build.sh uninstall    - Stop + disable + remove user service and binary
#########################################################

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

# When enabled, build output is diverted to /tmp (RAMfs)
BUILD_TO_TMP=1

SERVICE_NAME="lymalinkd"
INSTALL_BIN_DIR="$HOME/.local/bin"
INSTALL_SERVICE_DIR="$HOME/.config/systemd/user"
SERVICE_FILE="$INSTALL_SERVICE_DIR/${SERVICE_NAME}.service"

##############################################################################

_get_build_root() {
    if [ "$BUILD_TO_TMP" -eq 1 ] 2>/dev/null; then
        echo "/tmp/lymalinkd-build"
    else
        echo "$SCRIPT_DIR/build"
    fi
}

##############################################################################

_install_binary() {
    local BUILD_ROOT
    BUILD_ROOT="$(_get_build_root)"
    local BINARY_PATH="$BUILD_ROOT/release/bin/$SERVICE_NAME"

    echo "==> Stripping binary..."
    strip "$BINARY_PATH"

    echo "==> Installing binary to $INSTALL_BIN_DIR/$SERVICE_NAME..."
    mkdir -p "$INSTALL_BIN_DIR"
    cp "$BINARY_PATH" "$INSTALL_BIN_DIR/$SERVICE_NAME"
    chmod 755 "$INSTALL_BIN_DIR/$SERVICE_NAME"
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

_service_is_active() {
    systemctl --user is-active --quiet "$SERVICE_NAME" 2>/dev/null
}

##############################################################################

_service_is_enabled() {
    systemctl --user is-enabled --quiet "$SERVICE_NAME" 2>/dev/null
}

##############################################################################

clean() {
    local BUILD_ROOT
    BUILD_ROOT="$(_get_build_root)"
    echo "==> Cleaning build directories..."
    make -C "$SCRIPT_DIR" BUILD_ROOT="$BUILD_ROOT" clean
    echo "==> Clean done."
}

##############################################################################

build() {
    local MODE="$1"
    local MODE_LOWER
    MODE_LOWER="$(echo "$MODE" | tr '[:upper:]' '[:lower:]')"
    local BUILD_ROOT
    BUILD_ROOT="$(_get_build_root)"
    local BUILD_DIR="$BUILD_ROOT/${MODE_LOWER}"

    echo "==> Building ($MODE_LOWER)..."
    make -C "$SCRIPT_DIR" BUILD="$MODE_LOWER" BUILD_ROOT="$BUILD_ROOT"

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
    echo "==> Starting deploy..."
    clean
    build Release
    _install_binary
    _install_service

    if _service_is_active; then
        echo "==> Service is running, restarting..."
        systemctl --user restart "$SERVICE_NAME"
    else
        echo "==> Enabling and starting service..."
        systemctl --user enable "$SERVICE_NAME"
        systemctl --user start "$SERVICE_NAME"
    fi

    echo ""
    systemctl --user status "$SERVICE_NAME" --no-pager || true
    echo ""
    echo "==> Deploy done."
    echo "    Binary:  $INSTALL_BIN_DIR/$SERVICE_NAME"
    echo "    Service: $SERVICE_FILE"
    echo "    Logs:    journalctl --user -u $SERVICE_NAME -f"
}

##############################################################################

uninstall() {
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

    echo "==> Uninstall done."
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
    clean)   clean ;;
    debug)   build Debug ;;
    release) build Release ;;
    deploy)  deploy ;;
    start)   svc_start ;;
    stop)    svc_stop ;;
    restart) svc_restart ;;
    status)  svc_status ;;
    logs)    svc_logs ;;
    uninstall) uninstall ;;
    *)
        echo "Usage: $0 [command]"
        echo ""
        echo "  Build commands:"
        echo "    clean          Remove build directory"
        echo "    debug          Debug build"
        echo "    release        Release build"
        echo "    deploy         Clean + Release + strip + install + (re)start service"
        echo ""
        echo "  Service commands:"
        echo "    start          Start lymalinkd service"
        echo "    stop           Stop lymalinkd service"
        echo "    restart        Restart lymalinkd service"
        echo "    status         Show service status"
        echo "    logs           Follow service journal output"
        echo "    uninstall      Stop + disable + remove lymalinkd service and binary"
        exit 1
        ;;
esac
