# Windows Installer Build

Builds a per-user NSIS installer for Lymalink on Windows.

The installer installs into `%LOCALAPPDATA%\Programs\Lymalink`.

## Target

- Build hosts: Windows x64 with MSVC, Qt, vcpkg, Vulkan SDK, CMake, Ninja, and
  NSIS
- Install targets: Windows x64 user accounts
- Install scope: current user only
- Frontend Qt runtime: bundled with `windeployqt`
- Overlay architecture support: x64 and x86 overlay libraries

## Build Requirements

Install component build requirements first:

- [Frontend requirements](../../frontend/README.md#windows)
- [Backend requirements](../../backend/README.md#windows)
- [Overlay requirements](../../backend-overlay/README.md#windows)

Installer-only tools:

```text
NSIS with makensis.exe available on PATH
```

## Build Installer

Run from repository root in a Windows PowerShell environment with the required
toolchains available:

```powershell
installer/windows/build.ps1
```

Build flow:

1. Builds frontend, backend, and backend-overlay Windows release artifacts.
2. Stages payload in `installer/windows/build/lymalink-release/`.
3. Bundles frontend Qt runtime with `windeployqt`.
4. Copies backend daemon, SQLite runtime, sounds, and test icon.
5. Copies x64 and x86 overlay libraries and injector helpers.
6. Creates a per-user NSIS installer.

Version comes from root [VERSION](../../VERSION).

## Output

```text
installer/windows/build/
├── lymalink-release/
└── lymalink-installer-<VERSION>-win-x64.exe
```

## Installed Files

| Payload | Destination |
|---------|-------------|
| Frontend executable and Qt runtime | `%LOCALAPPDATA%\Programs\Lymalink\` |
| `lymalinkd.exe`, `sqlite3.dll`, sounds, and test icon | `%LOCALAPPDATA%\Programs\Lymalink\` |
| Overlay DLLs, injector helpers, and Vulkan manifests | `%LOCALAPPDATA%\Programs\Lymalink\overlay\` |
| Start Menu shortcut | `%APPDATA%\Microsoft\Windows\Start Menu\Programs\Lymalink\Lymalink.lnk` |
| Uninstaller | `%LOCALAPPDATA%\Programs\Lymalink\uninstall-lymalink.exe` |

The installer registers Vulkan implicit layer manifests under current-user x64
and x86 registry views:

```text
HKCU\Software\Khronos\Vulkan\ImplicitLayers
```

The installer also registers Lymalink under current-user Windows Installed apps:

```text
HKCU\Software\Microsoft\Windows\CurrentVersion\Uninstall\Lymalink
```

Re-running the installer updates installed program files under
`%LOCALAPPDATA%\Programs\Lymalink` and preserves existing user configuration
and database files under `%APPDATA%\Lymalink`.

## Uninstall

Uninstall from Windows Installed apps or run:

```powershell
%LOCALAPPDATA%\Programs\Lymalink\uninstall-lymalink.exe
```

Uninstall removes installer-managed files, Start Menu shortcut, overlay Vulkan registry values, Installed apps entry, logs, and generated images.
User configuration and database files are preserved.
