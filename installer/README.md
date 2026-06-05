# Linux Installer Build

Builds a self-extracting, user-level Linux installer for Lymalink.

The generated `.run` installs into the current user's home directory. Do not run
it with `sudo`; the install script rejects root.

## Target

- Build hosts: Linux `x86_64` systems with the required build dependencies
- Install targets: compatible Linux `x86_64` systems
- Install scope: current user only
- Frontend Qt runtime: bundled privately from the build host, never lower than
  `6.8.0`
- Flatpak VulkanLayer extension branch: `25.08`

The installer bundles the frontend Qt runtime manually from the build host
`qmake` paths, plus the ICU runtime libraries detected on the build host so Qt
can load the exact ICU SONAME it was built against. Frontend Qt libraries, Qt
plugins, and QML imports are private to the frontend wrapper. It does not bundle
system libraries such as OpenSSL, libstdc++, GL, D-Bus, or `systemd`.

Flatpak is required because the installer installs the Flatpak VulkanLayer
extension used by Flatpak games. If Flatpak or extension installation fails, the
installer fails.

## Build Requirements

Install component build requirements first:

- [Frontend requirements](../frontend/README.md#dependencies)
- [Backend requirements](../backend/README.md#dependencies)
- [Overlay requirements](../backend-overlay/README.md#dependencies)

Installer-only packages, using your distro package manager:

```bash
binutils coreutils flatpak makeself
```

## Build Installer

Run from repository root:

```bash
installer/build.sh
```

`build.sh` detects the build host from `/etc/os-release` and writes that tag
into the generated installer filename. There is no distro whitelist.

Build flow:

1. Builds frontend, backend, and native overlay release artifacts.
2. Builds Flatpak overlay artifacts.
3. Stages payload in `installer/build/lymalink-release/`.
4. Bundles frontend Qt libraries, plugins, QML imports, and ICU runtime.
5. Stores frontend build Qt version in payload metadata.
6. Builds Flatpak VulkanLayer extension bundle.
7. Creates self-extracting installer with `makeself`.

Version comes from root [VERSION](../VERSION).

## Output

```text
installer/build/
├── flatpak-work/
├── lymalink-release/
└── lymalink-installer-<VERSION>-<DISTRO>-x86_64.run
```

Distribute:

```text
installer/build/lymalink-installer-<VERSION>-<DISTRO_VERSION>-x86_64.run
```

## Target Runtime Requirements

Target systems need:

- Flatpak
- Native runtime libraries needed by backend and overlay, such as Vulkan, GL,
  D-Bus, `systemd`, GDK Pixbuf, and sdbus-c++
- Frontend host libraries intentionally not bundled, such as OpenSSL,
  libstdc++, desktop display libraries, and GPU driver stack

Frontend Qt/QML must come from the bundled payload. When the payload is
incomplete, the installer reports a bad build payload and exits. When host
runtime libraries are missing, the installer prints missing `.so` names and a
best-effort package command for the detected package manager.

## Test Install

Run as normal desktop user:

```bash
chmod +x installer/build/lymalink-installer-<VERSION>-<DISTRO>-x86_64.run
installer/build/lymalink-installer-<VERSION>-<DISTRO>-x86_64.run
```

Install script checks before copying files:

- Required payload files
- Frontend private Qt runtime and QML payload
- ELF runtime dependencies with `ldd`, including frontend binary, Qt plugins,
  QML plugins, and native overlay libraries
- Flatpak extension bundle

The installer reloads user `systemd` units. It does not enable or start
`lymalinkd.service`; desktop app controls backend activation.

## Installed Files

| Payload | Destination |
|---------|-------------|
| `Lymalink`, `lymalinkd`, `lymalink-overlay`, `uninstall-lymalink` | `~/.local/bin/` |
| Frontend binary, Qt runtime, QML imports, plugins, ICU, and Qt build metadata | `~/.local/lib/lymalink/frontend/` |
| Overlay `.so` files | `~/.local/lib/` |
| Vulkan manifest | `${XDG_DATA_HOME:-~/.local/share}/vulkan/implicit_layer.d/` |
| Icon and desktop entry | `${XDG_DATA_HOME:-~/.local/share}/icons/`, `applications/` |
| Sounds and test icon | `${XDG_DATA_HOME:-~/.local/share}/Lymalink/` |
| `lymalinkd.service` | `${XDG_CONFIG_HOME:-~/.config}/systemd/user/` |
| Flatpak VulkanLayer extension | User Flatpak installation |

If `~/.local/bin` is missing from `PATH`, installer adds a managed block to
`~/.bash_profile`, `~/.profile`, and `~/.bashrc`.

## Uninstall

```bash
~/.local/bin/uninstall-lymalink
```

Uninstaller removes installer-managed files, PATH blocks, notification sounds,
Flatpak extension, and user `systemd` unit. User config and database files stay.

## Notes

- Re-running generated installer updates existing user-level install.
- Prefer an installer built on the same or a compatible distro release.
- Release payload patches `HOME_PLACEHOLDER` to the installing user's home.
