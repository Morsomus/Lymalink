#!/usr/bin/env bash
#########################################################
# File: build.sh (installer)
# Date: 2026-05-31
# Author: Morsomus
# Copyright: see /LICENSE
# Description: Builds the self-extracting Lymalink installer
#########################################################

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="$(cd "$SCRIPT_DIR/../.." && pwd)"

# When enabled, build output is diverted to /tmp (RAMfs)
BUILD_TO_TMP=0
if [ "$BUILD_TO_TMP" -eq 1 ] 2>/dev/null; then
    BUILD_DIR="/tmp/lymalink-installer-build"
else
    BUILD_DIR="$SCRIPT_DIR/build"
fi

RELEASE_DIR="$BUILD_DIR/lymalink-release"
FRONTEND_BUNDLE_DIR="$RELEASE_DIR/lib/lymalink/frontend"
FLATPAK_WORK_DIR="$BUILD_DIR/flatpak-work"
FLATPAK_EXTENSION_DIR="$FLATPAK_WORK_DIR/extension"
FLATPAK_REPO_DIR="$FLATPAK_WORK_DIR/repo"

VERSION="$(tr -d '[:space:]' < "$ROOT_DIR/VERSION")"
ARCH="x86_64"

FRONTEND_BINARY="$ROOT_DIR/frontend/build/release/bin/Lymalink"
BACKEND_BUILD_BIN_DIR="$ROOT_DIR/backend/build/release/bin"
OVERLAY_BUILD_BIN_DIR="$ROOT_DIR/backend-overlay/build/release/bin"
OVERLAY_BUILD_BIN_DIR_I386="$ROOT_DIR/backend-overlay/build/release-i386/bin"
OVERLAY_FLATPAK_BUILD_BIN_DIR="$ROOT_DIR/backend-overlay/build/flatpak/release/bin"
OVERLAY_FLATPAK_BUILD_BIN_DIR_I386="$ROOT_DIR/backend-overlay/build/flatpak/release-i386/bin"
FLATPAK_EXTENSION_ID="org.freedesktop.Platform.VulkanLayer.lymalink"
FLATPAK_EXTENSION_BRANCH="25.08"
FLATPAK_BUNDLE="$RELEASE_DIR/flatpak/${FLATPAK_EXTENSION_ID}.flatpak"
MIN_QT_VERSION="6.8.0"
FRONTEND_QML_MODULES=(
    QtCore
    Qt/labs/folderlistmodel
    Qt5Compat/GraphicalEffects
    QtQml/Models
    QtQuick
    QtQuick/Controls
    QtQuick/Controls/Basic
    QtQuick/Controls/Basic/impl
    QtQuick/Controls/Fusion
    QtQuick/Controls/Fusion/impl
    QtQuick/Controls/impl
    QtQuick/Dialogs
    QtQuick/Dialogs/quickimpl
    QtQuick/Effects
    QtQuick/Layouts
    QtQuick/Shapes
    QtQuick/Templates
    QtQuick/Window
)
REQUIRED_FRONTEND_QML_MODULES=(
    Qt/labs/folderlistmodel
    Qt5Compat/GraphicalEffects
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
FRONTEND_QT_PLUGIN_FILES=(
    platforms/libqwayland.so
    platforms/libqxcb.so
    platformthemes/libqgtk3.so
    platformthemes/libqxdgdesktopportal.so
    imageformats/libqgif.so
    imageformats/libqico.so
    imageformats/libqjpeg.so
    imageformats/libqsvg.so
    imageformats/libqwebp.so
    tls/libqcertonlybackend.so
    tls/libqopensslbackend.so
    sqldrivers/libqsqlite.so
    xcbglintegrations/libqxcb-egl-integration.so
    xcbglintegrations/libqxcb-glx-integration.so
)
REQUIRED_FRONTEND_QT_PLUGIN_FILES=(
    platforms/libqwayland.so
    platforms/libqxcb.so
    sqldrivers/libqsqlite.so
    xcbglintegrations/libqxcb-egl-integration.so
    xcbglintegrations/libqxcb-glx-integration.so
)

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

require_command() {
    if ! command -v "$1" >/dev/null 2>&1; then
        echo "==> ERROR: Required command not found: $1" >&2
        exit 1
    fi
}

##############################################################################

require_file() {
    if [ ! -f "$1" ]; then
        echo "==> ERROR: Required build artifact not found: $1" >&2
        exit 1
    fi
}

##############################################################################

validate_elf_class() {
    local artifact="$1"
    local expected_class="$2"
    local description="$3"
    local file_output

    file_output="$(file -b "$artifact")"
    if ! printf '%s\n' "$file_output" | grep -q "ELF ${expected_class}-bit"; then
        echo "==> ERROR: $description has wrong ELF class: $artifact" >&2
        echo "==> Expected: ELF ${expected_class}-bit" >&2
        echo "==> Actual:   $file_output" >&2
        exit 1
    fi
}

##############################################################################

validate_overlay_artifact_set() {
    local directory="$1"
    local expected_class="$2"
    local description="$3"
    local library

    for library in lymalink-overlay.so lymalink-overlay-opengl.so lymalink-overlay-preloader.so; do
        require_file "$directory/$library"
        validate_elf_class "$directory/$library" "$expected_class" "$description $library"
    done
}

##############################################################################

detect_build_host_tag() {
    local distro_id=""
    local distro_version=""
    local host_tag=""

    # Read distribution metadata from os-release when available
    if [ -r /etc/os-release ]; then
        # shellcheck disable=SC1091
        . /etc/os-release
        distro_id="${ID:-}"
        distro_version="${VERSION_ID:-}"
    fi

    # Build a host identifier from distro name and version
    if [ -n "$distro_id" ] && [ -n "$distro_version" ]; then
        host_tag="${distro_id}${distro_version}"
    elif [ -n "$distro_id" ]; then
        host_tag="$distro_id"
    else
        host_tag="linux"
    fi

    # Remove characters that are unsafe for file or directory names
    printf '%s\n' "$host_tag" | sed 's/[^A-Za-z0-9._-]//g'
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
    echo "==> Build the installer on a host with Qt >= $MIN_QT_VERSION." >&2
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

write_vulkan_manifest() {
    local destination="$1"
    local library_path="$2"
    local layer_name="${3:-VK_LAYER_LYMALINK_overlay}"

    cat > "$destination" <<EOF
{
    "file_format_version": "1.0.0",
    "layer": {
        "name": "${layer_name}",
        "type": "GLOBAL",
        "library_path": "${library_path}",
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

copy_frontend_icu_runtime() {
    local destination="$1"
    local artifact
    local output
    local line
    local library_path
    local copied=0

    # Create ICU runtime output dir
    mkdir -p "$destination"

    # Check app and Qt libs for ICU deps
    while IFS= read -r artifact; do
        if ! output="$(ldd "$artifact" 2>/dev/null)"; then
            continue
        fi

        # Read each ldd dependency line
        while IFS= read -r line; do
            case "$line" in
                *"libicu"*.so.*"=>"*)
                    # Extract resolved ICU library path
                    library_path="$(printf '%s\n' "$line" | sed -E 's/^.*=>[[:space:]]*([^[:space:]]+)[[:space:]]+\(.*$/\1/')"

                    # Copy this ICU library and its symlink family
                    if [ -n "$library_path" ] && [ "$library_path" != "$line" ] && [ -f "$library_path" ]; then
                        copy_library_family "$library_path" "$destination"
                        copied=1
                    fi
                    ;;
            esac
        done <<< "$output"
    done < <(find "$FRONTEND_BUNDLE_DIR" -type f \( -name 'Lymalink' -o -name 'libQt6*.so*' \) | sort)

    # Remove dir if no ICU libs were found
    if [ "$copied" -eq 0 ]; then
        rmdir "$destination" 2>/dev/null || true
        echo "==> WARNING: No frontend ICU runtime libraries were detected to bundle." >&2
    fi
}

##############################################################################

copy_library_family() {
    local library_path="$1"
    local destination="$2"
    local library_dir
    local library_name
    local library_stem
    local candidate
    local destination_path

    # Ignore empty or missing library paths
    if [ -z "$library_path" ] || [ ! -f "$library_path" ]; then
        return
    fi

    # Split library path into dir/name/stem
    mkdir -p "$destination"
    library_dir="$(dirname "$library_path")"
    library_name="$(basename "$library_path")"
    library_stem="${library_name%%.so*}"

    # Copy all versions/symlinks for this library
    for candidate in "$library_dir/$library_stem".so*; do
        if [ -e "$candidate" ]; then
            destination_path="$destination/$(basename "$candidate")"

            # Do not overwrite existing files or symlinks.
            if [ -e "$destination_path" ] || [ -L "$destination_path" ]; then
                continue
            fi
            cp -a "$candidate" "$destination/"
        fi
    done
}

##############################################################################

copy_frontend_qt_library_closure() {
    local qt_lib_dir="$1"
    local artifact
    local output
    local line
    local library_path
    local library_name
    local library_stem
    local candidate
    local before_count
    local after_count
    local pass=0

    # Ensure bundled Qt lib dir exists
    mkdir -p "$FRONTEND_BUNDLE_DIR/lib"

    # Repeat until no new Qt libs are added
    while :; do
        pass=$((pass + 1))
        before_count="$(find "$FRONTEND_BUNDLE_DIR/lib" -maxdepth 1 -type f -o -type l | wc -l)"

        # Scan executable and bundled libs for Qt deps
        while IFS= read -r artifact; do
            if ! output="$(LD_LIBRARY_PATH="$qt_lib_dir${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}" ldd "$artifact" 2>/dev/null)"; then
                continue
            fi

            # Read each ldd dependency line
            while IFS= read -r line; do
                case "$line" in
                    *"libQt6"*.so.*"=>"*)
                        # Extract resolved Qt library path
                        library_path="$(printf '%s\n' "$line" | sed -E 's/^.*=>[[:space:]]*([^[:space:]]+)[[:space:]]+\(.*$/\1/')"

                        # Copy matching Qt library family from Qt lib dir
                        library_name="$(basename "$library_path")"
                        library_stem="${library_name%%.so*}"
                        for candidate in "$qt_lib_dir/$library_stem".so*; do
                            if [ -e "$candidate" ]; then
                                copy_library_family "$candidate" "$FRONTEND_BUNDLE_DIR/lib"
                            fi
                        done
                        ;;
                esac
            done <<< "$output"
        done < <(find "$FRONTEND_BUNDLE_DIR" -type f \( -name 'Lymalink' -o -name '*.so' -o -name '*.so.*' \) | sort)

        # Stop when closure stops growing or pass limit (8) is hit
        after_count="$(find "$FRONTEND_BUNDLE_DIR/lib" -maxdepth 1 -type f -o -type l | wc -l)"
        if [ "$after_count" -eq "$before_count" ] || [ "$pass" -ge 8 ]; then
            break
        fi
    done

    # Require QtCore as a basic sanity check
    if [ ! -e "$FRONTEND_BUNDLE_DIR/lib/libQt6Core.so.6" ]; then
        echo "==> ERROR: Bundled frontend Qt closure is missing libQt6Core.so.6." >&2
        exit 1
    fi
}

##############################################################################

copy_qml_module_files_only() {
    local source_dir="$1"
    local destination_dir="$2"
    local source_file

    # Copy only top-level files/symlinks
    mkdir -p "$destination_dir"
    for source_file in "$source_dir"/*; do
        if [ -f "$source_file" ] || [ -L "$source_file" ]; then
            cp -a "$source_file" "$destination_dir/"
        fi
    done
}

##############################################################################

copy_frontend_qt_runtime() {
    local qmake_path="$1"
    local qt_lib_dir
    local qt_plugin_dir
    local qt_qml_dir
    local plugin_file
    local module
    local source_dir
    local source_file
    local parent_module

    echo "==> Bundling frontend Qt runtime..."

    # Query Qt install paths from qmake
    qt_lib_dir="$("$qmake_path" -query QT_INSTALL_LIBS)"
    qt_plugin_dir="$("$qmake_path" -query QT_INSTALL_PLUGINS)"
    qt_qml_dir="$("$qmake_path" -query QT_INSTALL_QML)"

    # Require Qt shared libraries
    if [ ! -d "$qt_lib_dir" ] || ! compgen -G "$qt_lib_dir/libQt6*.so*" >/dev/null; then
        echo "==> ERROR: Could not find Qt runtime libraries in: $qt_lib_dir" >&2
        exit 1
    fi

    # Copy selected Qt plugins
    if [ -d "$qt_plugin_dir" ]; then
        for plugin_file in "${FRONTEND_QT_PLUGIN_FILES[@]}"; do
            source_file="$qt_plugin_dir/$plugin_file"
            if [ -f "$source_file" ]; then
                echo "==> Copying Qt plugin: $plugin_file"
                mkdir -p "$FRONTEND_BUNDLE_DIR/plugins/$(dirname "$plugin_file")"
                cp -a "$source_file" "$FRONTEND_BUNDLE_DIR/plugins/$plugin_file"
            else
                echo "==> WARNING: Qt plugin not found: $source_file"
            fi
        done
    else
        echo "==> WARNING: Qt plugin directory not found: $qt_plugin_dir"
    fi

    # Copy selected QML modules
    if [ -d "$qt_qml_dir" ]; then
        mkdir -p "$FRONTEND_BUNDLE_DIR/qml"
        for module in "${FRONTEND_QML_MODULES[@]}"; do
            source_dir="$qt_qml_dir/$module"
            if [ -d "$source_dir" ]; then
                echo "==> Copying Qt QML module: $module"
                mkdir -p "$FRONTEND_BUNDLE_DIR/qml/$module"

                # Avoid recursively copying large base QML modules
                case "$module" in
                    QtQuick|QtQuick/Controls|QtQuick/Dialogs)
                        copy_qml_module_files_only "$source_dir" "$FRONTEND_BUNDLE_DIR/qml/$module"
                        ;;
                    *)
                        cp -a "$source_dir/." "$FRONTEND_BUNDLE_DIR/qml/$module/"
                        ;;
                esac

                # Copy parent qmldir files for nested modules
                parent_module="$module"
                while [[ "$parent_module" == */* ]]; do
                    parent_module="${parent_module%/*}"
                    if [ -f "$qt_qml_dir/$parent_module/qmldir" ]; then
                        mkdir -p "$FRONTEND_BUNDLE_DIR/qml/$parent_module"
                        cp -a "$qt_qml_dir/$parent_module/qmldir" "$FRONTEND_BUNDLE_DIR/qml/$parent_module/qmldir"
                    fi
                done
            else
                echo "==> WARNING: Qt QML module not found on build host: $module" >&2
            fi
        done
    else
        echo "==> WARNING: Qt QML import directory not found: $qt_qml_dir" >&2
    fi

    # Copy Qt libraries needed by the frontend
    echo "==> Copying frontend Qt library closure..."
    copy_frontend_qt_library_closure "$qt_lib_dir"
}

##############################################################################

validate_frontend_qml_bundle() {
    local module
    local missing=0

    # Check each required QML module exists
    for module in "${REQUIRED_FRONTEND_QML_MODULES[@]}"; do
        if [ -d "$FRONTEND_BUNDLE_DIR/qml/$module" ]; then
            continue
        fi
        echo "==> ERROR: Frontend bundle is missing required Qt QML module: $module" >&2
        missing=1
    done

    # Fail if any required QML module is missing
    if [ "$missing" -ne 0 ]; then
        echo "==> Install the missing Qt QML package for this build host and rebuild." >&2
        exit 1
    fi
}

##############################################################################

validate_frontend_plugin_bundle() {
    local plugin_file
    local missing=0

    # Check each required Qt plugin exists
    for plugin_file in "${REQUIRED_FRONTEND_QT_PLUGIN_FILES[@]}"; do
        if [ -f "$FRONTEND_BUNDLE_DIR/plugins/$plugin_file" ]; then
            continue
        fi
        echo "==> ERROR: Frontend bundle is missing required Qt plugin: $plugin_file" >&2
        missing=1
    done

    # Fail if any required Qt plugin is missing
    if [ "$missing" -ne 0 ]; then
        echo "==> Install the missing Qt plugin package for this build host and rebuild." >&2
        exit 1
    fi

    # Require at least one native platform theme plugin
    if [ ! -f "$FRONTEND_BUNDLE_DIR/plugins/platformthemes/libqxdgdesktopportal.so" ] &&
        [ ! -f "$FRONTEND_BUNDLE_DIR/plugins/platformthemes/libqgtk3.so" ]; then
        echo "==> ERROR: Frontend bundle is missing a native Qt platform theme plugin." >&2
        echo "==> Install Qt xdg-desktop-portal or GTK platform theme package for this build host and rebuild." >&2
        exit 1
    fi
}

##############################################################################

validate_frontend_qt_bundle() {
    local build_version="$1"
    local qt_core_path
    local bundled_version

    # Require bundled QtCore
    qt_core_path="$FRONTEND_BUNDLE_DIR/lib/libQt6Core.so.6"
    if [ ! -e "$qt_core_path" ]; then
        echo "==> ERROR: Frontend bundle is missing libQt6Core.so.6." >&2
        exit 1
    fi

    # Resolve QtCore symlink and parse version
    qt_core_path="$(readlink -f "$qt_core_path")"
    bundled_version="$(printf '%s\n' "${qt_core_path##*/}" | sed -nE 's/^libQt6Core\.so\.([0-9]+(\.[0-9]+){1,2}).*$/\1/p')"
    if [ -z "$bundled_version" ]; then
        echo "==> ERROR: Could not parse bundled Qt version from: $qt_core_path" >&2
        exit 1
    fi

    # Ensure bundled Qt matches build metadata
    if [ "$bundled_version" != "$build_version" ]; then
        echo "==> ERROR: Frontend bundle Qt version does not match build metadata." >&2
        echo "==> Build Qt:  $build_version" >&2
        echo "==> Bundle Qt: $bundled_version" >&2
        exit 1
    fi
}

##############################################################################

validate_frontend_elf_dependencies() {
    local artifact
    local output
    local missing=0

    # Check every bundled ELF dependency
    while IFS= read -r artifact; do
        # Inspect dependencies using bundled libs first
        if ! output="$(LD_LIBRARY_PATH="$FRONTEND_BUNDLE_DIR/lib" ldd "$artifact" 2>&1)"; then
            if ! printf '%s\n' "$output" | grep -Eq 'version `.* not found|=>[[:space:]]*not found'; then
                echo "==> ERROR: Unable to inspect frontend dependency closure for $artifact" >&2
                printf '%s\n' "$output" >&2
                exit 1
            fi
        fi

        # Report unresolved libraries or symbol versions
        if printf '%s\n' "$output" | grep -Eq 'version `.* not found|=>[[:space:]]*not found'; then
            echo "==> ERROR: Unresolved frontend dependency in bundle artifact $artifact:" >&2
            printf '%s\n' "$output" | grep -E 'version `.* not found|=>[[:space:]]*not found' >&2
            missing=1
        fi
    done < <(find "$FRONTEND_BUNDLE_DIR" -type f \( -name 'Lymalink' -o -name '*.so' -o -name '*.so.*' \) | sort)

    # Fail if any bundled artifact has unresolved deps
    if [ "$missing" -ne 0 ]; then
        echo "==> Fix the frontend dependency closure before creating the installer." >&2
        exit 1
    fi
}

##############################################################################
##############################################################################
##############################################################################

# Handle optional clean command
case "${1:-}" in
    "clean")
        _require_no_extra_args clean "${@:2}"
        echo "==> Cleaning project..."
        "$ROOT_DIR/frontend/build.sh" clean
        "$ROOT_DIR/backend/build.sh" clean
        "$ROOT_DIR/backend-overlay/build.sh" clean
        rm -rf "$BUILD_DIR"
        exit 0
        ;;
    "")
        ;;
    *)
        echo "==> ERROR: Invalid option '$1'." >&2
        echo "==> Usage: $0 [clean]" >&2
        exit 1
        ;;
esac

# Require all build tools
for command_name in cmake cp file find flatpak g++ grep ldd make makeself pkg-config readlink sed sha256sum sort strip wc; do
    require_command "$command_name"
done

# Only build installers on x86_64
if [ "$(uname -m)" != "$ARCH" ]; then
    echo "==> ERROR: Installer builds currently support x86_64 only." >&2
    exit 1
fi

# Resolve installer output path
BUILD_HOST_TAG="$(detect_build_host_tag)"
INSTALLER_PATH="$BUILD_DIR/lymalink-installer-${VERSION}-${BUILD_HOST_TAG}-${ARCH}.run"

# Resolve and validate Qt
QMAKE_PATH="$(resolve_qmake)"
check_qt_version "$QMAKE_PATH"

# Clean component build outputs before compiling anything for the installer.
echo "==> Cleaning release component build artifacts..."
"$ROOT_DIR/backend-overlay/build.sh" clean
"$ROOT_DIR/backend/build.sh" clean
"$ROOT_DIR/frontend/build.sh" clean

# Build release binaries
echo "==> Building frontend release..."
"$ROOT_DIR/frontend/build.sh" release

echo "==> Building backend release..."
"$ROOT_DIR/backend/build.sh" release

echo "==> Building overlay release..."
"$ROOT_DIR/backend-overlay/build.sh" release

echo "==> Building Flatpak overlay release..."
"$ROOT_DIR/backend-overlay/build.sh" flatpak-release

# Validate required build outputs
require_file "$FRONTEND_BINARY"
require_file "$BACKEND_BUILD_BIN_DIR/lymalinkd"
require_file "$ROOT_DIR/backend-overlay/lymalink-overlay.sh"
validate_overlay_artifact_set "$OVERLAY_BUILD_BIN_DIR" 64 "native x86_64"
validate_overlay_artifact_set "$OVERLAY_BUILD_BIN_DIR_I386" 32 "native i386"
validate_overlay_artifact_set "$OVERLAY_FLATPAK_BUILD_BIN_DIR" 64 "Flatpak x86_64"
validate_overlay_artifact_set "$OVERLAY_FLATPAK_BUILD_BIN_DIR_I386" 32 "Flatpak i386"

# Create release staging tree
echo "==> Staging release payload..."
rm -rf "$RELEASE_DIR" "$FLATPAK_WORK_DIR" "$INSTALLER_PATH"
mkdir -p \
    "$RELEASE_DIR/bin" \
    "$RELEASE_DIR/lib" \
    "$RELEASE_DIR/lib/i386-linux-gnu" \
    "$FRONTEND_BUNDLE_DIR/bin" \
    "$RELEASE_DIR/share/vulkan/implicit_layer.d" \
    "$RELEASE_DIR/share/icons/hicolor/256x256/apps" \
    "$RELEASE_DIR/share/applications" \
    "$RELEASE_DIR/share/Lymalink/sounds" \
    "$RELEASE_DIR/systemd" \
    "$RELEASE_DIR/flatpak"

# Copy installer scripts and backend files
cp "$SCRIPT_DIR/install.sh" "$RELEASE_DIR/install.sh"
cp "$SCRIPT_DIR/uninstall.sh" "$RELEASE_DIR/uninstall.sh"
cp "$BACKEND_BUILD_BIN_DIR/lymalinkd" "$RELEASE_DIR/bin/lymalinkd"
cp "$ROOT_DIR/backend-overlay/lymalink-overlay.sh" "$RELEASE_DIR/bin/lymalink-overlay"

# Copy native overlay libraries
cp "$OVERLAY_BUILD_BIN_DIR/lymalink-overlay.so" "$RELEASE_DIR/lib/lymalink-overlay.so"
cp "$OVERLAY_BUILD_BIN_DIR/lymalink-overlay-opengl.so" "$RELEASE_DIR/lib/lymalink-overlay-opengl.so"
cp "$OVERLAY_BUILD_BIN_DIR/lymalink-overlay-preloader.so" "$RELEASE_DIR/lib/lymalink-overlay-preloader.so"

# Copy 32-bit overlay libraries
cp "$OVERLAY_BUILD_BIN_DIR_I386/lymalink-overlay.so" "$RELEASE_DIR/lib/i386-linux-gnu/lymalink-overlay.so"
cp "$OVERLAY_BUILD_BIN_DIR_I386/lymalink-overlay-opengl.so" "$RELEASE_DIR/lib/i386-linux-gnu/lymalink-overlay-opengl.so"
cp "$OVERLAY_BUILD_BIN_DIR_I386/lymalink-overlay-preloader.so" "$RELEASE_DIR/lib/i386-linux-gnu/lymalink-overlay-preloader.so"

# Copy assets
cp "$ROOT_DIR/backend/res/"*.ogg "$RELEASE_DIR/share/Lymalink/sounds/"
cp "$ROOT_DIR/frontend/res/img/64x64-lymalink-test-icon.png" "$RELEASE_DIR/share/Lymalink/"
cp "$ROOT_DIR/backend/res/img/BlankBackground_MFC_00041_ED.png" "$RELEASE_DIR/share/Lymalink/lymalinkd-tray-icon.png"
cp "$ROOT_DIR/frontend/res/img/BlankBackground_MFC_00002_E_256x256.png" \
    "$RELEASE_DIR/share/icons/hicolor/256x256/apps/lymalink.png"

# Bundle frontend runtime
cp "$FRONTEND_BINARY" "$FRONTEND_BUNDLE_DIR/bin/Lymalink"
QT_BUILD_VERSION="$("$QMAKE_PATH" -query QT_VERSION)"
printf '%s\n' "$QT_BUILD_VERSION" > "$FRONTEND_BUNDLE_DIR/qt-build-version"
copy_frontend_qt_runtime "$QMAKE_PATH"
copy_frontend_icu_runtime "$FRONTEND_BUNDLE_DIR/lib"
validate_frontend_qt_bundle "$QT_BUILD_VERSION"
validate_frontend_qml_bundle
validate_frontend_plugin_bundle
validate_frontend_elf_dependencies

# Write frontend launcher
cat > "$RELEASE_DIR/bin/Lymalink" <<'EOF'
#!/usr/bin/env bash
set -euo pipefail

if [ -z "${HOME:-}" ]; then
    echo "==> ERROR: HOME is not set." >&2
    exit 1
fi

# Resolve real login home for Snap environments
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

# Locate bundled frontend
LOGIN_HOME="$(resolve_login_home)"
APPDIR="${LYMALINK_FRONTEND_ROOT:-$LOGIN_HOME/.local/lib/lymalink/frontend}"

if [ ! -x "$APPDIR/bin/Lymalink" ]; then
    echo "==> ERROR: Bundled Lymalink frontend binary not found: $APPDIR/bin/Lymalink" >&2
    exit 1
fi

# Configure bundled Qt runtime
if [ -d "$APPDIR/lib" ]; then
    export LD_LIBRARY_PATH="$APPDIR/lib"
fi
if [ -d "$APPDIR/plugins" ]; then
    export QT_PLUGIN_PATH="$APPDIR/plugins"
    export QT_QPA_PLATFORM_PLUGIN_PATH="$APPDIR/plugins/platforms"

    # Prefer native platform theme if present
    if [ -z "${QT_QPA_PLATFORMTHEME:-}" ]; then
        if [ -f "$APPDIR/plugins/platformthemes/libqxdgdesktopportal.so" ]; then
            export QT_QPA_PLATFORMTHEME=xdgdesktopportal
        elif [ -f "$APPDIR/plugins/platformthemes/libqgtk3.so" ]; then
            export QT_QPA_PLATFORMTHEME=gtk3
        fi
    fi
fi
if [ -d "$APPDIR/qml" ]; then
    export QML2_IMPORT_PATH="$APPDIR/qml"
    export QML_IMPORT_PATH="$APPDIR/qml"
fi

# Launch bundled frontend
exec "$APPDIR/bin/Lymalink" "$@"
EOF

# Set executable permissions
chmod 755 \
    "$RELEASE_DIR/install.sh" \
    "$RELEASE_DIR/uninstall.sh" \
    "$RELEASE_DIR/bin/Lymalink" \
    "$RELEASE_DIR/lib/lymalink/frontend/bin/Lymalink" \
    "$RELEASE_DIR/bin/lymalinkd" \
    "$RELEASE_DIR/bin/lymalink-overlay" \
    "$RELEASE_DIR/lib/"*.so \
    "$RELEASE_DIR/lib/i386-linux-gnu/"*.so

# Set asset permissions
chmod 644 \
    "$RELEASE_DIR/share/Lymalink/"*.png \
    "$RELEASE_DIR/share/Lymalink/sounds/"*.ogg \
    "$RELEASE_DIR/share/icons/hicolor/256x256/apps/lymalink.png"

# Strip release binaries
strip \
    "$RELEASE_DIR/lib/lymalink/frontend/bin/Lymalink" \
    "$RELEASE_DIR/bin/lymalinkd" \
    "$RELEASE_DIR/lib/lymalink-overlay.so" \
    "$RELEASE_DIR/lib/lymalink-overlay-opengl.so" \
    "$RELEASE_DIR/lib/lymalink-overlay-preloader.so" \
    "$RELEASE_DIR/lib/i386-linux-gnu/lymalink-overlay.so" \
    "$RELEASE_DIR/lib/i386-linux-gnu/lymalink-overlay-opengl.so" \
    "$RELEASE_DIR/lib/i386-linux-gnu/lymalink-overlay-preloader.so"

# Write native Vulkan layer manifests
write_vulkan_manifest \
    "$RELEASE_DIR/share/vulkan/implicit_layer.d/lymalink_overlay.x86_64.json" \
    "HOME_PLACEHOLDER/.local/lib/lymalink-overlay.so" \
    "VK_LAYER_LYMALINK_overlay_x86_64"

write_vulkan_manifest \
    "$RELEASE_DIR/share/vulkan/implicit_layer.d/lymalink_overlay.x86.json" \
    "HOME_PLACEHOLDER/.local/lib/i386-linux-gnu/lymalink-overlay.so" \
    "VK_LAYER_LYMALINK_overlay_x86"

# Write desktop launcher
cat > "$RELEASE_DIR/share/applications/lymalink.desktop" <<'EOF'
[Desktop Entry]
Name=Lymalink
Exec=HOME_PLACEHOLDER/.local/bin/Lymalink
Icon=lymalink
StartupWMClass=Lymalink
Type=Application
Terminal=false
Categories=Game;Utility;
EOF

# Write user systemd service
cat > "$RELEASE_DIR/systemd/lymalinkd.service" <<'EOF'
[Unit]
Description=Lymalink Backend Daemon
After=default.target

[Service]
ExecStart=HOME_PLACEHOLDER/.local/bin/lymalinkd
Restart=on-failure
RestartSec=5
Type=notify
StandardOutput=journal
StandardError=journal

[Install]
WantedBy=default.target
EOF

# Stage Flatpak VulkanLayer extension
echo "==> Building Flatpak VulkanLayer extension bundle..."
mkdir -p \
    "$FLATPAK_EXTENSION_DIR/files/lib/x86_64-linux-gnu" \
    "$FLATPAK_EXTENSION_DIR/files/lib/i386-linux-gnu" \
    "$FLATPAK_EXTENSION_DIR/files/share/vulkan/implicit_layer.d" \
    "$FLATPAK_REPO_DIR"

# Copy Flatpak overlay libraries
cp "$OVERLAY_FLATPAK_BUILD_BIN_DIR/"*.so "$FLATPAK_EXTENSION_DIR/files/lib/x86_64-linux-gnu/"
cp "$OVERLAY_FLATPAK_BUILD_BIN_DIR_I386/"*.so "$FLATPAK_EXTENSION_DIR/files/lib/i386-linux-gnu/"
chmod 755 "$FLATPAK_EXTENSION_DIR/files/lib/x86_64-linux-gnu/"*.so
chmod 755 "$FLATPAK_EXTENSION_DIR/files/lib/i386-linux-gnu/"*.so

# Write Flatpak Vulkan layer manifests
write_vulkan_manifest \
    "$FLATPAK_EXTENSION_DIR/files/share/vulkan/implicit_layer.d/lymalink_overlay.x86_64.json" \
    "/usr/lib/extensions/vulkan/lymalink/lib/x86_64-linux-gnu/lymalink-overlay.so" \
    "VK_LAYER_LYMALINK_overlay_x86_64"

write_vulkan_manifest \
    "$FLATPAK_EXTENSION_DIR/files/share/vulkan/implicit_layer.d/lymalink_overlay.x86.json" \
    "/usr/lib/extensions/vulkan/lymalink/lib/i386-linux-gnu/lymalink-overlay.so" \
    "VK_LAYER_LYMALINK_overlay_x86"

# Write Flatpak extension metadata
cat > "$FLATPAK_EXTENSION_DIR/metadata" <<EOF
[Runtime]
name=${FLATPAK_EXTENSION_ID}
runtime=org.freedesktop.Platform/x86_64/${FLATPAK_EXTENSION_BRANCH}
sdk=org.freedesktop.Sdk/x86_64/${FLATPAK_EXTENSION_BRANCH}

[ExtensionOf]
ref=runtime/org.freedesktop.Platform/x86_64/${FLATPAK_EXTENSION_BRANCH}
runtime=org.freedesktop.Platform/x86_64/${FLATPAK_EXTENSION_BRANCH}
EOF

# Export and bundle Flatpak extension
flatpak build-export --runtime --arch="$ARCH" "$FLATPAK_REPO_DIR" "$FLATPAK_EXTENSION_DIR" "$FLATPAK_EXTENSION_BRANCH"
flatpak build-bundle --runtime --arch="$ARCH" "$FLATPAK_REPO_DIR" "$FLATPAK_BUNDLE" "$FLATPAK_EXTENSION_ID" "$FLATPAK_EXTENSION_BRANCH"
require_file "$FLATPAK_BUNDLE"

# Build self-extracting installer
echo "==> Creating self-extracting installer..."
makeself --sha256 --tar-format pax --tar-quietly "$RELEASE_DIR" "$INSTALLER_PATH" "Lymalink Installer" ./install.sh
require_file "$INSTALLER_PATH"

echo "==> Installer build done."
echo "    Staging:   $RELEASE_DIR"
echo "    Installer: $INSTALLER_PATH"
