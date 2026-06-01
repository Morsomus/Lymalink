#!/usr/bin/env bash
#########################################################
# File: install.sh
# Date: 2026-05-31
# Author: Morsomus
# Copyright: see /LICENSE
# Description: Installs Lymalink into user-level directories.
#########################################################

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

if [ -z "${HOME:-}" ]; then
    echo "==> ERROR: HOME is not set." >&2
    exit 1
fi

DATA_HOME="${XDG_DATA_HOME:-$HOME/.local/share}"
CONFIG_HOME="${XDG_CONFIG_HOME:-$HOME/.config}"
BIN_DIR="$HOME/.local/bin"
LIB_DIR="$HOME/.local/lib"
SERVICE_DIR="$CONFIG_HOME/systemd/user"
VULKAN_DIR="$DATA_HOME/vulkan/implicit_layer.d"
ICON_DIR="$DATA_HOME/icons/hicolor/256x256/apps"
APPLICATION_DIR="$DATA_HOME/applications"
APP_DATA_DIR="$DATA_HOME/Lymalink"
SOUND_DIR="$APP_DATA_DIR/sounds"
FLATPAK_EXTENSION_ID="org.freedesktop.Platform.VulkanLayer.lymalink"
FLATPAK_BUNDLE="$SCRIPT_DIR/flatpak/${FLATPAK_EXTENSION_ID}.flatpak"
PATH_BLOCK_START="# >>> Lymalink installer PATH >>>"
PATH_BLOCK_END="# <<< Lymalink installer PATH <<<"

require_command() {
    if ! command -v "$1" >/dev/null 2>&1; then
        echo "==> ERROR: Required command not found: $1" >&2
        exit 1
    fi
}

require_file() {
    if [ ! -f "$1" ]; then
        echo "==> ERROR: Required installer payload file not found: $1" >&2
        exit 1
    fi
}

check_elf_dependencies() {
    local artifact
    local output
    local missing=0

    for artifact in \
        "$SCRIPT_DIR/bin/Lymalink" \
        "$SCRIPT_DIR/bin/lymalinkd" \
        "$SCRIPT_DIR/lib/lymalink-overlay.so" \
        "$SCRIPT_DIR/lib/lymalink-overlay-opengl.so" \
        "$SCRIPT_DIR/lib/lymalink-overlay-preloader.so"; do
        if ! output="$(ldd "$artifact" 2>&1)"; then
            echo "==> ERROR: Unable to inspect runtime dependencies for $artifact" >&2
            printf '%s\n' "$output" >&2
            exit 1
        fi
        if printf '%s\n' "$output" | grep -q 'not found'; then
            echo "==> ERROR: Missing runtime libraries required by $artifact:" >&2
            printf '%s\n' "$output" | grep 'not found' >&2
            missing=1
        fi
    done

    if [ "$missing" -ne 0 ]; then
        echo "==> Install the missing distro packages and run the installer again." >&2
        exit 1
    fi
}

install_path_block() {
    local profile="$1"

    if grep -Fq "$PATH_BLOCK_START" "$profile" 2>/dev/null; then
        return
    fi

    cat >> "$profile" <<'EOF'

# >>> Lymalink installer PATH >>>
case ":$PATH:" in
    *:"$HOME/.local/bin":*) ;;
    *) export PATH="$HOME/.local/bin:$PATH" ;;
esac
# <<< Lymalink installer PATH <<<
EOF
}

replace_home_placeholder() {
    local destination="$1"
    local escaped_home
    escaped_home="$(printf '%s' "$HOME" | sed 's/[\\&#]/\\&/g')"
    sed -i "s#HOME_PLACEHOLDER#$escaped_home#g" "$destination"
}

for command_name in flatpak grep install ldd sed systemctl; do
    require_command "$command_name"
done

if [ ! -f "$FLATPAK_BUNDLE" ]; then
    echo "==> ERROR: Flatpak extension bundle not found: $FLATPAK_BUNDLE" >&2
    exit 1
fi

for payload_file in \
    "$SCRIPT_DIR/bin/Lymalink" \
    "$SCRIPT_DIR/bin/lymalinkd" \
    "$SCRIPT_DIR/bin/lymalink-overlay" \
    "$SCRIPT_DIR/uninstall.sh" \
    "$SCRIPT_DIR/lib/lymalink-overlay.so" \
    "$SCRIPT_DIR/lib/lymalink-overlay-opengl.so" \
    "$SCRIPT_DIR/lib/lymalink-overlay-preloader.so" \
    "$SCRIPT_DIR/share/vulkan/implicit_layer.d/lymalink_overlay.json" \
    "$SCRIPT_DIR/share/icons/hicolor/256x256/apps/lymalink.png" \
    "$SCRIPT_DIR/share/applications/lymalink.desktop" \
    "$SCRIPT_DIR/share/Lymalink/64x64-lymalink-test-icon.png" \
    "$SCRIPT_DIR/systemd/lymalinkd.service"; do
    require_file "$payload_file"
done

if ! compgen -G "$SCRIPT_DIR/share/Lymalink/sounds/*.ogg" >/dev/null; then
    echo "==> ERROR: Required installer sound files not found." >&2
    exit 1
fi

check_elf_dependencies

echo "==> Installing Lymalink files..."
rm -rf "$SOUND_DIR"
mkdir -p "$BIN_DIR" "$LIB_DIR" "$SERVICE_DIR" "$VULKAN_DIR" "$ICON_DIR" "$APPLICATION_DIR" "$SOUND_DIR"
rm -f "$APP_DATA_DIR/.installer-sounds" "$APP_DATA_DIR/.backend-sounds"
install -m 755 "$SCRIPT_DIR/bin/Lymalink" "$BIN_DIR/Lymalink"
install -m 755 "$SCRIPT_DIR/bin/lymalinkd" "$BIN_DIR/lymalinkd"
install -m 755 "$SCRIPT_DIR/bin/lymalink-overlay" "$BIN_DIR/lymalink-overlay"
install -m 755 "$SCRIPT_DIR/uninstall.sh" "$BIN_DIR/uninstall-lymalink"
install -m 755 "$SCRIPT_DIR/lib/lymalink-overlay.so" "$LIB_DIR/lymalink-overlay.so"
install -m 755 "$SCRIPT_DIR/lib/lymalink-overlay-opengl.so" "$LIB_DIR/lymalink-overlay-opengl.so"
install -m 755 "$SCRIPT_DIR/lib/lymalink-overlay-preloader.so" "$LIB_DIR/lymalink-overlay-preloader.so"
install -m 644 "$SCRIPT_DIR/share/vulkan/implicit_layer.d/lymalink_overlay.json" "$VULKAN_DIR/lymalink_overlay.json"
install -m 644 "$SCRIPT_DIR/share/icons/hicolor/256x256/apps/lymalink.png" "$ICON_DIR/lymalink.png"
install -m 644 "$SCRIPT_DIR/share/applications/lymalink.desktop" "$APPLICATION_DIR/lymalink.desktop"
install -m 644 "$SCRIPT_DIR/share/Lymalink/64x64-lymalink-test-icon.png" "$APP_DATA_DIR/64x64-lymalink-test-icon.png"
install -m 644 "$SCRIPT_DIR/systemd/lymalinkd.service" "$SERVICE_DIR/lymalinkd.service"

for sound_file in "$SCRIPT_DIR/share/Lymalink/sounds/"*.ogg; do
    sound_name="${sound_file##*/}"
    install -m 644 "$sound_file" "$SOUND_DIR/$sound_name"
done

replace_home_placeholder "$VULKAN_DIR/lymalink_overlay.json"
replace_home_placeholder "$APPLICATION_DIR/lymalink.desktop"
replace_home_placeholder "$SERVICE_DIR/lymalinkd.service"

echo "==> Installing Flatpak VulkanLayer extension..."
flatpak install --user --bundle --or-update -y --noninteractive "$FLATPAK_BUNDLE"

case ":$PATH:" in
    *:"$BIN_DIR":*) ;;
    *)
        install_path_block "$HOME/.bash_profile"
        install_path_block "$HOME/.profile"
        ;;
esac

echo "==> Reloading systemd user units..."
systemctl --user daemon-reload

if command -v gtk-update-icon-cache >/dev/null 2>&1; then
    gtk-update-icon-cache -f -t "$DATA_HOME/icons/hicolor" >/dev/null 2>&1 || true
else
    echo "==> WARNING: gtk-update-icon-cache not found; icon cache was not refreshed."
fi

if command -v update-desktop-database >/dev/null 2>&1; then
    update-desktop-database "$APPLICATION_DIR" >/dev/null 2>&1 || true
else
    echo "==> WARNING: update-desktop-database not found; desktop cache was not refreshed."
fi

echo "==> Lymalink install done."
echo "    Frontend:      $BIN_DIR/Lymalink"
echo "    Backend:       $BIN_DIR/lymalinkd"
echo "    Service unit:  $SERVICE_DIR/lymalinkd.service"
echo "    Flatpak layer: $FLATPAK_EXTENSION_ID//25.08"
echo "    Uninstall:     $BIN_DIR/uninstall-lymalink"
echo "    The desktop application controls backend service activation."
