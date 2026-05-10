# Lymalink

**Achievement tracker for Steam emulators, Steam/Cloud APIs, and fully custom targets.**

> **Note:** Lymalink is currently in early development and is not yet functional. Features and documentation are subject to change.

## Overview

Lymalink is a cross-platform application designed to monitor local game achievement files and synchronize data through cloud APIs. It provides a unified system for tracking milestones across Steam emulators and official environments alike. Beyond gaming, the platform offers the flexibility to create custom achievements which can be tracked from various sources.

## Table of Contents

- [Project Status](#project-status)
- [Planned Features](#planned-features)
- [Building](#building)
  - [Requirements](#requirements)
  - [Build Script](#build-script)
- [Disclaimer](#disclaimer)
  - [General](#general)
  - [Steam Emulator Support](#steam-emulator-support)
  - [Official Steam API / User Data](#official-steam-api--user-data)
  - [No Warranty](#no-warranty)

## Project Status

### Linux
| Component | Status |
|-----------|--------|
| **Frontend** | 🚧 In Development |
| &nbsp;&nbsp;&nbsp;&nbsp;Side Navigation Bar | ✅ Ready to Deploy |
| &nbsp;&nbsp;&nbsp;&nbsp;Side Navigation Bar Business Logic | 📋 Planning |
| &nbsp;&nbsp;&nbsp;&nbsp;Settings | 🚧 In Development |
| &nbsp;&nbsp;&nbsp;&nbsp;Settings Business Logic | 📋 Planning |
| &nbsp;&nbsp;&nbsp;&nbsp;Dashboard | ✅ Ready to Deploy |
| &nbsp;&nbsp;&nbsp;&nbsp;Dashboard Business Logic | 📋 Planning |
| &nbsp;&nbsp;&nbsp;&nbsp;Achievement Progress Details | 📋 Planning |
| &nbsp;&nbsp;&nbsp;&nbsp;Achievement Progress Details Business Logic | 📋 Planning |
| &nbsp;&nbsp;&nbsp;&nbsp;Statistics | 🔴 To Be Started |
| &nbsp;&nbsp;&nbsp;&nbsp;Statistics Business Logic | 🔴 To Be Started |
| **Backend Service** | 📋 Planning |

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

## Building

> **Note:** The following build instructions apply currently only to Linux.

### Requirements

**Build dependencies:**

- CMake >= 3.16
- Qt 6 (Core, Gui, Qml, Quick, Widgets modules)
- C++20-compatible compiler (GCC 10+, Clang 10+, or equivalent)
- Qt6 qml module tools (`qt6-qml-module`/`qtcreator`)

**Recommended (for running):**

- Linux Desktop Environment
- X11 or Wayland compositor
- Plasma/XDG desktop integration for automatic app menu entry

### Build Script

The `frontend/build.sh` script automates the full build workflow. Run it from the repository root or `frontend/` directory:

```bash
chmod +x frontend/build.sh
frontend/build.sh clean       # Remove build/
frontend/build.sh debug       # Debug build   -> frontend/build/debug/
frontend/build.sh release     # Release build -> frontend/build/release/
frontend/build.sh deploy      # Clean + release build, strip binary, copy to ~/Apps/Lymalink/ and create desktop entry
frontend/build.sh dev         # Clean + debug build, launch
```

**Build modes:**

| Command       | Build type | Output                        | Binary location                      |
|---------------|-----------|-------------------------------|--------------------------------------|
| `debug`       | Debug     | `frontend/build/debug/`       | `build/debug/bin/Lymalink`           |
| `release`     | Release   | `frontend/build/release/`     | `build/release/bin/Lymalink`         |
| `deploy`      | Release   | `~/Apps/Lymalink/`            | `~/Apps/Lymalink/Lymalink`           |
| `dev`         | Debug     | `~/Apps/Lymalink/` + launch   | `~/Apps/Lymalink/Lymalink`           |

---

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