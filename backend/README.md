# Backend Build

Build information for the Linux backend daemon.

## Linux

### Supported Build Host

- Linux `x86_64`

### Dependencies

Install these packages, or your distro's equivalent:

```bash
sudo apt update
sudo apt install \
  build-essential \
  catch2 \
  libcanberra-dev \
  libgdk-pixbuf-2.0-dev \
  libsdbus-c++-dev \
  libsqlite3-dev \
  libsystemd-dev \
  make \
  pkg-config
```

### Build Script

From repository root:

```bash
backend/build.sh clean
backend/build.sh debug
backend/build.sh release
backend/build.sh test
```

Direct Makefile build:

```bash
make -C backend BUILD=debug
make -C backend BUILD=release
```

### Run Locally

```bash
backend/build/debug/bin/lymalinkd
```

Logs:

```bash
tail -f ~/.local/state/lymalink/lymalink-frontend.log
```

### Tests

```bash
backend/build.sh test
backend/build.sh test --silent
```

The backend test command builds and runs the Catch2 test binary under `backend/build/debug/tests`.

### User Service

Deploy installs:

- binary: `~/.local/bin/lymalinkd`
- service: `~/.config/systemd/user/lymalinkd.service`
- achievement sounds: `${XDG_DATA_HOME:-~/.local/share}/Lymalink/sounds/*.ogg`
- test icon: `${XDG_DATA_HOME:-~/.local/share}/Lymalink/64x64-lymalink-test-icon.png`

Deploy file transfer map:

- `backend/build/release/bin/lymalinkd` -> `~/.local/bin/lymalinkd`
- `backend/res/*.ogg` -> `${XDG_DATA_HOME:-~/.local/share}/Lymalink/sounds/*.ogg`
- `frontend/res/img/64x64-lymalink-test-icon.png` -> `${XDG_DATA_HOME:-~/.local/share}/Lymalink/64x64-lymalink-test-icon.png`
- generated content -> `~/.config/systemd/user/lymalinkd.service`

Commands:

```bash
backend/build.sh deploy
backend/build.sh deploy --debug
backend/build.sh start
backend/build.sh stop
backend/build.sh restart
backend/build.sh status
backend/build.sh logs
backend/build.sh uninstall
```

Uninstall removes the backend service, binary, sounds directory, and test icon.
User configuration, database files, and unrelated application data are
preserved.

## Windows

### Supported Build Host

- Windows 10/11 `x86_64`

### Dependencies

- PowerShell 5.1 or later
- CMake
- Ninja
- Qt 6 MSVC kit, including `qmake.exe`
- vcpkg with `sqlite3:x64-windows`
- Build Tools for Visual Studio 2022 with MSVC x64/x86 tools, a Windows SDK, and CMake tools

Set `VCPKG_ROOT`, then install SQLite:

```powershell
$env:VCPKG_ROOT = 'C:\path\to\vcpkg'
& "$env:VCPKG_ROOT\vcpkg.exe" install sqlite3:x64-windows
```

To persist the vcpkg path:

```powershell
[Environment]::SetEnvironmentVariable('VCPKG_ROOT', 'C:\path\to\vcpkg', 'User')
```

Miniaudio and nlohmann/json download automatically on first build. If needed, allow the script for the current terminal:

```powershell
Set-ExecutionPolicy -Scope Process Bypass
```

### Build Script

Run from repository root:

```powershell
.\backend\build.ps1 clean
.\backend\build.ps1 debug
.\backend\build.ps1 release
```

Build outputs:

| Command | Output | Binary |
|---------|--------|--------|
| `debug` | `backend\build\windows\debug\` | `backend\build\windows\debug\bin\lymalinkd.exe` |
| `release` | `backend\build\windows\release\` | `backend\build\windows\release\bin\lymalinkd.exe` |

### Deploy

```powershell
.\backend\build.ps1 deploy
.\backend\build.ps1 deploy --debug
.\backend\build.ps1 start
.\backend\build.ps1 stop
.\backend\build.ps1 restart
.\backend\build.ps1 status
.\backend\build.ps1 logs
.\backend\build.ps1 uninstall
```

`deploy` installs `lymalinkd.exe`, `sqlite3.dll`, sounds, and test icon to `%LOCALAPPDATA%\Programs\Lymalink`. `start` runs daemon independently for current session. `logs` follows `%LOCALAPPDATA%\Lymalink\logs\lymalink-backend.log`. `uninstall` removes backend files only; frontend files, configuration, database, and logs remain.

## Backend Layout

```text
backend
├── build.sh
├── Makefile
├── README.md
├── res
│   └── *.ogg
├── src
│   ├── database
│   │   ├── SQLiteManager.cpp
│   │   └── SQLiteManager.h
│   ├── ipc
│   │   ├── DBusService.cpp
│   │   └── DBusService.h
│   ├── Lymalinkd.cpp
│   ├── Lymalinkd.h
│   ├── main.cpp
│   ├── notification
│   │   ├── AchievementNotificationService.cpp
│   │   ├── AchievementNotificationService.h
│   │   ├── CanberraSoundService.cpp
│   │   ├── CanberraSoundService.h
│   │   ├── FreedesktopNotificationService.cpp
│   │   ├── FreedesktopNotificationService.h
│   │   └── ISoundService.h
│   ├── overlay
│   │   ├── OverlayNotifier.cpp
│   │   └── OverlayNotifier.h
│   ├── service
│   │   ├── SystemdNotify.cpp
│   │   └── SystemdNotify.h
│   ├── tools
│   │   ├── Logger.cpp
│   │   ├── Logger.h
│   │   ├── parsers
│   │   │   ├── AchievementParser.h
│   │   │   ├── GoldbergParser.cpp
│   │   │   ├── GoldbergParser.h
│   │   │   ├── RUNECodexParser.cpp
│   │   │   └── RUNECodexParser.h
│   │   ├── Utils.cpp
│   │   └── Utils.h
│   └── watcher
│       ├── AchievementHandler.cpp
│       ├── AchievementHandler.h
│       ├── PathScanner.cpp
│       ├── PathScanner.h
│       ├── ProcessWatcher.cpp
│       └── ProcessWatcher.h
└── tests
    └── SQLiteManagerTests.cpp
```
