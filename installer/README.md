# Lymalink Linux Installer

Builds a self-extracting, user-level Linux installer for Lymalink. The generated
installer contains the frontend, backend daemon, native overlay libraries,
OpenGL launcher, Vulkan layer manifest, desktop files, systemd user unit, and
Flatpak VulkanLayer extension.

Root access is not required to run the generated installer.

## Supported Target

- Linux `x86_64`
- User-level installation only
- Flatpak VulkanLayer extension branch `25.08`

The installer does not bundle distro runtime libraries, GPU drivers, systemd,
D-Bus, or Flatpak itself.

## Build Requirements

Install the normal frontend, backend, and overlay build dependencies first.
See:

- [Frontend requirements](../README.md#frontend)
- [Backend requirements](../backend/README.md#build)
- [Overlay build notes](../backend-overlay/README.md#build)

Installer build also requires:

- `cmake`
- `flatpak`
- `g++`
- `make`
- `makeself`
- `pkg-config`
- `sha256sum`
- `strip`
- `wget` and `unzip` when `backend-overlay/src/imgui/` has not been populated

Overlay compilation requires Vulkan, OpenGL, EGL, and GDK Pixbuf development
files. Frontend compilation requires Qt 6 modules and OpenSSL development files.

## Build Installer

Run from repository root:

```bash
installer/build.sh
```

The script:

1. Builds release frontend, backend, and overlay artifacts.
2. Stages installer payload under `installer/build/lymalink-release/`.
3. Strips binaries and shared libraries.
4. Builds bundled Flatpak VulkanLayer extension.
5. Creates self-extracting installer with `makeself`.

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
| `Lymalink`, `lymalinkd`, `lymalink-overlay`, `uninstall-lymalink` | `~/.local/bin/` |
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

