#!/usr/bin/env bash
#########################################################
# File: build.sh
# Date: 2026-06-01
# Author: Morsomus
# Copyright: see /LICENSE
# Description: Automated build and deployment script for Lymalink overlay.
# Usage:
#   ./build.sh clean        - Clean build directory
#   ./build.sh debug        - Debug build
#   ./build.sh release      - Release build
#   ./build.sh flatpak-debug   - Debug build targeting Freedesktop SDK
#   ./build.sh flatpak-release - Release build targeting Freedesktop SDK
#   ./build.sh deploy       - Clean + Release + strip + install overlay
#   ./build.sh deploy --debug - Clean + Debug + install overlay
#   ./build.sh uninstall    - Remove overlay libraries, launcher and manifest
#########################################################

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"

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

LOGIN_HOME="$(resolve_login_home)"
DATA_HOME="$(resolve_xdg_data_home "$LOGIN_HOME")"

# When enabled, build output is diverted to /tmp (RAMfs)
BUILD_TO_TMP=0

OVERLAY_LIB="lymalink-overlay.so"
OVERLAY_OPENGL_LIB="lymalink-overlay-opengl.so"
OVERLAY_PRELOADER_LIB="lymalink-overlay-preloader.so"
OVERLAY_LAUNCHER="lymalink-overlay"
OVERLAY_MANIFEST="lymalink_overlay.json"
IMGUI_VERSION="1.92.8"
IMGUI_DIR="$SCRIPT_DIR/src/imgui"

INSTALL_BIN_DIR="$LOGIN_HOME/.local/bin"
INSTALL_LIB_DIR="$LOGIN_HOME/.local/lib"
INSTALL_VULKAN_LAYER_DIR="$DATA_HOME/vulkan/implicit_layer.d"
FLATPAK_EXTENSION_ID="org.freedesktop.Platform.VulkanLayer.lymalink"
FLATPAK_EXTENSION_BRANCH="25.08"
FLATPAK_EXTENSION_MANIFEST="lymalink_overlay.x86_64.json"
FLATPAK_PLATFORM_ID="org.freedesktop.Platform"
FLATPAK_SDK_ID="org.freedesktop.Sdk"
SUPPORTED_DEPLOY_ARCH="x86_64"

##############################################################################

_get_build_root() {
    if [ "$BUILD_TO_TMP" -eq 1 ] 2>/dev/null; then
        echo "/tmp/lymalink-overlay-build"
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

_require_command() {
    if ! command -v "$1" >/dev/null 2>&1; then
        echo "==> ERROR: Required command not found: $1"
        exit 1
    fi
}

##############################################################################

_require_flatpak_sdk() {
    _require_command flatpak

    if ! flatpak info --user "$FLATPAK_SDK_ID//$FLATPAK_EXTENSION_BRANCH" >/dev/null 2>&1; then
        echo "==> ERROR: Required Flatpak SDK not installed: $FLATPAK_SDK_ID//$FLATPAK_EXTENSION_BRANCH"
        echo "==> Install it with: flatpak install --user flathub $FLATPAK_SDK_ID//$FLATPAK_EXTENSION_BRANCH"
        exit 1
    fi
}

##############################################################################

_check_flatpak_runtime_dependencies() {
    local BUILD_DIR="$1"

    if ! flatpak info --user "$FLATPAK_PLATFORM_ID//$FLATPAK_EXTENSION_BRANCH" >/dev/null 2>&1; then
        echo "==> ERROR: Required Flatpak runtime not installed: $FLATPAK_PLATFORM_ID//$FLATPAK_EXTENSION_BRANCH"
        echo "==> Install it with: flatpak install --user flathub $FLATPAK_PLATFORM_ID//$FLATPAK_EXTENSION_BRANCH"
        exit 1
    fi

    echo "==> Checking Flatpak overlay dependencies against $FLATPAK_PLATFORM_ID//$FLATPAK_EXTENSION_BRANCH..."
    flatpak run --user \
        --filesystem="$BUILD_DIR:ro" \
        --command=bash \
        "$FLATPAK_PLATFORM_ID//$FLATPAK_EXTENSION_BRANCH" \
        -c '
            for library in "$@"; do
                if ! output="$(ldd "$library" 2>&1)"; then
                    echo "==> ERROR: Unable to inspect Flatpak runtime dependencies for $library" >&2
                    printf "%s\n" "$output" >&2
                    exit 1
                fi
                if printf "%s\n" "$output" | grep -q "not found"; then
                    echo "==> ERROR: Flatpak runtime libraries missing for $library:" >&2
                    printf "%s\n" "$output" | grep "not found" >&2
                    exit 1
                fi
            done
        ' bash \
        "$BUILD_DIR/bin/$OVERLAY_LIB" \
        "$BUILD_DIR/bin/$OVERLAY_OPENGL_LIB" \
        "$BUILD_DIR/bin/$OVERLAY_PRELOADER_LIB"
}

##############################################################################

_install_imgui() {
    if [ -f "$IMGUI_DIR/imgui.h" ]; then
        echo "==> ImGui found at $IMGUI_DIR"
        return 0
    fi

    echo "==> ImGui not found. Downloading version $IMGUI_VERSION..."

    local ZIP_URL="https://github.com/ocornut/imgui/archive/refs/tags/v${IMGUI_VERSION}.zip"
    local ZIP_FILE="/tmp/imgui-${IMGUI_VERSION}.zip"

    _require_command wget
    _require_command unzip

    mkdir -p "$IMGUI_DIR"

    echo "==> Downloading ImGui $IMGUI_VERSION..."
    wget -q --show-progress -O "$ZIP_FILE" "$ZIP_URL"

    echo "==> Extracting ImGui..."
    unzip -q "$ZIP_FILE" -d /tmp/
    mv /tmp/imgui-${IMGUI_VERSION}/* "$IMGUI_DIR/"
    rm -rf /tmp/imgui-${IMGUI_VERSION} "$ZIP_FILE"

    echo "==> ImGui successfully installed to $IMGUI_DIR"
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

_install_overlay() {
    local MODE_LOWER="$1"
    local BUILD_ROOT
    BUILD_ROOT="$(_get_build_root)"
    local OVERLAY_PATH="$BUILD_ROOT/$MODE_LOWER/bin/$OVERLAY_LIB"
    local OVERLAY_OPENGL_PATH="$BUILD_ROOT/$MODE_LOWER/bin/$OVERLAY_OPENGL_LIB"
    local OVERLAY_PRELOADER_PATH="$BUILD_ROOT/$MODE_LOWER/bin/$OVERLAY_PRELOADER_LIB"
    local OVERLAY_LAUNCHER_PATH="$SCRIPT_DIR/lymalink-overlay.sh"

    for PATH_TO_CHECK in \
        "$OVERLAY_PATH" \
        "$OVERLAY_OPENGL_PATH" \
        "$OVERLAY_PRELOADER_PATH" \
        "$OVERLAY_LAUNCHER_PATH"; do
        if [ ! -f "$PATH_TO_CHECK" ]; then
            echo "==> ERROR: Required overlay artifact not found at $PATH_TO_CHECK"
            exit 1
        fi
    done

    if [ "$MODE_LOWER" = "release" ]; then
        echo "==> Stripping overlay libraries..."
        strip "$OVERLAY_PATH"
        strip "$OVERLAY_OPENGL_PATH"
        strip "$OVERLAY_PRELOADER_PATH"
    fi

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
    chmod 755 "$EXT_DIR/files/lib/x86_64-linux-gnu/"*.so

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

_install_flatpak_extension() {
    local MODE_LOWER="$1"
    if ! command -v flatpak >/dev/null 2>&1; then
        echo "==> Flatpak not found, skipping VulkanLayer extension."
        return
    fi

    local BUILD_ROOT
    BUILD_ROOT="$(_get_build_root)"
    build_flatpak "$MODE_LOWER"
    local OVERLAY_PATH="$BUILD_ROOT/flatpak/$MODE_LOWER/bin/$OVERLAY_LIB"
    local OVERLAY_OPENGL_PATH="$BUILD_ROOT/flatpak/$MODE_LOWER/bin/$OVERLAY_OPENGL_LIB"
    local OVERLAY_PRELOADER_PATH="$BUILD_ROOT/flatpak/$MODE_LOWER/bin/$OVERLAY_PRELOADER_LIB"
    local EXT_DIR="$BUILD_ROOT/$MODE_LOWER/flatpak-extension"
    local REPO_DIR="$BUILD_ROOT/$MODE_LOWER/flatpak-repo"
    local BUNDLE_PATH="$BUILD_ROOT/$MODE_LOWER/${FLATPAK_EXTENSION_ID}.flatpak"

    echo "==> Building Flatpak VulkanLayer extension..."
    _prepare_flatpak_extension_tree "$OVERLAY_PATH" "$OVERLAY_OPENGL_PATH" "$OVERLAY_PRELOADER_PATH" "$EXT_DIR"
    rm -rf "$REPO_DIR" "$BUNDLE_PATH"
    mkdir -p "$REPO_DIR"
    flatpak build-export --runtime --arch=x86_64 "$REPO_DIR" "$EXT_DIR" "$FLATPAK_EXTENSION_BRANCH"
    flatpak build-bundle --runtime --arch=x86_64 "$REPO_DIR" "$BUNDLE_PATH" "$FLATPAK_EXTENSION_ID" "$FLATPAK_EXTENSION_BRANCH"

    echo "==> Installing Flatpak extension (--user): $FLATPAK_EXTENSION_ID//$FLATPAK_EXTENSION_BRANCH"
    if ! flatpak install --user --bundle --or-update -y --noninteractive "$BUNDLE_PATH"; then
        echo "==> WARNING: Flatpak extension install failed for --user."
    fi
}

##############################################################################

_uninstall_flatpak_extension() {
    if command -v flatpak >/dev/null 2>&1 \
        && flatpak info --user "$FLATPAK_EXTENSION_ID" >/dev/null 2>&1; then
        echo "==> Removing Flatpak extension (--user): $FLATPAK_EXTENSION_ID"
        flatpak uninstall --user -y --noninteractive "$FLATPAK_EXTENSION_ID" || true
    fi
}

##############################################################################

clean() {
    local BUILD_ROOT
    BUILD_ROOT="$(_get_build_root)"
    echo "==> Cleaning overlay build directory..."
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
    local MAKE_JOBS
    MAKE_JOBS="$(_make_jobs)"

    echo "==> Building overlay ($MODE_LOWER)..."
    _install_imgui
    make -j"$MAKE_JOBS" -C "$SCRIPT_DIR" BUILD="$MODE_LOWER" BUILD_ROOT="$BUILD_ROOT"

    local LIB
    for LIB in "$OVERLAY_LIB" "$OVERLAY_OPENGL_LIB" "$OVERLAY_PRELOADER_LIB"; do
        if [ -f "$BUILD_DIR/bin/$LIB" ]; then
            echo "==> Done: $BUILD_DIR/bin/$LIB (shared library)"
        else
            echo "==> ERROR: Expected library not found at $BUILD_DIR/bin/$LIB"
            exit 1
        fi
    done
}

##############################################################################

build_flatpak() {
    local MODE="$1"
    local MODE_LOWER
    MODE_LOWER="$(echo "$MODE" | tr '[:upper:]' '[:lower:]')"
    local BUILD_ROOT
    BUILD_ROOT="$(_get_build_root)/flatpak"
    local BUILD_DIR="$BUILD_ROOT/${MODE_LOWER}"
    local MAKE_JOBS
    MAKE_JOBS="$(_make_jobs)"

    echo "==> Building Flatpak overlay ($MODE_LOWER, Freedesktop SDK $FLATPAK_EXTENSION_BRANCH)..."
    _install_imgui
    _require_flatpak_sdk
    flatpak run --user \
        --filesystem="$PROJECT_ROOT" \
        --command=make \
        "$FLATPAK_SDK_ID//$FLATPAK_EXTENSION_BRANCH" \
        -j"$MAKE_JOBS" -C "$SCRIPT_DIR" BUILD="$MODE_LOWER" BUILD_ROOT="$BUILD_ROOT"

    local LIB
    for LIB in "$OVERLAY_LIB" "$OVERLAY_OPENGL_LIB" "$OVERLAY_PRELOADER_LIB"; do
        if [ -f "$BUILD_DIR/bin/$LIB" ]; then
            echo "==> Done: $BUILD_DIR/bin/$LIB (Flatpak shared library)"
        else
            echo "==> ERROR: Expected Flatpak library not found at $BUILD_DIR/bin/$LIB"
            exit 1
        fi
    done

    _check_flatpak_runtime_dependencies "$BUILD_DIR"
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

    if [ "$(uname -m)" != "$SUPPORTED_DEPLOY_ARCH" ]; then
        echo "==> ERROR: Overlay deploy currently supports $SUPPORTED_DEPLOY_ARCH only."
        exit 1
    fi

    echo "==> Starting overlay deploy..."
    clean
    build "$MODE"
    _install_overlay "$MODE_LOWER"
    _install_flatpak_extension "$MODE_LOWER"

    echo "==> Overlay deploy done."
    echo "    Vulkan lib:   $INSTALL_LIB_DIR/$OVERLAY_LIB"
    echo "    OpenGL lib:   $INSTALL_LIB_DIR/$OVERLAY_OPENGL_LIB"
    echo "    Preloader:    $INSTALL_LIB_DIR/$OVERLAY_PRELOADER_LIB"
    echo "    Launcher:     $INSTALL_BIN_DIR/$OVERLAY_LAUNCHER"
    echo "    Vulkan layer: $INSTALL_VULKAN_LAYER_DIR/$OVERLAY_MANIFEST"
}

##############################################################################

uninstall_overlay() {
    local LIB
    for LIB in "$OVERLAY_LIB" "$OVERLAY_OPENGL_LIB" "$OVERLAY_PRELOADER_LIB"; do
        if [ -f "$INSTALL_LIB_DIR/$LIB" ]; then
            echo "==> Removing overlay library: $INSTALL_LIB_DIR/$LIB"
            rm -f "$INSTALL_LIB_DIR/$LIB"
        fi
    done

    rm -f "$INSTALL_BIN_DIR/$OVERLAY_LAUNCHER"
    rm -f "$INSTALL_VULKAN_LAYER_DIR/$OVERLAY_MANIFEST"
    _uninstall_flatpak_extension
    echo "==> Overlay uninstall done."
}

##############################################################################

case "${1:-}" in
    clean)           _require_no_extra_args clean "${@:2}"; clean ;;
    debug)           _require_no_extra_args debug "${@:2}"; build Debug ;;
    release)         _require_no_extra_args release "${@:2}"; build Release ;;
    flatpak-debug)   _require_no_extra_args flatpak-debug "${@:2}"; build_flatpak Debug ;;
    flatpak-release) _require_no_extra_args flatpak-release "${@:2}"; build_flatpak Release ;;
    deploy)          deploy "${@:2}" ;;
    uninstall)       _require_no_extra_args uninstall "${@:2}"; uninstall_overlay ;;
    *)
        echo "Usage: $0 [clean|debug|release|flatpak-debug|flatpak-release|deploy [--debug]|uninstall]"
        exit 1
        ;;
esac
