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

Debug/release binaries are built under `/tmp/lymalinkd-build` by default.

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

The backend test command builds and runs the Catch2 test binary under `/tmp/lymalinkd-build/debug/tests`.

## Run Locally

```bash
/tmp/lymalinkd-build/debug/bin/lymalinkd
```

Temporary dev log:

```bash
tail -f /tmp/lymalinkd.log
```

## User Service

Deploy installs:

- binary: `~/.local/bin/lymalinkd`
- service: `~/.config/systemd/user/lymalinkd.service`
- overlay libs: `~/.local/lib/lymalink-overlay*.so`
- Vulkan layer manifest: `~/.local/share/vulkan/implicit_layer.d/lymalink_overlay.json`
- OpenGL launcher: `~/.local/bin/lymalink-overlay`
- achievement sounds: `${XDG_DATA_HOME:-~/.local/share}/Lymalink/sounds/*.ogg`
- test icon: `${XDG_DATA_HOME:-~/.local/share}/Lymalink/64x64-lymalink-test-icon.png`

Deploy file transfer map:

- `/tmp/lymalinkd-build/release/bin/lymalinkd` -> `~/.local/bin/lymalinkd`
- `/tmp/lymalinkd-build/release/bin/lymalink-overlay.so` -> `~/.local/lib/lymalink-overlay.so`
- `/tmp/lymalinkd-build/release/bin/lymalink-overlay-opengl.so` -> `~/.local/lib/lymalink-overlay-opengl.so`
- `/tmp/lymalinkd-build/release/bin/lymalink-overlay-preloader.so` -> `~/.local/lib/lymalink-overlay-preloader.so`
- `backend-overlay/lymalink-overlay.sh` -> `~/.local/bin/lymalink-overlay`
- `backend/res/*.ogg` -> `${XDG_DATA_HOME:-~/.local/share}/Lymalink/sounds/*.ogg`
- `frontend/res/img/64x64-lymalink-test-icon.png` -> `${XDG_DATA_HOME:-~/.local/share}/Lymalink/64x64-lymalink-test-icon.png`
- generated content -> `~/.config/systemd/user/lymalinkd.service`
- generated content -> `${XDG_DATA_HOME:-~/.local/share}/vulkan/implicit_layer.d/lymalink_overlay.json`
- generated Flatpak bundle install (`org.freedesktop.Platform.VulkanLayer.lymalink//25.08`) -> user Flatpak runtime

Commands:

```bash
backend/build.sh deploy
backend/build.sh start
backend/build.sh stop
backend/build.sh restart
backend/build.sh status
backend/build.sh logs
backend/build.sh uninstall
```

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

## Backend Overlay Layout

```text
backend-overlay
├── include
│   ├── OverlaySharedMemoryState.h
│   └── OverlaySocketProtocol.h
├── lymalink-overlay.sh
├── Makefile
└── src
    ├── GLOverlayOpenGL.cpp
    ├── GLOverlayPreloader.cpp
    ├── Logger.cpp
    ├── Logger.h
    ├── OverlayReceiver.cpp
    ├── OverlayReceiver.h
    ├── VulkanOverlayLayer.cpp
    ├── VulkanOverlayLayer.h
    ├── VulkanOverlayRenderer.cpp
    └── VulkanOverlayRenderer.h
```