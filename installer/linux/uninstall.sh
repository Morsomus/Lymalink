#!/usr/bin/env bash
#########################################################
# File: uninstall.sh
# Date: 2026-05-31
# Author: Morsomus
# Copyright: see /LICENSE
# Description: Removes installer-managed Lymalink files
#########################################################

set -euo pipefail

# Ensure the HOME environment variable
if [ -z "${HOME:-}" ]; then
    echo "==> ERROR: HOME is not set." >&2
    exit 1
fi

resolve_login_home() {
    local login_home=""

    # Determine actual login home via getent if available
    if command -v getent >/dev/null 2>&1 && [ -n "${USER:-}" ]; then
        login_home="$(getent passwd "$USER" | cut -d: -f6)"
    fi

    # Handle Snap-specific home path redirections
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

    # Check if the provided path is contained within a Snap home directory
    case "$path" in
        "$login_home"/snap/*) return 0 ;;
        *) return 1 ;;
    esac
}

LOGIN_HOME="$(resolve_login_home)"

# Resolve XDG data home, avoiding Snap paths
if [ -n "${XDG_DATA_HOME:-}" ] && ! path_inside_snap_home "$XDG_DATA_HOME" "$LOGIN_HOME"; then
    DATA_HOME="$XDG_DATA_HOME"
else
    DATA_HOME="$LOGIN_HOME/.local/share"
fi

# Resolve XDG config home, avoiding Snap paths
if [ -n "${XDG_CONFIG_HOME:-}" ] && ! path_inside_snap_home "$XDG_CONFIG_HOME" "$LOGIN_HOME"; then
    CONFIG_HOME="$XDG_CONFIG_HOME"
else
    CONFIG_HOME="$LOGIN_HOME/.config"
fi

if [ -n "${XDG_STATE_HOME:-}" ] && ! path_inside_snap_home "$XDG_STATE_HOME" "$LOGIN_HOME"; then
    STATE_HOME="$XDG_STATE_HOME"
else
    STATE_HOME="$LOGIN_HOME/.local/state"
fi

BIN_DIR="$LOGIN_HOME/.local/bin"
LIB_DIR="$LOGIN_HOME/.local/lib"
APP_LIB_DIR="$LIB_DIR/lymalink"
FRONTEND_DIR="$APP_LIB_DIR/frontend"
APP_CONFIG_DIR="$CONFIG_HOME/Lymalink"
SERVICE_DIR="$CONFIG_HOME/systemd/user"
VULKAN_DIR="$DATA_HOME/vulkan/implicit_layer.d"
ICON_DIR="$DATA_HOME/icons/hicolor/256x256/apps"
APPLICATION_DIR="$DATA_HOME/applications"
APP_DATA_DIR="$DATA_HOME/Lymalink"
SOUND_DIR="$APP_DATA_DIR/sounds"
FLATPAK_EXTENSION_ID="org.freedesktop.Platform.VulkanLayer.lymalink"
PATH_BLOCK_START="# >>> Lymalink installer PATH >>>"
PATH_BLOCK_END="# <<< Lymalink installer PATH <<<"

##############################################################################

remove_path_block() {
    local profile="$1"
    local temporary

    # Skip if the file doesn't exist or doesn't contain the start marker
    if [ ! -f "$profile" ] || ! grep -Fq "$PATH_BLOCK_START" "$profile"; then
        return
    fi

    # Use awk to filter out everything between the start and end markers
    temporary="${profile}.lymalink-tmp"
    awk -v start="$PATH_BLOCK_START" -v end="$PATH_BLOCK_END" '
        $0 == start { skip = 1; next }
        $0 == end { skip = 0; next }
        !skip { print }
    ' "$profile" > "$temporary"
    mv "$temporary" "$profile"
}

##############################################################################

echo "==> Removing Lymalink..."

# Stop and disable the user-level systemd service if systemctl is available
if command -v systemctl >/dev/null 2>&1; then
    systemctl --user stop lymalinkd.service >/dev/null 2>&1 || true
    systemctl --user disable lymalinkd.service >/dev/null 2>&1 || true
fi

# Remove binaries, shared libraries, service files, Vulkan layers, and desktop entries
rm -f \
    "$BIN_DIR/Lymalink" \
    "$BIN_DIR/lymalinkd" \
    "$BIN_DIR/lymalink-overlay" \
    "$LIB_DIR/lymalink-overlay.so" \
    "$LIB_DIR/lymalink-overlay-opengl.so" \
    "$LIB_DIR/lymalink-overlay-preloader.so" \
    "$SERVICE_DIR/lymalinkd.service" \
    "$VULKAN_DIR/lymalink_overlay.json" \
    "$VULKAN_DIR/lymalink_overlay.x86_64.json" \
    "$VULKAN_DIR/lymalink_overlay.x86.json" \
    "$ICON_DIR/lymalink.png" \
    "$APPLICATION_DIR/lymalink.desktop" \
    "$APP_DATA_DIR/64x64-lymalink-test-icon.png"

# Remove overlay library files
rm -f \
    "$LIB_DIR/x86_64-linux-gnu/lymalink-overlay.so" \
    "$LIB_DIR/x86_64-linux-gnu/lymalink-overlay-opengl.so" \
    "$LIB_DIR/x86_64-linux-gnu/lymalink-overlay-preloader.so" \
    "$LIB_DIR/i386-linux-gnu/lymalink-overlay.so" \
    "$LIB_DIR/i386-linux-gnu/lymalink-overlay-opengl.so" \
    "$LIB_DIR/i386-linux-gnu/lymalink-overlay-preloader.so"

# Remove data directories and cleanup empty folders
rm -rf "$SOUND_DIR"
rm -rf "$FRONTEND_DIR"
rm -f "$APP_DATA_DIR/.installer-sounds" "$APP_DATA_DIR/.backend-sounds"
rmdir "$LIB_DIR/x86_64-linux-gnu" >/dev/null 2>&1 || true
rmdir "$LIB_DIR/i386-linux-gnu" >/dev/null 2>&1 || true
rmdir "$APP_LIB_DIR" >/dev/null 2>&1 || true
rmdir "$APP_DATA_DIR" >/dev/null 2>&1 || true

# Uninstall the Lymalink Vulkan layer extension from Flatpak if installed
if command -v flatpak >/dev/null 2>&1 \
    && flatpak info --user "$FLATPAK_EXTENSION_ID" >/dev/null 2>&1; then
    flatpak uninstall --user -y --noninteractive "$FLATPAK_EXTENSION_ID" || true
fi

# Clean up PATH additions from common shell profile files
remove_path_block "$LOGIN_HOME/.bash_profile"
remove_path_block "$LOGIN_HOME/.profile"
remove_path_block "$LOGIN_HOME/.bashrc"

# Remove generated user data while preserving configuration and database files.
if [ -d "$APP_DATA_DIR" ]; then
    find "$APP_DATA_DIR" -depth -mindepth 1 \
        ! -name "lymalink_database" \
        ! -name "lymalink_database-wal" \
        ! -name "lymalink_database-shm" \
        -exec rm -rf {} +
    rmdir "$APP_DATA_DIR" >/dev/null 2>&1 || true
fi
if [ -d "$APP_CONFIG_DIR" ]; then
    find "$APP_CONFIG_DIR" -depth -mindepth 1 ! -name "config.ini" -exec rm -rf {} +
    rmdir "$APP_CONFIG_DIR" >/dev/null 2>&1 || true
fi
rm -rf "$STATE_HOME/Lymalink" "$STATE_HOME/lymalink"

# Refresh systemd user unit configurations if available
if command -v systemctl >/dev/null 2>&1; then
    systemctl --user daemon-reload >/dev/null 2>&1 || true
fi

# Refresh GTK icon cache if the utility is available
if command -v gtk-update-icon-cache >/dev/null 2>&1; then
    gtk-update-icon-cache -f -t "$DATA_HOME/icons/hicolor" >/dev/null 2>&1 || true
fi

# Refresh desktop entry database if the utility is available
if command -v update-desktop-database >/dev/null 2>&1; then
    update-desktop-database "$APPLICATION_DIR" >/dev/null 2>&1 || true
fi

# Self-destruct: remove the uninstaller binary itself
rm -f "$BIN_DIR/uninstall-lymalink"

echo "==> Lymalink uninstall done."
echo "    User configuration and database files were preserved."
