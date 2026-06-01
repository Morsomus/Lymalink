# Lymalinkd

`lymalinkd` is the Linux backend daemon for Lymalink.

## Build

Requirements:

- Linux
- `g++` or another C++20 compiler
- `make`
- `systemd --user` for deploy/service commands
- `sdbus-cpp-devel`
- `sqlite-devel`
- `catch2-devel`
- `libcanberra-devel`

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

## Tests

```bash
backend/build.sh test
backend/build.sh test --silent
```

The backend test command builds and runs the Catch2 test binary under `backend/build/debug/tests`.

## Run Locally

```bash
backend/build/debug/bin/lymalinkd
```

Logs:

```bash
tail -f ~/.local/state/lymalink/lymalink-frontend.log
tail -f ~/.local/state/lymalink/lymalink-backend.log
```

## User Service

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
│   │   │   ├── CodexParser.cpp
│   │   │   └── CodexParser.h
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
