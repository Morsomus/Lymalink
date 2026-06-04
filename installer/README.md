# Linux Installer Build

Builds a self-extracting, user-level Linux installer for Lymalink. The generated
installer contains the frontend, backend daemon, native overlay libraries,
OpenGL launcher, Vulkan layer manifest, desktop files, systemd user unit, and
Flatpak VulkanLayer extension.

Root access is not required to run the generated installer. The installer exits
with an error when run as root because it installs only into the current user's
home directory.

## Supported Target

- Build host: Ubuntu 22.04 `x86_64`
- Linux `x86_64`
- User-level installation only
- Flatpak VulkanLayer extension branch `25.08`
- Flatpak extension libraries compiled inside `org.freedesktop.Sdk//25.08`
- Flatpak extension dependencies checked inside `org.freedesktop.Platform//25.08`

The installer bundles the frontend Qt runtime with `linuxdeployqt`. It does not
bundle backend or overlay distro runtime libraries, GPU drivers, systemd, D-Bus,
or Flatpak itself.

Installer creation currently requires Ubuntu 22.04. Build on Ubuntu 22.04
because `linuxdeployqt` and Qt runtime bundling are host-sensitive; newer or
older distributions can fail or produce incompatible bundles.

## Build Requirements

Install the normal frontend, backend, and overlay build dependencies first.
See:

- [Frontend requirements](../frontend/README.md#ubuntu-2204-dependencies)
- [Backend requirements](../backend/README.md#ubuntu-2204-dependencies)
- [Overlay requirements](../backend-overlay/README.md#ubuntu-2204-dependencies)

Installer build also requires:

```bash
sudo apt update
sudo apt install \
  binutils \
  coreutils \
  flatpak \
  makeself
```

Install `linuxdeployqt` separately and make sure it is available in `PATH`:

```bash
linuxdeployqt --version
```

`installer/build.sh` also calls `cmake`, `g++`, `make`, `pkg-config`, `strip`,
`sha256sum`, and the component build tools listed in the frontend, backend, and
overlay READMEs.

Install Flatpak SDK and runtime branch `25.08`:

```bash
flatpak install --user flathub org.freedesktop.Sdk//25.08
flatpak install --user flathub org.freedesktop.Platform//25.08
```

Overlay compilation requires Vulkan, OpenGL, EGL, and GDK Pixbuf development
files. Frontend compilation and deployment require Qt 6 modules, Qt QML tooling,
and OpenSSL development files.

## Build Installer

Run from repository root:

```bash
installer/build.sh
```

The script:

1. Builds release frontend, backend, and native overlay artifacts.
2. Builds Flatpak overlay artifacts inside Freedesktop SDK `25.08` and checks
   their dependencies inside Freedesktop Platform `25.08`.
3. Bundles the frontend Qt runtime into a private AppDir with `linuxdeployqt`.
4. Stages installer payload under `installer/build/lymalink-release/`.
5. Strips native binaries and shared libraries.
6. Builds bundled Flatpak VulkanLayer extension from SDK-built overlay libraries.
7. Creates self-extracting installer with `makeself`.

Version comes from repository root `VERSION` file.

## Output

```text
installer/build/
├── flatpak-work/
├── lymalink-release/
└── lymalink-installer-<VERSION>-x86_64.run
```

Distributable file:

```text
installer/build/lymalink-installer-<VERSION>-x86_64.run
```

## Test Generated Installer

Run installer as normal desktop user:

```bash
chmod +x installer/build/lymalink-installer-<VERSION>-x86_64.run
installer/build/lymalink-installer-<VERSION>-x86_64.run
```

Install script checks required commands and ELF runtime dependencies before
copying files. It reloads systemd user units but does not enable or start
`lymalinkd.service`; desktop application controls backend service activation.

Installed files:

| Files | Destination |
|-------|-------------|
| `Lymalink`, `lymalinkd`, `lymalink-overlay`, `uninstall-lymalink` wrappers/tools | `~/.local/bin/` |
| Bundled frontend binary, Qt libraries, plugins, and QML imports | `~/.local/lib/lymalink/frontend/` |
| `lymalink-overlay*.so` | `~/.local/lib/` |
| Vulkan manifest | `${XDG_DATA_HOME:-~/.local/share}/vulkan/implicit_layer.d/` |
| Icon and desktop entry | `${XDG_DATA_HOME:-~/.local/share}/icons/`, `applications/` |
| Sounds and test icon | `${XDG_DATA_HOME:-~/.local/share}/Lymalink/` |
| `lymalinkd.service` | `${XDG_CONFIG_HOME:-~/.config}/systemd/user/` |
| Flatpak VulkanLayer extension | User Flatpak installation |

When needed, installer adds `~/.local/bin` to `~/.bash_profile` and
`~/.profile`.

## Uninstall Test

```bash
~/.local/bin/uninstall-lymalink
```

Uninstaller removes all installer-managed content, including files, PATH blocks, the notification sounds directory, 
the Flatpak extension, and the systemd user unit. User configuration and database files remain intact.

## Installer Layout

```text
installer/
├── build.sh       # Builds and packages release installer
├── install.sh     # Runs inside self-extracting installer
├── README.md
└── uninstall.sh   # Installed as ~/.local/bin/uninstall-lymalink
```

## Notes

- Re-running generated installer updates existing user-level installation.
- Release payload embeds absolute home directory paths during installation.
