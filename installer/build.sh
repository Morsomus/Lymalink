#!/usr/bin/env bash
#########################################################
# File: build.sh (installer)
# Date: 2026-05-31
# Author: Morsomus
# Copyright: see /LICENSE
# Description: Builds the self-extracting Lymalink installer.
#########################################################

set -euo pipefail

if [ "$#" -gt 0 ]; then
    echo "==> ERROR: Installer build does not accept options." >&2
    echo "==> Usage: $0" >&2
    exit 1
fi

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"

# When enabled, build output is diverted to /tmp (RAMfs)
BUILD_TO_TMP=0
if [ "$BUILD_TO_TMP" -eq 1 ] 2>/dev/null; then
    BUILD_DIR="/tmp/lymalink-installer-build"
else
    BUILD_DIR="$SCRIPT_DIR/build"
fi

RELEASE_DIR="$BUILD_DIR/lymalink-release"
FRONTEND_APPDIR="$BUILD_DIR/lymalink-frontend.AppDir"
FRONTEND_BUNDLE_DIR="$RELEASE_DIR/lib/lymalink/frontend"
FLATPAK_WORK_DIR="$BUILD_DIR/flatpak-work"
FLATPAK_EXTENSION_DIR="$FLATPAK_WORK_DIR/extension"
FLATPAK_REPO_DIR="$FLATPAK_WORK_DIR/repo"

VERSION="$(tr -d '[:space:]' < "$ROOT_DIR/VERSION")"
ARCH="x86_64"
INSTALLER_PATH="$BUILD_DIR/lymalink-installer-${VERSION}-${ARCH}.run"

FRONTEND_BINARY="$ROOT_DIR/frontend/build/release/bin/Lymalink"
BACKEND_BUILD_BIN_DIR="$ROOT_DIR/backend/build/release/bin"
OVERLAY_BUILD_BIN_DIR="$ROOT_DIR/backend-overlay/build/release/bin"
OVERLAY_FLATPAK_BUILD_BIN_DIR="$ROOT_DIR/backend-overlay/build/flatpak/release/bin"
FLATPAK_EXTENSION_ID="org.freedesktop.Platform.VulkanLayer.lymalink"
FLATPAK_EXTENSION_BRANCH="25.08"
FLATPAK_BUNDLE="$RELEASE_DIR/flatpak/${FLATPAK_EXTENSION_ID}.flatpak"

require_command() {
    if ! command -v "$1" >/dev/null 2>&1; then
        echo "==> ERROR: Required command not found: $1" >&2
        exit 1
    fi
}

require_file() {
    if [ ! -f "$1" ]; then
        echo "==> ERROR: Required build artifact not found: $1" >&2
        exit 1
    fi
}

resolve_qmake() {
    local candidate

    for candidate in qmake6 qmake; do
        if command -v "$candidate" >/dev/null 2>&1 && "$candidate" -query QT_INSTALL_PREFIX >/dev/null 2>&1; then
            command -v "$candidate"
            return 0
        fi
    done

    echo "==> ERROR: Could not find working Qt qmake for linuxdeployqt." >&2
    echo "==> Install Qt6 qmake tools, e.g. qt6-base-dev-tools, and verify: qmake6 -v" >&2
    exit 1
}

write_vulkan_manifest() {
    local destination="$1"
    local library_path="$2"

    cat > "$destination" <<EOF
{
    "file_format_version": "1.0.0",
    "layer": {
        "name": "VK_LAYER_LYMALINK_overlay",
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

for command_name in cmake flatpak g++ linuxdeployqt make makeself pkg-config sha256sum strip; do
    require_command "$command_name"
done

if [ "$(uname -m)" != "$ARCH" ]; then
    echo "==> ERROR: Installer builds currently support x86_64 only." >&2
    exit 1
fi

echo "==> Building frontend release..."
"$ROOT_DIR/frontend/build.sh" release

echo "==> Building backend release..."
"$ROOT_DIR/backend/build.sh" release

echo "==> Building overlay release..."
"$ROOT_DIR/backend-overlay/build.sh" release

echo "==> Building Flatpak overlay release..."
"$ROOT_DIR/backend-overlay/build.sh" flatpak-release

require_file "$FRONTEND_BINARY"
require_file "$BACKEND_BUILD_BIN_DIR/lymalinkd"
require_file "$OVERLAY_BUILD_BIN_DIR/lymalink-overlay.so"
require_file "$OVERLAY_BUILD_BIN_DIR/lymalink-overlay-opengl.so"
require_file "$OVERLAY_BUILD_BIN_DIR/lymalink-overlay-preloader.so"
require_file "$OVERLAY_FLATPAK_BUILD_BIN_DIR/lymalink-overlay.so"
require_file "$OVERLAY_FLATPAK_BUILD_BIN_DIR/lymalink-overlay-opengl.so"
require_file "$OVERLAY_FLATPAK_BUILD_BIN_DIR/lymalink-overlay-preloader.so"
require_file "$ROOT_DIR/backend-overlay/lymalink-overlay.sh"

echo "==> Staging release payload..."
rm -rf "$RELEASE_DIR" "$FRONTEND_APPDIR" "$FLATPAK_WORK_DIR" "$INSTALLER_PATH"
mkdir -p \
    "$RELEASE_DIR/bin" \
    "$RELEASE_DIR/lib" \
    "$FRONTEND_APPDIR/usr/bin" \
    "$FRONTEND_APPDIR/usr/share/applications" \
    "$FRONTEND_APPDIR/usr/share/icons/hicolor/256x256/apps" \
    "$RELEASE_DIR/share/vulkan/implicit_layer.d" \
    "$RELEASE_DIR/share/icons/hicolor/256x256/apps" \
    "$RELEASE_DIR/share/applications" \
    "$RELEASE_DIR/share/Lymalink/sounds" \
    "$RELEASE_DIR/systemd" \
    "$RELEASE_DIR/flatpak"

cp "$SCRIPT_DIR/install.sh" "$RELEASE_DIR/install.sh"
cp "$SCRIPT_DIR/uninstall.sh" "$RELEASE_DIR/uninstall.sh"
cp "$BACKEND_BUILD_BIN_DIR/lymalinkd" "$RELEASE_DIR/bin/lymalinkd"
cp "$ROOT_DIR/backend-overlay/lymalink-overlay.sh" "$RELEASE_DIR/bin/lymalink-overlay"
cp "$OVERLAY_BUILD_BIN_DIR/lymalink-overlay.so" "$RELEASE_DIR/lib/lymalink-overlay.so"
cp "$OVERLAY_BUILD_BIN_DIR/lymalink-overlay-opengl.so" "$RELEASE_DIR/lib/lymalink-overlay-opengl.so"
cp "$OVERLAY_BUILD_BIN_DIR/lymalink-overlay-preloader.so" "$RELEASE_DIR/lib/lymalink-overlay-preloader.so"
cp "$ROOT_DIR/backend/res/"*.ogg "$RELEASE_DIR/share/Lymalink/sounds/"
cp "$ROOT_DIR/frontend/res/img/64x64-lymalink-test-icon.png" "$RELEASE_DIR/share/Lymalink/"
cp "$ROOT_DIR/frontend/res/img/BlankBackground_MFC_00002_E_256x256.png" \
    "$RELEASE_DIR/share/icons/hicolor/256x256/apps/lymalink.png"

cp "$FRONTEND_BINARY" "$FRONTEND_APPDIR/usr/bin/Lymalink"
cp "$ROOT_DIR/frontend/res/img/BlankBackground_MFC_00002_E_256x256.png" \
    "$FRONTEND_APPDIR/usr/share/icons/hicolor/256x256/apps/lymalink.png"
cat > "$FRONTEND_APPDIR/usr/share/applications/lymalink.desktop" <<'EOF'
[Desktop Entry]
Name=Lymalink
Exec=Lymalink
Icon=lymalink
StartupWMClass=Lymalink
Type=Application
Terminal=false
Categories=Game;Utility;
EOF

echo "==> Bundling frontend Qt runtime with linuxdeployqt..."
QMAKE_PATH="$(resolve_qmake)"
linuxdeployqt \
    "$FRONTEND_APPDIR/usr/share/applications/lymalink.desktop" \
    -qmake="$QMAKE_PATH" \
    -qmldir="$ROOT_DIR/frontend"
mkdir -p "$FRONTEND_BUNDLE_DIR"
cp -a "$FRONTEND_APPDIR/usr" "$FRONTEND_BUNDLE_DIR/"

cat > "$RELEASE_DIR/bin/Lymalink" <<'EOF'
#!/usr/bin/env bash
set -euo pipefail

if [ -z "${HOME:-}" ]; then
    echo "==> ERROR: HOME is not set." >&2
    exit 1
fi

APPDIR="${LYMALINK_FRONTEND_ROOT:-$HOME/.local/lib/lymalink/frontend/usr}"

if [ ! -x "$APPDIR/bin/Lymalink" ]; then
    echo "==> ERROR: Bundled Lymalink frontend binary not found: $APPDIR/bin/Lymalink" >&2
    exit 1
fi

export LD_LIBRARY_PATH="$APPDIR/lib${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
export QT_PLUGIN_PATH="$APPDIR/plugins${QT_PLUGIN_PATH:+:$QT_PLUGIN_PATH}"
export QT_QPA_PLATFORM_PLUGIN_PATH="$APPDIR/plugins/platforms${QT_QPA_PLATFORM_PLUGIN_PATH:+:$QT_QPA_PLATFORM_PLUGIN_PATH}"
export QML2_IMPORT_PATH="$APPDIR/qml${QML2_IMPORT_PATH:+:$QML2_IMPORT_PATH}"

exec "$APPDIR/bin/Lymalink" "$@"
EOF

chmod 755 \
    "$RELEASE_DIR/install.sh" \
    "$RELEASE_DIR/uninstall.sh" \
    "$RELEASE_DIR/bin/Lymalink" \
    "$RELEASE_DIR/bin/lymalinkd" \
    "$RELEASE_DIR/bin/lymalink-overlay" \
    "$RELEASE_DIR/lib/"*.so
chmod 644 \
    "$RELEASE_DIR/share/Lymalink/"*.png \
    "$RELEASE_DIR/share/Lymalink/sounds/"*.ogg \
    "$RELEASE_DIR/share/icons/hicolor/256x256/apps/lymalink.png"

strip \
    "$RELEASE_DIR/lib/lymalink/frontend/usr/bin/Lymalink" \
    "$RELEASE_DIR/bin/lymalinkd" \
    "$RELEASE_DIR/lib/lymalink-overlay.so" \
    "$RELEASE_DIR/lib/lymalink-overlay-opengl.so" \
    "$RELEASE_DIR/lib/lymalink-overlay-preloader.so"

write_vulkan_manifest \
    "$RELEASE_DIR/share/vulkan/implicit_layer.d/lymalink_overlay.json" \
    "HOME_PLACEHOLDER/.local/lib/lymalink-overlay.so"

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

echo "==> Building Flatpak VulkanLayer extension bundle..."
mkdir -p \
    "$FLATPAK_EXTENSION_DIR/files/lib/x86_64-linux-gnu" \
    "$FLATPAK_EXTENSION_DIR/files/share/vulkan/implicit_layer.d" \
    "$FLATPAK_REPO_DIR"
cp "$OVERLAY_FLATPAK_BUILD_BIN_DIR/"*.so "$FLATPAK_EXTENSION_DIR/files/lib/x86_64-linux-gnu/"
chmod 755 "$FLATPAK_EXTENSION_DIR/files/lib/x86_64-linux-gnu/"*.so
write_vulkan_manifest \
    "$FLATPAK_EXTENSION_DIR/files/share/vulkan/implicit_layer.d/lymalink_overlay.x86_64.json" \
    "/usr/lib/extensions/vulkan/lymalink/lib/x86_64-linux-gnu/lymalink-overlay.so"

cat > "$FLATPAK_EXTENSION_DIR/metadata" <<EOF
[Runtime]
name=${FLATPAK_EXTENSION_ID}
runtime=org.freedesktop.Platform/x86_64/${FLATPAK_EXTENSION_BRANCH}
sdk=org.freedesktop.Sdk/x86_64/${FLATPAK_EXTENSION_BRANCH}

[ExtensionOf]
ref=runtime/org.freedesktop.Platform/x86_64/${FLATPAK_EXTENSION_BRANCH}
runtime=org.freedesktop.Platform/x86_64/${FLATPAK_EXTENSION_BRANCH}
EOF

flatpak build-export --runtime --arch="$ARCH" \
    "$FLATPAK_REPO_DIR" "$FLATPAK_EXTENSION_DIR" "$FLATPAK_EXTENSION_BRANCH"
flatpak build-bundle --runtime --arch="$ARCH" \
    "$FLATPAK_REPO_DIR" "$FLATPAK_BUNDLE" \
    "$FLATPAK_EXTENSION_ID" "$FLATPAK_EXTENSION_BRANCH"
require_file "$FLATPAK_BUNDLE"

echo "==> Creating self-extracting installer..."
makeself --sha256 "$RELEASE_DIR" "$INSTALLER_PATH" "Lymalink Installer" ./install.sh
require_file "$INSTALLER_PATH"

echo "==> Installer build done."
echo "    Staging:   $RELEASE_DIR"
echo "    Installer: $INSTALLER_PATH"
