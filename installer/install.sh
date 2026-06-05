#!/usr/bin/env bash
#########################################################
# File: install.sh
# Date: 2026-05-31
# Author: Morsomus
# Copyright: see /LICENSE
# Description: Installs Lymalink into user-level directories
#########################################################

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

if [ "$EUID" -eq 0 ]; then
    echo "==> ERROR: Do not run the Lymalink installer as root." >&2
    echo "==> Lymalink installs into the current user's home directory and does not require sudo." >&2
    exit 1
fi

if [ -z "${HOME:-}" ]; then
    echo "==> ERROR: HOME is not set." >&2
    exit 1
fi

DATA_HOME="${XDG_DATA_HOME:-$HOME/.local/share}"
CONFIG_HOME="${XDG_CONFIG_HOME:-$HOME/.config}"
BIN_DIR="$HOME/.local/bin"
LIB_DIR="$HOME/.local/lib"
APP_LIB_DIR="$LIB_DIR/lymalink"
FRONTEND_DIR="$APP_LIB_DIR/frontend"
FRONTEND_BINARY="$SCRIPT_DIR/lib/lymalink/frontend/bin/Lymalink"
FRONTEND_QT_BUILD_VERSION_FILE="$SCRIPT_DIR/lib/lymalink/frontend/qt-build-version"
FRONTEND_PRIVATE_LIB_DIR="$SCRIPT_DIR/lib/lymalink/frontend/lib"
FRONTEND_PRIVATE_QML_DIR="$SCRIPT_DIR/lib/lymalink/frontend/qml"
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
MISSING_LIBRARIES=()
INCOMPATIBLE_LIBRARIES=()
MISSING_COMMANDS=()
MISSING_PAYLOAD_FILES=()
REQUIRED_FRONTEND_QML_MODULES=(
    Qt/labs/folderlistmodel
    QtQml/Models
    QtQuick
    QtQuick/Controls
    QtQuick/Dialogs
    QtQuick/Effects
    QtQuick/Controls/Fusion
    QtQuick/Controls/Fusion/impl
    QtQuick/Layouts
    QtQuick/Shapes
    QtQuick/Templates
    QtQuick/Window
)
REQUIRED_FRONTEND_QT_PLUGIN_FILES=(
    platforms/libqwayland.so
    platforms/libqxcb.so
    sqldrivers/libqsqlite.so
    xcbglintegrations/libqxcb-egl-integration.so
    xcbglintegrations/libqxcb-glx-integration.so
)

require_command() {
    if ! command -v "$1" >/dev/null 2>&1; then
        MISSING_COMMANDS+=("$1")
    fi
}

require_file() {
    if [ ! -f "$1" ]; then
        MISSING_PAYLOAD_FILES+=("$1")
    fi
}

check_frontend_qml_bundle() {
    local module
    local missing=0

    for module in "${REQUIRED_FRONTEND_QML_MODULES[@]}"; do
        if [ -d "$FRONTEND_PRIVATE_QML_DIR/$module" ]; then
            continue
        fi
        echo "==> ERROR: Bad frontend runtime payload: missing bundled Qt QML module: $module" >&2
        missing=1
    done

    if [ "$missing" -ne 0 ]; then
        echo "==> Rebuild the installer on a host that has the required frontend QML modules." >&2
        exit 1
    fi
}

check_frontend_plugin_bundle() {
    local plugin_file
    local missing=0

    for plugin_file in "${REQUIRED_FRONTEND_QT_PLUGIN_FILES[@]}"; do
        if [ -f "$SCRIPT_DIR/lib/lymalink/frontend/plugins/$plugin_file" ]; then
            continue
        fi
        echo "==> ERROR: Bad frontend runtime payload: missing bundled Qt plugin: $plugin_file" >&2
        missing=1
    done

    if [ "$missing" -ne 0 ]; then
        echo "==> Rebuild the installer on a host that has the required Qt plugins." >&2
        exit 1
    fi

    if [ ! -f "$SCRIPT_DIR/lib/lymalink/frontend/plugins/platformthemes/libqxdgdesktopportal.so" ] &&
        [ ! -f "$SCRIPT_DIR/lib/lymalink/frontend/plugins/platformthemes/libqgtk3.so" ]; then
        echo "==> ERROR: Bad frontend runtime payload: missing native Qt platform theme plugin." >&2
        echo "==> Rebuild the installer on a host with Qt xdg-desktop-portal or GTK platform theme support." >&2
        exit 1
    fi
}

add_unique_missing_library() {
    local library="$1"
    local existing

    for existing in "${MISSING_LIBRARIES[@]}"; do
        if [ "$existing" = "$library" ]; then
            return
        fi
    done

    MISSING_LIBRARIES+=("$library")
}

add_missing_libraries_from_ldd() {
    local output="$1"
    local line
    local library

    while IFS= read -r line; do
        case "$line" in
            *"not found"*)
                library="$(printf '%s\n' "$line" | sed -E 's/^[[:space:]]*([^[:space:]]+)[[:space:]]+=>[[:space:]]+not found.*$/\1/')"
                if [ -n "$library" ] && [ "$library" != "$line" ]; then
                    add_unique_missing_library "$library"
                fi
                ;;
        esac
    done <<< "$output"
}

add_unique_incompatible_library() {
    local library="$1"
    local existing

    for existing in "${INCOMPATIBLE_LIBRARIES[@]}"; do
        if [ "$existing" = "$library" ]; then
            return
        fi
    done

    INCOMPATIBLE_LIBRARIES+=("$library")
}

add_incompatible_libraries_from_ldd() {
    local output="$1"
    local line
    local library

    while IFS= read -r line; do
        case "$line" in
            *"version "*" not found"*)
                library="$(printf '%s\n' "$line" | sed -nE 's/^.*\/([^/]+): version `[^`]+'"'"' not found.*$/\1/p')"
                if [ -n "$library" ]; then
                    add_unique_incompatible_library "$library"
                fi
                ;;
        esac
    done <<< "$output"
}

detect_package_manager() {
    local distro_id=""
    local distro_like=""

    if [ -r /etc/os-release ]; then
        # shellcheck disable=SC1091
        . /etc/os-release
        distro_id="${ID:-}"
        distro_like="${ID_LIKE:-}"
    fi

    case " $distro_id $distro_like " in
        *" debian "*|*" ubuntu "*) echo "apt" ;;
        *" fedora "*|*" rhel "*|*" centos "*) echo "dnf" ;;
        *" opensuse "*|*" opensuse-"*|*" suse "*) echo "zypper" ;;
        *" arch "*) echo "pacman" ;;
        *) echo "" ;;
    esac
}

package_for_library() {
    local package_manager="$1"
    local library="$2"
    local icu_version

    case "$package_manager:$library" in
        apt:libicu*.so.*)
            icu_version="${library##*.so.}"
            if [ -n "$icu_version" ] && [ "$icu_version" != "$library" ]; then
                echo "libicu$icu_version"
            else
                echo "libicu-dev"
            fi
            return
            ;;
        dnf:libicu*.so.*) echo "libicu"; return ;;
        zypper:libicu*.so.*) echo "libicu"; return ;;
        pacman:libicu*.so.*) echo "icu"; return ;;
    esac

    case "$package_manager:$library" in
        apt:libQt6Core.so.*) echo "libqt6core6"; return ;;
        apt:libQt6DBus.so.*) echo "libqt6dbus6"; return ;;
        apt:libQt6Gui.so.*) echo "libqt6gui6"; return ;;
        apt:libQt6Network.so.*) echo "libqt6network6"; return ;;
        apt:libQt6OpenGL.so.*) echo "libqt6opengl6"; return ;;
        apt:libQt6Qml*.so.*) echo "libqt6qml6"; return ;;
        apt:libQt6Quick*.so.*) echo "libqt6quick6"; return ;;
        apt:libQt6Sql.so.*) echo "libqt6sql6"; return ;;
        apt:libQt6Widgets.so.*) echo "libqt6widgets6"; return ;;
        apt:libvulkan.so.*) echo "libvulkan1" ;;
        apt:libGL.so.*) echo "libgl1" ;;
        apt:libOpenGL.so.*) echo "libopengl0" ;;
        apt:libEGL.so.*) echo "libegl1" ;;
        apt:libX11.so.*) echo "libx11-6" ;;
        apt:libxcb.so.*) echo "libxcb1" ;;
        apt:libxcb-cursor.so.*) echo "libxcb-cursor0" ;;
        apt:libxcb-icccm.so.*) echo "libxcb-icccm4" ;;
        apt:libxcb-image.so.*) echo "libxcb-image0" ;;
        apt:libxcb-xinput.so.*) echo "libxcb-xinput0" ;;
        apt:libxcb-xkb.so.*) echo "libxcb-xkb1" ;;
        apt:libxcb-keysyms.so.*) echo "libxcb-keysyms1" ;;
        apt:libxcb-randr.so.*) echo "libxcb-randr0" ;;
        apt:libxcb-render.so.*) echo "libxcb-render0" ;;
        apt:libxcb-render-util.so.*) echo "libxcb-render-util0" ;;
        apt:libxcb-shape.so.*) echo "libxcb-shape0" ;;
        apt:libxcb-shm.so.*) echo "libxcb-shm0" ;;
        apt:libxcb-xfixes.so.*) echo "libxcb-xfixes0" ;;
        apt:libxcb-xinerama.so.*) echo "libxcb-xinerama0" ;;
        apt:libxkbcommon.so.*) echo "libxkbcommon0" ;;
        apt:libxkbcommon-x11.so.*) echo "libxkbcommon-x11-0" ;;
        apt:libwayland-client.so.*) echo "libwayland-client0" ;;
        apt:libdbus-1.so.*) echo "libdbus-1-3" ;;
        apt:libsystemd.so.*) echo "libsystemd0" ;;
        apt:libsdbus-c++.so.*) echo "libsdbus-c++2" ;;
        apt:libssl.so.*|apt:libcrypto.so.*) echo "libssl3" ;;
        apt:libb2.so.*) echo "libb2-1" ;;
        apt:libdouble-conversion.so.*) echo "libdouble-conversion3" ;;
        apt:libgtk-3.so.*) echo "libgtk-3-0" ;;
        apt:libgdk_pixbuf-2.0.so.*) echo "libgdk-pixbuf-2.0-0" ;;
        apt:libglib-2.0.so.*|apt:libgobject-2.0.so.*|apt:libgio-2.0.so.*) echo "libglib2.0-0" ;;
        apt:libmd4c.so.*) echo "libmd4c0" ;;
        apt:libpcre2-16.so.*) echo "libpcre2-16-0" ;;
        apt:libstdc++.so.*) echo "libstdc++6" ;;

        dnf:libQt6*.so.*) echo "qt6-qtbase qt6-qtdeclarative qt6-qtquickcontrols2"; return ;;
        dnf:libvulkan.so.*) echo "vulkan-loader" ;;
        dnf:libGL.so.*) echo "libglvnd-glx" ;;
        dnf:libEGL.so.*) echo "libglvnd-egl" ;;
        dnf:libX11.so.*) echo "libX11" ;;
        dnf:libxcb.so.*|dnf:libxcb-randr.so.*|dnf:libxcb-render.so.*|dnf:libxcb-shape.so.*|dnf:libxcb-shm.so.*|dnf:libxcb-xfixes.so.*|dnf:libxcb-xinerama.so.*) echo "libxcb" ;;
        dnf:libxcb-cursor.so.*) echo "xcb-util-cursor" ;;
        dnf:libxcb-icccm.so.*) echo "xcb-util-wm" ;;
        dnf:libxcb-image.so.*) echo "xcb-util-image" ;;
        dnf:libxcb-keysyms.so.*) echo "xcb-util-keysyms" ;;
        dnf:libxcb-render-util.so.*) echo "xcb-util-renderutil" ;;
        dnf:libxkbcommon.so.*) echo "libxkbcommon" ;;
        dnf:libwayland-client.so.*) echo "wayland-libs" ;;
        dnf:libdbus-1.so.*) echo "dbus-libs" ;;
        dnf:libsystemd.so.*) echo "systemd-libs" ;;
        dnf:libsdbus-c++.so.*) echo "sdbus-cpp" ;;
        dnf:libssl.so.*|dnf:libcrypto.so.*) echo "openssl-libs" ;;
        dnf:libgtk-3.so.*) echo "gtk3" ;;
        dnf:libgdk_pixbuf-2.0.so.*) echo "gdk-pixbuf2" ;;
        dnf:libglib-2.0.so.*|dnf:libgobject-2.0.so.*|dnf:libgio-2.0.so.*) echo "glib2" ;;
        dnf:libstdc++.so.*) echo "libstdc++" ;;

        zypper:libQt6*.so.*) echo "libQt6Core6 libQt6Gui6 libQt6Qml6 libQt6Quick6 libqt6-qtdeclarative"; return ;;
        zypper:libvulkan.so.*) echo "libvulkan1" ;;
        zypper:libGL.so.*) echo "Mesa-libGL1" ;;
        zypper:libEGL.so.*) echo "Mesa-libEGL1" ;;
        zypper:libX11.so.*) echo "libX11-6" ;;
        zypper:libxcb.so.*) echo "libxcb1" ;;
        zypper:libxcb-cursor.so.*) echo "libxcb-cursor0" ;;
        zypper:libxcb-icccm.so.*) echo "libxcb-icccm4" ;;
        zypper:libxcb-image.so.*) echo "libxcb-image0" ;;
        zypper:libxcb-keysyms.so.*) echo "libxcb-keysyms1" ;;
        zypper:libxcb-randr.so.*) echo "libxcb-randr0" ;;
        zypper:libxcb-render.so.*) echo "libxcb-render0" ;;
        zypper:libxcb-render-util.so.*) echo "libxcb-render-util0" ;;
        zypper:libxcb-shape.so.*) echo "libxcb-shape0" ;;
        zypper:libxcb-shm.so.*) echo "libxcb-shm0" ;;
        zypper:libxcb-xfixes.so.*) echo "libxcb-xfixes0" ;;
        zypper:libxcb-xinerama.so.*) echo "libxcb-xinerama0" ;;
        zypper:libxkbcommon.so.*) echo "libxkbcommon0" ;;
        zypper:libwayland-client.so.*) echo "libwayland-client0" ;;
        zypper:libdbus-1.so.*) echo "libdbus-1-3" ;;
        zypper:libsystemd.so.*) echo "libsystemd0" ;;
        zypper:libsdbus-c++.so.*) echo "libsdbus-c++2" ;;
        zypper:libssl.so.*|zypper:libcrypto.so.*) echo "libopenssl3" ;;
        zypper:libgtk-3.so.*) echo "gtk3" ;;
        zypper:libgdk_pixbuf-2.0.so.*) echo "gdk-pixbuf" ;;
        zypper:libglib-2.0.so.*|zypper:libgobject-2.0.so.*|zypper:libgio-2.0.so.*) echo "glib2" ;;
        zypper:libstdc++.so.*) echo "libstdc++6" ;;

        pacman:libQt6*.so.*) echo "qt6-base qt6-declarative qt6-quickcontrols2"; return ;;
        pacman:libvulkan.so.*) echo "vulkan-icd-loader" ;;
        pacman:libGL.so.*|pacman:libEGL.so.*) echo "libglvnd" ;;
        pacman:libX11.so.*) echo "libx11" ;;
        pacman:libxcb.so.*|pacman:libxcb-randr.so.*|pacman:libxcb-render.so.*|pacman:libxcb-shape.so.*|pacman:libxcb-shm.so.*|pacman:libxcb-xfixes.so.*|pacman:libxcb-xinerama.so.*) echo "libxcb" ;;
        pacman:libxcb-cursor.so.*) echo "xcb-util-cursor" ;;
        pacman:libxcb-icccm.so.*) echo "xcb-util-wm" ;;
        pacman:libxcb-image.so.*) echo "xcb-util-image" ;;
        pacman:libxcb-keysyms.so.*) echo "xcb-util-keysyms" ;;
        pacman:libxcb-render-util.so.*) echo "xcb-util-renderutil" ;;
        pacman:libxkbcommon.so.*) echo "libxkbcommon" ;;
        pacman:libwayland-client.so.*) echo "wayland" ;;
        pacman:libdbus-1.so.*) echo "dbus" ;;
        pacman:libsystemd.so.*) echo "systemd-libs" ;;
        pacman:libsdbus-c++.so.*) echo "sdbus-cpp" ;;
        pacman:libssl.so.*|pacman:libcrypto.so.*) echo "openssl" ;;
        pacman:libgtk-3.so.*) echo "gtk3" ;;
        pacman:libgdk_pixbuf-2.0.so.*) echo "gdk-pixbuf2" ;;
        pacman:libglib-2.0.so.*|pacman:libgobject-2.0.so.*|pacman:libgio-2.0.so.*) echo "glib2" ;;
        pacman:libstdc++.so.*) echo "gcc-libs" ;;

        *) echo "" ;;
    esac
}

package_for_command() {
    local package_manager="$1"
    local command_name="$2"

    case "$package_manager:$command_name" in
        apt:cp|apt:install|apt:readlink|apt:sort) echo "coreutils" ;;
        apt:find) echo "findutils" ;;
        apt:flatpak) echo "flatpak" ;;
        apt:grep) echo "grep" ;;
        apt:ldd) echo "libc-bin" ;;
        apt:sed) echo "sed" ;;
        apt:systemctl) echo "systemd" ;;

        dnf:cp|dnf:install|dnf:readlink|dnf:sort) echo "coreutils" ;;
        dnf:find) echo "findutils" ;;
        dnf:flatpak) echo "flatpak" ;;
        dnf:grep) echo "grep" ;;
        dnf:ldd) echo "glibc" ;;
        dnf:sed) echo "sed" ;;
        dnf:systemctl) echo "systemd" ;;

        zypper:cp|zypper:install|zypper:readlink|zypper:sort) echo "coreutils" ;;
        zypper:find) echo "findutils" ;;
        zypper:flatpak) echo "flatpak" ;;
        zypper:grep) echo "grep" ;;
        zypper:ldd) echo "glibc" ;;
        zypper:sed) echo "sed" ;;
        zypper:systemctl) echo "systemd" ;;

        pacman:cp|pacman:install|pacman:readlink|pacman:sort) echo "coreutils" ;;
        pacman:find) echo "findutils" ;;
        pacman:flatpak) echo "flatpak" ;;
        pacman:grep) echo "grep" ;;
        pacman:ldd) echo "glibc" ;;
        pacman:sed) echo "sed" ;;
        pacman:systemctl) echo "systemd" ;;

        *) echo "" ;;
    esac
}

add_unique_package() {
    local package="$1"
    shift
    local existing

    for existing in "$@"; do
        if [ "$existing" = "$package" ]; then
            return 1
        fi
    done

    return 0
}

print_package_install_help() {
    local package_manager="$1"
    shift
    local packages=("$@")

    if [ -z "$package_manager" ] || [ "${#packages[@]}" -eq 0 ]; then
        echo "==> Please install the missing dependencies with your system package manager, then run the installer again." >&2
        return
    fi

    echo "==> Possible package install command:" >&2
    case "$package_manager" in
        apt)
            echo "    sudo apt update" >&2
            echo "    sudo apt install ${packages[*]}" >&2
            ;;
        dnf)
            echo "    sudo dnf install ${packages[*]}" >&2
            ;;
        zypper)
            echo "    sudo zypper install ${packages[*]}" >&2
            ;;
        pacman)
            echo "    sudo pacman -S ${packages[*]}" >&2
            ;;
    esac
}

print_missing_command_help() {
    local package_manager
    local packages=()
    local command_name
    local package

    package_manager="$(detect_package_manager)"

    echo "==> Missing required commands:" >&2
    for command_name in "${MISSING_COMMANDS[@]}"; do
        echo "    $command_name" >&2
        package="$(package_for_command "$package_manager" "$command_name")"
        if [ -n "$package" ] && add_unique_package "$package" "${packages[@]}"; then
            packages+=("$package")
        fi
    done

    print_package_install_help "$package_manager" "${packages[@]}"
}

print_missing_payload_help() {
    local payload_file

    echo "==> Missing required installer payload files:" >&2
    for payload_file in "${MISSING_PAYLOAD_FILES[@]}"; do
        echo "    $payload_file" >&2
    done
    echo "==> The installer payload is incomplete. Rebuild or re-download the installer, then run it again." >&2
}

fail_if_preflight_missing() {
    local failed=0

    if [ "${#MISSING_COMMANDS[@]}" -ne 0 ]; then
        print_missing_command_help
        failed=1
    fi

    if [ "${#MISSING_PAYLOAD_FILES[@]}" -ne 0 ]; then
        if [ "$failed" -ne 0 ]; then
            echo "" >&2
        fi
        print_missing_payload_help
        failed=1
    fi

    if [ "$failed" -ne 0 ]; then
        exit 1
    fi
}

print_missing_library_help() {
    local package_manager
    local packages=()
    local library
    local package

    package_manager="$(detect_package_manager)"

    echo "==> Missing libraries detected:" >&2
    for library in "${MISSING_LIBRARIES[@]}"; do
        echo "    $library" >&2
        package="$(package_for_library "$package_manager" "$library")"
        if [ -n "$package" ] && add_unique_package "$package" "${packages[@]}"; then
            packages+=("$package")
        fi
    done

    if [ -z "$package_manager" ] || [ "${#packages[@]}" -eq 0 ]; then
        echo "==> Please use your system's package manager to search for and install the missing dependencies listed above." >&2
        return
    fi

    echo "==> Possible package install command:" >&2
    case "$package_manager" in
        apt)
            echo "    sudo apt update" >&2
            echo "    sudo apt install ${packages[*]}" >&2
            ;;
        dnf)
            echo "    sudo dnf install ${packages[*]}" >&2
            ;;
        zypper)
            echo "    sudo zypper install ${packages[*]}" >&2
            ;;
        pacman)
            echo "    sudo pacman -S ${packages[*]}" >&2
            ;;
    esac
}

print_incompatible_library_help() {
    local library

    echo "==> Incompatible runtime libraries detected:" >&2
    for library in "${INCOMPATIBLE_LIBRARIES[@]}"; do
        echo "    $library" >&2
    done
    echo "==> This usually means the installer was built on a different or incompatible distribution release." >&2
    echo "==> Use an installer built on the same or compatible distribution release." >&2
}

qt_version_from_library_path() {
    local path="$1"
    local name

    name="${path##*/}"
    printf '%s\n' "$name" | sed -nE 's/^libQt6Core\.so\.([0-9]+(\.[0-9]+){1,2}).*$/\1/p'
}

check_frontend_qt_bundle() {
    local build_version
    local qt_core_path
    local bundled_version

    build_version="$(tr -d '[:space:]' < "$FRONTEND_QT_BUILD_VERSION_FILE")"
    if [ -z "$build_version" ]; then
        echo "==> ERROR: Bad frontend runtime payload: Qt build version metadata is empty." >&2
        exit 1
    fi

    qt_core_path="$FRONTEND_PRIVATE_LIB_DIR/libQt6Core.so.6"
    if [ ! -e "$qt_core_path" ]; then
        echo "==> ERROR: Bad frontend runtime payload: bundled libQt6Core.so.6 not found." >&2
        exit 1
    fi

    qt_core_path="$(readlink -f "$qt_core_path")"
    bundled_version="$(qt_version_from_library_path "$qt_core_path")"
    if [ -z "$bundled_version" ]; then
        echo "==> ERROR: Bad frontend runtime payload: could not parse bundled Qt version from $qt_core_path." >&2
        exit 1
    fi

    if [ "$bundled_version" != "$build_version" ]; then
        echo "==> ERROR: Bad frontend runtime payload: bundled Qt version does not match build metadata." >&2
        echo "==> Build Qt:  $build_version" >&2
        echo "==> Bundle Qt: $bundled_version" >&2
        echo "==> Rebuild the installer on a host with a consistent Qt runtime." >&2
        exit 1
    fi
}

check_ldd_output() {
    local artifact="$1"
    local output="$2"
    local missing=0

    if printf '%s\n' "$output" | grep -q 'version `.* not found'; then
        echo "==> ERROR: Incompatible runtime libraries required by $artifact:" >&2
        printf '%s\n' "$output" | grep 'version `.* not found' >&2
        add_incompatible_libraries_from_ldd "$output"
        missing=1
    fi

    if printf '%s\n' "$output" | grep -q '=>[[:space:]]*not found'; then
        echo "==> ERROR: Missing runtime libraries required by $artifact:" >&2
        printf '%s\n' "$output" | grep '=>[[:space:]]*not found' >&2
        add_missing_libraries_from_ldd "$output"
        missing=1
    fi

    return "$missing"
}

check_frontend_ldd_output() {
    local artifact="$1"
    local output="$2"
    local missing=0

    if printf '%s\n' "$output" | grep -q 'version `.* not found'; then
        echo "==> ERROR: Incompatible runtime libraries required by frontend artifact $artifact:" >&2
        printf '%s\n' "$output" | grep 'version `.* not found' >&2
        add_incompatible_libraries_from_ldd "$output"
        missing=1
    fi

    if printf '%s\n' "$output" | grep -q 'libQt6.*=>[[:space:]]*not found'; then
        echo "==> ERROR: Bad frontend runtime payload: missing bundled Qt libraries for $artifact:" >&2
        printf '%s\n' "$output" | grep 'libQt6.*=>[[:space:]]*not found' >&2
        missing=1
    fi

    if printf '%s\n' "$output" | grep -v 'libQt6' | grep -q '=>[[:space:]]*not found'; then
        echo "==> ERROR: Missing frontend runtime libraries required by $artifact:" >&2
        printf '%s\n' "$output" | grep -v 'libQt6' | grep '=>[[:space:]]*not found' >&2
        add_missing_libraries_from_ldd "$(printf '%s\n' "$output" | grep -v 'libQt6')"
        missing=1
    fi

    return "$missing"
}

check_frontend_elf_dependencies() {
    local artifact
    local output
    local missing=0

    while IFS= read -r artifact; do
        if ! output="$(LD_LIBRARY_PATH="$FRONTEND_PRIVATE_LIB_DIR" ldd "$artifact" 2>&1)"; then
            if ! printf '%s\n' "$output" | grep -Eq 'version `.* not found|=>[[:space:]]*not found'; then
                echo "==> ERROR: Unable to inspect frontend runtime dependencies for $artifact" >&2
                printf '%s\n' "$output" >&2
                exit 1
            fi
        fi

        if ! check_frontend_ldd_output "$artifact" "$output"; then
            missing=1
        fi
    done < <(find "$SCRIPT_DIR/lib/lymalink/frontend" -type f \( -name 'Lymalink' -o -name '*.so' -o -name '*.so.*' \) | sort)

    if [ "$missing" -ne 0 ]; then
        if [ "${#INCOMPATIBLE_LIBRARIES[@]}" -ne 0 ]; then
            print_incompatible_library_help
        fi
        if [ "${#MISSING_LIBRARIES[@]}" -ne 0 ]; then
            print_missing_library_help
        fi
        echo "==> Fix the reported frontend runtime dependency issue and run the installer again." >&2
        exit 1
    fi
}

check_native_elf_dependencies() {
    local artifact
    local output
    local missing=0

    for artifact in \
        "$SCRIPT_DIR/bin/lymalinkd" \
        "$SCRIPT_DIR/lib/lymalink-overlay.so" \
        "$SCRIPT_DIR/lib/lymalink-overlay-opengl.so" \
        "$SCRIPT_DIR/lib/lymalink-overlay-preloader.so"; do
        if ! output="$(ldd "$artifact" 2>&1)"; then
            if ! printf '%s\n' "$output" | grep -Eq 'version `.* not found|=>[[:space:]]*not found'; then
                echo "==> ERROR: Unable to inspect runtime dependencies for $artifact" >&2
                printf '%s\n' "$output" >&2
                exit 1
            fi
        fi

        if ! check_ldd_output "$artifact" "$output"; then
            missing=1
        fi
    done

    if [ "$missing" -ne 0 ]; then
        if [ "${#INCOMPATIBLE_LIBRARIES[@]}" -ne 0 ]; then
            print_incompatible_library_help
        fi
        if [ "${#MISSING_LIBRARIES[@]}" -ne 0 ]; then
            print_missing_library_help
        fi
        echo "==> Fix the reported runtime dependency issue and run the installer again." >&2
        exit 1
    fi
}

is_gnome_session() {
    case ":${XDG_CURRENT_DESKTOP:-}:${DESKTOP_SESSION:-}:${GNOME_DESKTOP_SESSION_ID:-}:" in
        *GNOME*|*gnome*) return 0 ;;
        *) return 1 ;;
    esac
}

gnome_appindicator_enabled() {
    local extension_id
    local enabled_extensions

    if command -v gsettings >/dev/null 2>&1; then
        enabled_extensions="$(gsettings get org.gnome.shell enabled-extensions 2>/dev/null || true)"
        for extension_id in appindicatorsupport@rgcjonas.gmail.com ubuntu-appindicators@ubuntu.com; do
            if printf '%s\n' "$enabled_extensions" | grep -Fq "$extension_id"; then
                return 0
            fi
        done
    fi

    return 1
}

gnome_appindicator_installed() {
    local extension_id
    local extension_dir

    for extension_id in appindicatorsupport@rgcjonas.gmail.com ubuntu-appindicators@ubuntu.com; do
        for extension_dir in \
            "$HOME/.local/share/gnome-shell/extensions/$extension_id" \
            "/usr/share/gnome-shell/extensions/$extension_id"; do
            if [ -d "$extension_dir" ]; then
                return 0
            fi
        done
    done

    if command -v dpkg-query >/dev/null 2>&1 &&
        dpkg-query -W -f='${Status}' gnome-shell-extension-appindicator 2>/dev/null | grep -Fq 'install ok installed'; then
        return 0
    fi

    if command -v rpm >/dev/null 2>&1 &&
        rpm -q gnome-shell-extension-appindicator >/dev/null 2>&1; then
        return 0
    fi

    if command -v pacman >/dev/null 2>&1 &&
        pacman -Q gnome-shell-extension-appindicator >/dev/null 2>&1; then
        return 0
    fi

    return 1
}

print_gnome_tray_note() {
    local package_manager

    if ! is_gnome_session || gnome_appindicator_enabled; then
        return
    fi

    if gnome_appindicator_installed; then
        echo "==> NOTE: GNOME AppIndicator/KStatusNotifier extension is installed but not enabled."
        echo "    Enable it in GNOME Extensions, then log out/in if the tray icon is still missing."
        return
    fi

    echo "==> NOTE: GNOME AppIndicator/KStatusNotifier extension was not detected."
    echo "    Lymalink tray icon needs this extension on GNOME."

    package_manager="$(detect_package_manager)"
    case "$package_manager" in
        apt)
            echo "    Suggested package: sudo apt install gnome-shell-extension-appindicator"
            ;;
        dnf)
            echo "    Suggested package: sudo dnf install gnome-shell-extension-appindicator"
            ;;
        zypper)
            echo "    Suggested package: sudo zypper install gnome-shell-extension-appindicator"
            ;;
        pacman)
            echo "    Install your distro/AUR package for GNOME AppIndicator support, then enable the extension."
            ;;
        *)
            echo "    Install your distro package for GNOME AppIndicator support, then enable the extension."
            ;;
    esac
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

for command_name in cp find flatpak grep install ldd readlink sed sort systemctl; do
    require_command "$command_name"
done

for payload_file in \
    "$SCRIPT_DIR/bin/Lymalink" \
    "$FRONTEND_BINARY" \
    "$FRONTEND_QT_BUILD_VERSION_FILE" \
    "$FLATPAK_BUNDLE" \
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
    MISSING_PAYLOAD_FILES+=("$SCRIPT_DIR/share/Lymalink/sounds/*.ogg")
fi

fail_if_preflight_missing

check_frontend_qt_bundle
check_frontend_qml_bundle
check_frontend_plugin_bundle
check_frontend_elf_dependencies
check_native_elf_dependencies

echo "==> Installing Lymalink files..."
rm -rf "$SOUND_DIR"
rm -rf "$FRONTEND_DIR"
mkdir -p "$BIN_DIR" "$LIB_DIR" "$APP_LIB_DIR" "$FRONTEND_DIR" "$SERVICE_DIR" "$VULKAN_DIR" "$ICON_DIR" "$APPLICATION_DIR" "$SOUND_DIR"
rm -f "$APP_DATA_DIR/.installer-sounds" "$APP_DATA_DIR/.backend-sounds"
install -m 755 "$SCRIPT_DIR/bin/Lymalink" "$BIN_DIR/Lymalink"
install -m 755 "$SCRIPT_DIR/bin/lymalinkd" "$BIN_DIR/lymalinkd"
install -m 755 "$SCRIPT_DIR/bin/lymalink-overlay" "$BIN_DIR/lymalink-overlay"
install -m 755 "$SCRIPT_DIR/uninstall.sh" "$BIN_DIR/uninstall-lymalink"
cp -a "$SCRIPT_DIR/lib/lymalink/frontend/." "$FRONTEND_DIR/"
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
flatpak install --user --bundle --reinstall -y --noninteractive "$FLATPAK_BUNDLE"

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
print_gnome_tray_note
