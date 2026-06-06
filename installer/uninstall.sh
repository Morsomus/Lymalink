#!/usr/bin/env bash
#########################################################
# File: uninstall.sh
# Date: 2026-05-31
# Author: Morsomus
# Copyright: see /LICENSE
# Description: Removes installer-managed Lymalink files
#########################################################

set -euo pipefail

if [ -z "${HOME:-}" ]; then
    echo "==> ERROR: HOME is not set." >&2
    exit 1
fi

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

if [ -n "${XDG_DATA_HOME:-}" ] && ! path_inside_snap_home "$XDG_DATA_HOME" "$LOGIN_HOME"; then
    DATA_HOME="$XDG_DATA_HOME"
else
    DATA_HOME="$LOGIN_HOME/.local/share"
fi

if [ -n "${XDG_CONFIG_HOME:-}" ] && ! path_inside_snap_home "$XDG_CONFIG_HOME" "$LOGIN_HOME"; then
    CONFIG_HOME="$XDG_CONFIG_HOME"
else
    CONFIG_HOME="$LOGIN_HOME/.config"
fi

BIN_DIR="$LOGIN_HOME/.local/bin"
LIB_DIR="$LOGIN_HOME/.local/lib"
APP_LIB_DIR="$LIB_DIR/lymalink"
FRONTEND_DIR="$APP_LIB_DIR/frontend"
SERVICE_DIR="$CONFIG_HOME/systemd/user"
VULKAN_DIR="$DATA_HOME/vulkan/implicit_layer.d"
ICON_DIR="$DATA_HOME/icons/hicolor/256x256/apps"
APPLICATION_DIR="$DATA_HOME/applications"
APP_DATA_DIR="$DATA_HOME/Lymalink"
SOUND_DIR="$APP_DATA_DIR/sounds"
FLATPAK_EXTENSION_ID="org.freedesktop.Platform.VulkanLayer.lymalink"
PATH_BLOCK_START="# >>> Lymalink installer PATH >>>"
PATH_BLOCK_END="# <<< Lymalink installer PATH <<<"

remove_path_block() {
    local profile="$1"
    local temporary

    if [ ! -f "$profile" ] || ! grep -Fq "$PATH_BLOCK_START" "$profile"; then
        return
    fi

    temporary="${profile}.lymalink-tmp"
    awk -v start="$PATH_BLOCK_START" -v end="$PATH_BLOCK_END" '
        $0 == start { skip = 1; next }
        $0 == end { skip = 0; next }
        !skip { print }
    ' "$profile" > "$temporary"
    mv "$temporary" "$profile"
}

echo "==> Removing Lymalink..."

if command -v systemctl >/dev/null 2>&1; then
    systemctl --user stop lymalinkd.service >/dev/null 2>&1 || true
    systemctl --user disable lymalinkd.service >/dev/null 2>&1 || true
fi

rm -f \
    "$BIN_DIR/Lymalink" \
    "$BIN_DIR/lymalinkd" \
    "$BIN_DIR/lymalink-overlay" \
    "$LIB_DIR/lymalink-overlay.so" \
    "$LIB_DIR/lymalink-overlay-opengl.so" \
    "$LIB_DIR/lymalink-overlay-preloader.so" \
    "$SERVICE_DIR/lymalinkd.service" \
    "$VULKAN_DIR/lymalink_overlay.json" \
    "$ICON_DIR/lymalink.png" \
    "$APPLICATION_DIR/lymalink.desktop" \
    "$APP_DATA_DIR/64x64-lymalink-test-icon.png"

rm -rf "$SOUND_DIR"
rm -rf "$FRONTEND_DIR"
rm -f "$APP_DATA_DIR/.installer-sounds" "$APP_DATA_DIR/.backend-sounds"
rmdir "$APP_LIB_DIR" >/dev/null 2>&1 || true
rmdir "$APP_DATA_DIR" >/dev/null 2>&1 || true

if command -v flatpak >/dev/null 2>&1 \
    && flatpak info --user "$FLATPAK_EXTENSION_ID" >/dev/null 2>&1; then
    flatpak uninstall --user -y --noninteractive "$FLATPAK_EXTENSION_ID" || true
fi

remove_path_block "$LOGIN_HOME/.bash_profile"
remove_path_block "$LOGIN_HOME/.profile"
remove_path_block "$LOGIN_HOME/.bashrc"

if command -v systemctl >/dev/null 2>&1; then
    systemctl --user daemon-reload >/dev/null 2>&1 || true
fi

if command -v gtk-update-icon-cache >/dev/null 2>&1; then
    gtk-update-icon-cache -f -t "$DATA_HOME/icons/hicolor" >/dev/null 2>&1 || true
fi

if command -v update-desktop-database >/dev/null 2>&1; then
    update-desktop-database "$APPLICATION_DIR" >/dev/null 2>&1 || true
fi

rm -f "$BIN_DIR/uninstall-lymalink"

echo "==> Lymalink uninstall done."
echo "    User configuration and database files were preserved."
