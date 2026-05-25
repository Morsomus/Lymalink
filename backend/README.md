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

## Layout

```text
backend/
├── build.sh
├── Makefile
├── README.md
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
│   ├── service
│   │   ├── SystemdNotify.cpp
│   │   └── SystemdNotify.h
│   ├── tools
│   │   ├── Logger.cpp
│   │   ├── Logger.h
│   │   ├── Utils.cpp
│   │   └── Utils.h
│   └── watcher
│       ├── PathScanner.cpp
│       ├── PathScanner.h
│       ├── ProcessWatcher.cpp
│       └── ProcessWatcher.h
└── tests
    └── SQLiteManagerTests.cpp
```
