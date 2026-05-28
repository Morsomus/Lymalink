# Lymalink

**Achievement tracker for Steam emulators, Steam/Cloud APIs, and fully custom targets.**

> **Note:** Lymalink is currently in early development and is not yet functional. Features and documentation are subject to change.

## Overview

Lymalink is a cross-platform application designed to monitor local game achievement files and synchronize data through cloud APIs. It provides a unified solution for tracking milestones across Steam emulators and official environments alike. Beyond gaming, the platform offers the flexibility to create custom achievements which can be tracked from various sources. At its core runs a lightweight background daemon (`lymalinkd`) responsible for file monitoring and delivering desktop notifications when achievements are detected.

The frontend provides a clean interface for viewing progress and managing settings, but is entirely optional. Once configured through the frontend, the daemon can run independently in the background without it. For users who prefer tighter control, Lymalink can also be configured to require the frontend to be open before the daemon is permitted to run.

## Table of Contents

- [Project Status](#project-status)
- [Planned Features](#planned-features)
- [Building for Linux](#building-for-linux)
  - [Frontend](#frontend)
  - [Backend](#backend)
- [Credits](#credits)
- [Disclaimer](#disclaimer)
  - [General](#general)
  - [Steam Emulator Support](#steam-emulator-support)
  - [Official Steam API / User Data](#official-steam-api--user-data)
  - [No Warranty](#no-warranty)

## Project Status

Current progress towards the first working version, platform-wise

### Linux
| Component | Status<br>v0.8.0-beta | Milestone |
|-----------|--------|--------|
| **Frontend** | 🚧 In Development | v1.0.0 |
| &nbsp;&nbsp;&nbsp;&nbsp;Side Navigation Bar | ✅ Ready to Deploy | |
| &nbsp;&nbsp;&nbsp;&nbsp;Side Navigation Bar Business Logic | ✅ Ready to Deploy | v0.8.0-beta |
| &nbsp;&nbsp;&nbsp;&nbsp;Settings | ✅ Ready to Deploy | |
| &nbsp;&nbsp;&nbsp;&nbsp;Settings Business Logic | 🚧 In Development | v0.8.0-beta |
| &nbsp;&nbsp;&nbsp;&nbsp;Dashboard | ✅ Ready to Deploy | |
| &nbsp;&nbsp;&nbsp;&nbsp;Dashboard Business Logic | ✅ Ready to Deploy | v0.8.0-beta |
| &nbsp;&nbsp;&nbsp;&nbsp;Achievement Progress Details | ✅ Ready to Deploy | |
| &nbsp;&nbsp;&nbsp;&nbsp;Achievement Progress Details Business Logic | ✅ Ready to Deploy | v0.8.0-beta |
| &nbsp;&nbsp;&nbsp;&nbsp;Statistics | 🔴 To Be Started | v0.9.0-beta |
| &nbsp;&nbsp;&nbsp;&nbsp;Statistics Business Logic | 🔴 To Be Started | v0.9.0-beta |
| &nbsp;&nbsp;&nbsp;&nbsp;Localisation | 🔴 To Be Started | |
| **Backend Service** | 🚧 In Development | v0.8.0-beta |
| **Compatibility & Testing** | 🚧 In Development | v1.0.0 |
| &nbsp;&nbsp;&nbsp;&nbsp;Distribution Compatibility | 🚧 In Development | v1.0.0 |

### Windows
| Component | Status |
|-----------|--------|
| Frontend | 🔴 To Be Started |
| Backend Service | 🔴 To Be Started |

### macOS
| Component | Status |
|-----------|--------|
| Frontend | ❓ To Be Decided |
| Backend Service | ❓ To Be Decided |

---

## Planned Features

> The following features are planned and subject to change. Core tracking functionality is not listed here.

- **Multiple Profiles** - Switch between independent profiles. Useful for tracking a fresh playthrough, a challenge run, or simply keeping accounts separate.
- **Customizable UI** - Toggle interface elements on or off, adjust tooltips, languages to choose from, switch between dark, light, and system themes to suit your preference and much more.
- **Export & Import** - Move your achievement data between devices or back it up in open formats such as JSON or CSV.
- **Achievement Reports (Multiple file formats)** - Generate shareable summaries for a single title or a selection of titles. A clean, exportable snapshot of your achievements.
- **Desktop Notifications** - Get notified when achievements are detected or a sync completes. User decides if tracker can run at system startup in the background, or only while the app is open.

---

## Building for Linux

### Frontend

#### Requirements

**Build dependencies:**

- CMake >= 3.16
- Qt 6 (Core, Gui, Qml, Quick, Widgets modules)
- C++20-compatible compiler (GCC 10+, Clang 10+, or equivalent)
- Qt6 qml module tools (`qt6-qml-module`/`qtcreator`)

**Recommended (for running):**

- Linux Desktop Environment
- X11 or Wayland compositor
- Plasma/XDG desktop integration for automatic app menu entry

#### Build Script

The `frontend/build.sh` script automates the full build workflow. Run it from the repository root or `frontend/` directory:

```bash
chmod +x frontend/build.sh
frontend/build.sh clean       # Remove build/
frontend/build.sh debug       # Debug build   -> frontend/build/debug/
frontend/build.sh release     # Release build -> frontend/build/release/
frontend/build.sh deploy      # Clean + release build, strip binary, install to XDG user dirs
frontend/build.sh uninstall   # Remove Lymalink and resources from system
frontend/build.sh dev         # Clean + debug build, launch
```

**Build modes:**

| Command       | Build type | Output                        | Binary location                      |
|---------------|-----------|-------------------------------|--------------------------------------|
| `debug`       | Debug     | `frontend/build/debug/`       | `build/debug/bin/Lymalink`           |
| `release`     | Release   | `frontend/build/release/`     | `build/release/bin/Lymalink`         |
| `deploy`      | Release   | XDG user dirs (see below)     | `~/.local/bin/Lymalink`              |
| `dev`         | Debug     | `/tmp/lymalink-build/` + launch | `build/debug/bin/Lymalink`         |

**Deploy installs to standard XDG directories:**

| File | Location |
|------|----------|
| Binary | `~/.local/bin/Lymalink` |
| Icon | `~/.local/share/icons/hicolor/256x256/apps/lymalink.png` |
| Desktop entry | `~/.local/share/applications/lymalink.desktop` |

### Backend

`lymalinkd` is the Linux backend daemon. It can be built with `backend/build.sh` from the repository root:

```bash
backend/build.sh clean
backend/build.sh debug
backend/build.sh release
backend/build.sh test
```

Debug and release binaries are built under `/tmp/lymalinkd-build` by default.

To install and manage the user service:

```bash
backend/build.sh deploy
backend/build.sh start
backend/build.sh stop
backend/build.sh restart
backend/build.sh status
backend/build.sh logs
backend/build.sh uninstall
```

For backend requirements, direct `make` usage, tests, local run commands, and service details, see [backend/README.md](backend/README.md).

---

## Credits
See [CREDITS.md](frontend/res/docs/CREDITS.md) for third-party sound assets used in this project.

## Disclaimer

### General

Lymalink is an independent, open-source project and is **not affiliated with, authorized by, maintained by, sponsored by, or endorsed by** Valve Corporation, Steam, GOG, or any other platform, company, or service referenced in this software.

All trademarks, service marks, trade names, logos, and brand assets mentioned or displayed in this project are the property of their respective owners. Their use here is solely for identification and reference purposes and does not imply any association with or endorsement by those owners.

### Steam Emulator Support

Lymalink is capable of reading local achievement data files generated by Steam emulators. This functionality is provided **strictly for personal, informational use**.

This software:
- Does **not** provide, distribute, or assist in obtaining Steam emulators
- Does **not** interact with Steam or any platform service beyond explicit API calls made by the user
- Does **not** bypass any digital rights management (DRM) system or platform protection
- Is **not** affiliated with or associated with any scene group, cracking group, or piracy community

The authors of this project do not condone or support any illegal activity. Users are solely responsible for ensuring their use of this software complies with applicable laws and the terms of service of any platform they interact with.

### Official Steam API / User Data

Lymalink supports importing a user's own Steam achievement data via the official Steam Web API. This feature requires the user to supply their own Steam Web API key, obtained directly from Valve. No Steam credentials or private data are collected, stored, or transmitted by this application beyond what the user explicitly configures.

For more information on obtaining a Steam Web API key, see: https://steamcommunity.com/dev/apikey

### No Warranty

This software is provided **as-is**, without any express or implied warranty. In no event shall the authors be held liable for any damages arising from the use of, or inability to use, this software.
