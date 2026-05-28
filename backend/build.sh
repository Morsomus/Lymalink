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
#   ./build.sh deploy       - Clean + Release + strip + install service + overlay
#   ./build.sh start        - Start lymalinkd systemd user service
#   ./build.sh stop         - Stop lymalinkd systemd user service
#   ./build.sh restart      - Restart lymalinkd systemd user service
#   ./build.sh status       - Show lymalinkd service status
#   ./build.sh logs         - Follow lymalinkd journal output
#   ./build.sh uninstall    - Stop + disable + remove user service, binary and overlay
#########################################################

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
OVERLAY_DIR="$SCRIPT_DIR/../backend-overlay"

# When enabled, build output is diverted to /tmp (RAMfs)
BUILD_TO_TMP=1

SERVICE_NAME="lymalinkd"
OVERLAY_LIB="lymalink-overlay.so"
OVERLAY_OPENGL_LIB="lymalink-overlay-opengl.so"
OVERLAY_PRELOADER_LIB="lymalink-overlay-preloader.so"
OVERLAY_LAUNCHER="lymalink-overlay"
OVERLAY_MANIFEST="lymalink_overlay.json"
IMGUI_VERSION="1.92.8"
IMGUI_DIR="$OVERLAY_DIR/src/imgui"

INSTALL_BIN_DIR="$HOME/.local/bin"
INSTALL_LIB_DIR="$HOME/.local/lib"
INSTALL_SERVICE_DIR="$HOME/.config/systemd/user"
INSTALL_VULKAN_LAYER_DIR="${XDG_DATA_HOME:-$HOME/.local/share}/vulkan/implicit_layer.d"
SERVICE_FILE="$INSTALL_SERVICE_DIR/${SERVICE_NAME}.service"
INSTALL_SOUND_DIR="${XDG_DATA_HOME:-$HOME/.local/share}/Lymalink/sounds"
INSTALL_DATA_DIR="${XDG_DATA_HOME:-$HOME/.local/share}/Lymalink"
INSTALL_TEST_ICON_PATH="$INSTALL_DATA_DIR/64x64-lymalink-test-icon.png"
TEST_ICON_SOURCE="$SCRIPT_DIR/../frontend/res/img/64x64-lymalink-test-icon.png"
FLATPAK_EXTENSION_ID="org.freedesktop.Platform.VulkanLayer.lymalink"
FLATPAK_EXTENSION_BRANCH="25.08"
FLATPAK_EXTENSION_MANIFEST="lymalink_overlay.x86_64.json"
FLATPAK_TARGET_APPS=("com.heroicgameslauncher.hgl" "com.usebottles.bottles")

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

_install_imgui() {
    if [ -f "$IMGUI_DIR/imgui.h" ]; then
        echo "==> ImGui found at $IMGUI_DIR"
        return 0
    fi

    echo "==> ImGui not found. Downloading version $IMGUI_VERSION..."

    mkdir -p "$IMGUI_DIR"
    local ZIP_URL="https://github.com/ocornut/imgui/archive/refs/tags/v${IMGUI_VERSION}.zip"
    local ZIP_FILE="/tmp/imgui-${IMGUI_VERSION}.zip"

    if ! command -v wget &> /dev/null; then
        echo "==> Installing wget..."
        sudo dnf install -y wget
    fi

    echo "==> Downloading ImGui $IMGUI_VERSION..."
    wget -q --show-progress -O "$ZIP_FILE" "$ZIP_URL"

    echo "==> Extracting ImGui..."
    unzip -q "$ZIP_FILE" -d /tmp/
    mv /tmp/imgui-${IMGUI_VERSION}/* "$IMGUI_DIR/"
    rm -rf /tmp/imgui-${IMGUI_VERSION} "$ZIP_FILE"

    echo "==> ImGui successfully installed to $IMGUI_DIR"
}

##############################################################################

_install_overlay() {
    local BUILD_ROOT
    BUILD_ROOT="$(_get_build_root)"
    local OVERLAY_PATH="$BUILD_ROOT/release/bin/$OVERLAY_LIB"
    local OVERLAY_OPENGL_PATH="$BUILD_ROOT/release/bin/$OVERLAY_OPENGL_LIB"
    local OVERLAY_PRELOADER_PATH="$BUILD_ROOT/release/bin/$OVERLAY_PRELOADER_LIB"
    local OVERLAY_LAUNCHER_PATH="$OVERLAY_DIR/lymalink-overlay.sh"

    if [ ! -f "$OVERLAY_PATH" ]; then
        echo "==> WARNING: Overlay library not found at $OVERLAY_PATH, skipping."
        return
    fi
    if [ ! -f "$OVERLAY_OPENGL_PATH" ]; then
        echo "==> WARNING: OpenGL overlay library not found at $OVERLAY_OPENGL_PATH, skipping."
        return
    fi
    if [ ! -f "$OVERLAY_PRELOADER_PATH" ]; then
        echo "==> WARNING: OpenGL preloader library not found at $OVERLAY_PRELOADER_PATH, skipping."
        return
    fi
    if [ ! -f "$OVERLAY_LAUNCHER_PATH" ]; then
        echo "==> WARNING: Overlay launcher not found at $OVERLAY_LAUNCHER_PATH, skipping."
        return
    fi

    echo "==> Stripping overlay libraries..."
    strip "$OVERLAY_PATH"
    strip "$OVERLAY_OPENGL_PATH"
    strip "$OVERLAY_PRELOADER_PATH"

    echo "==> Installing overlay libraries to $INSTALL_LIB_DIR..."
    mkdir -p "$INSTALL_LIB_DIR"
    cp "$OVERLAY_PATH" "$INSTALL_LIB_DIR/$OVERLAY_LIB"
    cp "$OVERLAY_OPENGL_PATH" "$INSTALL_LIB_DIR/$OVERLAY_OPENGL_LIB"
    cp "$OVERLAY_PRELOADER_PATH" "$INSTALL_LIB_DIR/$OVERLAY_PRELOADER_LIB"
    chmod 755 "$INSTALL_LIB_DIR/$OVERLAY_LIB"
    chmod 755 "$INSTALL_LIB_DIR/$OVERLAY_OPENGL_LIB"
    chmod 755 "$INSTALL_LIB_DIR/$OVERLAY_PRELOADER_LIB"

    echo "==> Installing OpenGL launcher to $INSTALL_BIN_DIR/$OVERLAY_LAUNCHER..."
    mkdir -p "$INSTALL_BIN_DIR"
    cp "$OVERLAY_LAUNCHER_PATH" "$INSTALL_BIN_DIR/$OVERLAY_LAUNCHER"
    chmod 755 "$INSTALL_BIN_DIR/$OVERLAY_LAUNCHER"

    echo "==> Installing Vulkan layer manifest to $INSTALL_VULKAN_LAYER_DIR/$OVERLAY_MANIFEST..."
    mkdir -p "$INSTALL_VULKAN_LAYER_DIR"

    _write_vulkan_overlay_manifest \
        "$INSTALL_VULKAN_LAYER_DIR/$OVERLAY_MANIFEST" \
        "${INSTALL_LIB_DIR}/${OVERLAY_LIB}"

    echo "==> Vulkan layer manifest written."
}

##############################################################################

_uninstall_overlay() {
    local LIB
    for LIB in "$OVERLAY_LIB" "$OVERLAY_OPENGL_LIB" "$OVERLAY_PRELOADER_LIB"; do
        if [ -f "$INSTALL_LIB_DIR/$LIB" ]; then
            echo "==> Removing overlay library: $INSTALL_LIB_DIR/$LIB"
            rm -f "$INSTALL_LIB_DIR/$LIB"
        else
            echo "==> Overlay library not found: $INSTALL_LIB_DIR/$LIB"
        fi
    done

    if [ -f "$INSTALL_BIN_DIR/$OVERLAY_LAUNCHER" ]; then
        echo "==> Removing OpenGL launcher: $INSTALL_BIN_DIR/$OVERLAY_LAUNCHER"
        rm -f "$INSTALL_BIN_DIR/$OVERLAY_LAUNCHER"
    else
        echo "==> OpenGL launcher not found: $INSTALL_BIN_DIR/$OVERLAY_LAUNCHER"
    fi

    if [ -f "$INSTALL_VULKAN_LAYER_DIR/$OVERLAY_MANIFEST" ]; then
        echo "==> Removing Vulkan layer manifest: $INSTALL_VULKAN_LAYER_DIR/$OVERLAY_MANIFEST"
        rm -f "$INSTALL_VULKAN_LAYER_DIR/$OVERLAY_MANIFEST"
    else
        echo "==> Vulkan layer manifest not found: $INSTALL_VULKAN_LAYER_DIR/$OVERLAY_MANIFEST"
    fi
}

##############################################################################

_write_vulkan_overlay_manifest() {
    local DEST="$1"
    local LIBRARY_PATH="$2"

    cat > "$DEST" <<EOF
{
    "file_format_version": "1.0.0",
    "layer": {
        "name": "VK_LAYER_LYMALINK_overlay",
        "type": "GLOBAL",
        "library_path": "${LIBRARY_PATH}",
        "api_version": "1.4.312",
        "implementation_version": "1",
        "description": "Lymalink Achievement Overlay",
        "functions": {
            "vkNegotiateLoaderLayerInterfaceVersion": "LymalinkLayer_vkNegotiateLoaderLayerInterfaceVersion",
            "vkGetInstanceProcAddr": "LymalinkLayer_vkGetInstanceProcAddr",
            "vkGetDeviceProcAddr": "LymalinkLayer_vkGetDeviceProcAddr"
        },
        "enable_environment": {
            "LYMALINK_OVERLAY_ENABLE": "1"
        },
        "disable_environment": {
            "LYMALINK_OVERLAY_DISABLE": "1"
        }
    }
}
EOF
}

##############################################################################

_install_flatpak_extension() {
    if ! command -v flatpak >/dev/null 2>&1; then
        echo "==> Flatpak not found, skipping VulkanLayer extension."
        return
    fi

    local BUILD_ROOT
    BUILD_ROOT="$(_get_build_root)"
    local OVERLAY_PATH="$BUILD_ROOT/release/bin/$OVERLAY_LIB"
    local OVERLAY_OPENGL_PATH="$BUILD_ROOT/release/bin/$OVERLAY_OPENGL_LIB"
    local OVERLAY_PRELOADER_PATH="$BUILD_ROOT/release/bin/$OVERLAY_PRELOADER_LIB"
    local EXT_DIR="$BUILD_ROOT/release/flatpak-extension"
    local REPO_DIR="$BUILD_ROOT/release/flatpak-repo"
    local BUNDLE_PATH="$BUILD_ROOT/release/${FLATPAK_EXTENSION_ID}.flatpak"

    if [ ! -f "$OVERLAY_PATH" ]; then
        echo "==> WARNING: Overlay library not found at $OVERLAY_PATH, skipping Flatpak extension."
        return
    fi
    if [ ! -f "$OVERLAY_OPENGL_PATH" ]; then
        echo "==> WARNING: OpenGL overlay library not found at $OVERLAY_OPENGL_PATH, skipping Flatpak extension."
        return
    fi
    if [ ! -f "$OVERLAY_PRELOADER_PATH" ]; then
        echo "==> WARNING: OpenGL preloader library not found at $OVERLAY_PRELOADER_PATH, skipping Flatpak extension."
        return
    fi

    echo "==> Building Flatpak VulkanLayer extension..."
    _prepare_flatpak_extension_tree "$OVERLAY_PATH" "$OVERLAY_OPENGL_PATH" "$OVERLAY_PRELOADER_PATH" "$EXT_DIR"
    rm -rf "$REPO_DIR" "$BUNDLE_PATH"
    mkdir -p "$REPO_DIR"
    flatpak build-export --runtime --arch=x86_64 "$REPO_DIR" "$EXT_DIR" "$FLATPAK_EXTENSION_BRANCH"
    flatpak build-bundle --runtime --arch=x86_64 "$REPO_DIR" "$BUNDLE_PATH" "$FLATPAK_EXTENSION_ID" "$FLATPAK_EXTENSION_BRANCH"

    local NEED_USER=0
    local APP
    for APP in "${FLATPAK_TARGET_APPS[@]}"; do
        if flatpak info --user "$APP" >/dev/null 2>&1; then
            NEED_USER=1
            break
        fi
    done

    # User-level install is the only active test path.
    if [ "$NEED_USER" -eq 0 ]; then
        NEED_USER=1
    fi

    local SCOPE
    if [ "$NEED_USER" -eq 1 ]; then
        SCOPE="--user"
        echo "==> Installing Flatpak extension ($SCOPE): $FLATPAK_EXTENSION_ID//$FLATPAK_EXTENSION_BRANCH"
        if ! flatpak install "$SCOPE" --bundle --or-update -y --noninteractive "$BUNDLE_PATH"; then
            echo "==> WARNING: Flatpak extension install failed for $SCOPE."
        fi
    fi
}

##############################################################################

_uninstall_flatpak_extension() {
    if ! command -v flatpak >/dev/null 2>&1; then
        return
    fi

    if flatpak info --user "$FLATPAK_EXTENSION_ID" >/dev/null 2>&1; then
        echo "==> Removing Flatpak extension (--user): $FLATPAK_EXTENSION_ID"
        flatpak uninstall --user -y --noninteractive "$FLATPAK_EXTENSION_ID" || true
    fi
}

##############################################################################

_prepare_flatpak_extension_tree() {
    local OVERLAY_PATH="$1"
    local OVERLAY_OPENGL_PATH="$2"
    local OVERLAY_PRELOADER_PATH="$3"
    local EXT_DIR="$4"

    rm -rf "$EXT_DIR"
    mkdir -p "$EXT_DIR/files/lib/x86_64-linux-gnu"
    mkdir -p "$EXT_DIR/files/share/vulkan/implicit_layer.d"

    cp "$OVERLAY_PATH" "$EXT_DIR/files/lib/x86_64-linux-gnu/$OVERLAY_LIB"
    cp "$OVERLAY_OPENGL_PATH" "$EXT_DIR/files/lib/x86_64-linux-gnu/$OVERLAY_OPENGL_LIB"
    cp "$OVERLAY_PRELOADER_PATH" "$EXT_DIR/files/lib/x86_64-linux-gnu/$OVERLAY_PRELOADER_LIB"
    chmod 755 "$EXT_DIR/files/lib/x86_64-linux-gnu/$OVERLAY_LIB"
    chmod 755 "$EXT_DIR/files/lib/x86_64-linux-gnu/$OVERLAY_OPENGL_LIB"
    chmod 755 "$EXT_DIR/files/lib/x86_64-linux-gnu/$OVERLAY_PRELOADER_LIB"

    _write_vulkan_overlay_manifest \
        "$EXT_DIR/files/share/vulkan/implicit_layer.d/$FLATPAK_EXTENSION_MANIFEST" \
        "/usr/lib/extensions/vulkan/lymalink/lib/x86_64-linux-gnu/${OVERLAY_LIB}"

    cat > "$EXT_DIR/metadata" <<EOF
[Runtime]
name=${FLATPAK_EXTENSION_ID}
runtime=org.freedesktop.Platform/x86_64/${FLATPAK_EXTENSION_BRANCH}
sdk=org.freedesktop.Sdk/x86_64/${FLATPAK_EXTENSION_BRANCH}

[ExtensionOf]
ref=runtime/org.freedesktop.Platform/x86_64/${FLATPAK_EXTENSION_BRANCH}
runtime=org.freedesktop.Platform/x86_64/${FLATPAK_EXTENSION_BRANCH}
EOF
}

##############################################################################

clean() {
    local BUILD_ROOT
    BUILD_ROOT="$(_get_build_root)"
    echo "==> Cleaning build directories..."
    make -C "$SCRIPT_DIR" BUILD_ROOT="$BUILD_ROOT" clean
    make -C "$OVERLAY_DIR" BUILD_ROOT="$BUILD_ROOT" clean
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

    echo "==> Building backend ($MODE_LOWER)..."
    make -C "$SCRIPT_DIR" BUILD="$MODE_LOWER" BUILD_ROOT="$BUILD_ROOT"

    echo "==> Building overlay ($MODE_LOWER)..."
    _install_imgui
    make -C "$OVERLAY_DIR" BUILD="$MODE_LOWER" BUILD_ROOT="$BUILD_ROOT"

    local BINARY_PATH="$BUILD_DIR/bin/$SERVICE_NAME"
    if [ -f "$BINARY_PATH" ] && [ -x "$BINARY_PATH" ]; then
        echo "==> Done: $BINARY_PATH (executable)"
    else
        echo "==> ERROR: Expected binary not found at $BINARY_PATH"
        exit 1
    fi

    local OVERLAY_PATH="$BUILD_DIR/bin/$OVERLAY_LIB"
    if [ -f "$OVERLAY_PATH" ]; then
        echo "==> Done: $OVERLAY_PATH (shared library)"
    else
        echo "==> WARNING: Overlay library not found at $OVERLAY_PATH"
    fi

    local OVERLAY_OPENGL_PATH="$BUILD_DIR/bin/$OVERLAY_OPENGL_LIB"
    if [ -f "$OVERLAY_OPENGL_PATH" ]; then
        echo "==> Done: $OVERLAY_OPENGL_PATH (shared library)"
    else
        echo "==> WARNING: OpenGL overlay library not found at $OVERLAY_OPENGL_PATH"
    fi

    local OVERLAY_PRELOADER_PATH="$BUILD_DIR/bin/$OVERLAY_PRELOADER_LIB"
    if [ -f "$OVERLAY_PRELOADER_PATH" ]; then
        echo "==> Done: $OVERLAY_PRELOADER_PATH (shared library)"
    else
        echo "==> WARNING: OpenGL preloader library not found at $OVERLAY_PRELOADER_PATH"
    fi
}

##############################################################################

deploy() {
    echo "==> Starting deploy..."
    clean
    build Release
    _install_binary
    _install_overlay
    _install_flatpak_extension
    _install_sounds
    _install_test_icon
    _install_service

    if _service_is_active; then
        echo "==> Service is running, restarting..."
        systemctl --user restart "$SERVICE_NAME"
    else
        echo "==> Enabling and starting service..."
        systemctl --user enable "$SERVICE_NAME"
        systemctl --user start "$SERVICE_NAME"
    fi

    local FLATPAK_REF="runtime/${FLATPAK_EXTENSION_ID}/x86_64/${FLATPAK_EXTENSION_BRANCH}"
    local FLATPAK_SHORT_REF="${FLATPAK_EXTENSION_ID}//${FLATPAK_EXTENSION_BRANCH}"
    local FLATPAK_STATUS="not installed"
    if command -v flatpak >/dev/null 2>&1; then
        if flatpak info --user "$FLATPAK_REF" >/dev/null 2>&1 \
            || flatpak info --user "$FLATPAK_SHORT_REF" >/dev/null 2>&1; then
            FLATPAK_STATUS="--user ${FLATPAK_REF}"
        fi
    else
        FLATPAK_STATUS="flatpak not found"
    fi

    echo ""
    systemctl --user status "$SERVICE_NAME" --no-pager || true
    echo ""
    echo "==> Deploy done."
    echo "    Binary:       $INSTALL_BIN_DIR/$SERVICE_NAME"
    echo "    Vulkan lib:   $INSTALL_LIB_DIR/$OVERLAY_LIB"
    echo "    OpenGL lib:   $INSTALL_LIB_DIR/$OVERLAY_OPENGL_LIB"
    echo "    Preloader:    $INSTALL_LIB_DIR/$OVERLAY_PRELOADER_LIB"
    echo "    Launcher:     $INSTALL_BIN_DIR/$OVERLAY_LAUNCHER"
    echo "    Vulkan layer: $INSTALL_VULKAN_LAYER_DIR/$OVERLAY_MANIFEST"
    echo "    Flatpak ext:  $FLATPAK_STATUS"
    echo "    Sounds:       $INSTALL_SOUND_DIR"
    echo "    Test icon:    $INSTALL_TEST_ICON_PATH"
    echo "    Service:      $SERVICE_FILE"
    echo "    Logs:         journalctl --user -u $SERVICE_NAME -f"
}

##############################################################################

_install_sounds() {
    echo "==> Installing achievement sounds to $INSTALL_SOUND_DIR..."
    mkdir -p "$INSTALL_SOUND_DIR"

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

    _uninstall_overlay
    _uninstall_flatpak_extension

    if [ -d "$INSTALL_SOUND_DIR" ]; then
        echo "==> Removing sounds: $INSTALL_SOUND_DIR"
        rm -rf "$INSTALL_SOUND_DIR"
    else
        echo "==> Sounds directory not found: $INSTALL_SOUND_DIR"
    fi

    if [ -f "$INSTALL_TEST_ICON_PATH" ]; then
        echo "==> Removing test icon: $INSTALL_TEST_ICON_PATH"
        rm -f "$INSTALL_TEST_ICON_PATH"
    else
        echo "==> Test icon not found: $INSTALL_TEST_ICON_PATH"
    fi

    echo "==> Uninstall done."
}

##############################################################################

run_tests() {
    local TEST_OUTPUT_MODE="${1:-}"

    if [ -n "$TEST_OUTPUT_MODE" ] && [ "$TEST_OUTPUT_MODE" != "--silent" ]; then
        echo "==> ERROR: Unknown test option: $TEST_OUTPUT_MODE"
        echo "==> Usage: $0 test [--silent]"
        exit 1
    fi

    local BUILD_ROOT
    BUILD_ROOT="$(_get_build_root)"

    clean
    echo "==> Building tests (debug)..."
    make -C "$SCRIPT_DIR" BUILD="debug" BUILD_ROOT="$BUILD_ROOT" "$BUILD_ROOT/debug/tests/SQLiteManagerTests"

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
    clean)     clean ;;
    debug)     build Debug ;;
    release)   build Release ;;
    test)      run_tests "${2:-}" ;;
    deploy)    deploy ;;
    start)     svc_start ;;
    stop)      svc_stop ;;
    restart)   svc_restart ;;
    status)    svc_status ;;
    logs)      svc_logs ;;
    uninstall) uninstall_service ;;
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
        echo ""
        echo "  Service commands:"
        echo "    start          Start lymalinkd service"
        echo "    stop           Stop lymalinkd service"
        echo "    restart        Restart lymalinkd service"
        echo "    status         Show service status"
        echo "    logs           Follow service journal output"
        echo "    uninstall      Stop + disable + remove lymalinkd service, binary and overlay"
        exit 1
        ;;
esac
