# Frontend Build

Build information for the Linux Qt frontend.

## Supported Build Host

- Linux `x86_64`

## Dependencies

Install these packages, or your distro's equivalent:

```bash
sudo apt update
sudo apt install \
  build-essential \
  cmake \
  libgl1-mesa-dev \
  libssl-dev \
  pkg-config \
  qt6-base-dev \
  qt6-base-dev-tools \
  qt6-declarative-dev \
  qt6-tools-dev \
  qt6-tools-dev-tools \
  qml6-module-qt5compat-graphicaleffects \
  qml6-module-qtquick \
  qml6-module-qtquick-controls \
  qml6-module-qtquick-dialogs \
  qml6-module-qtquick-effects \
  qml6-module-qtquick-layouts \
  qml6-module-qtquick-shapes \
  qml6-module-qtquick-window
```

## Build Script

Run from repository root:

```bash
frontend/build.sh clean
frontend/build.sh debug
frontend/build.sh release
frontend/build.sh test
frontend/build.sh test --silent
frontend/build.sh dev
```

Build outputs:

| Command | Output | Binary |
|---------|--------|--------|
| `debug` | `frontend/build/debug/` | `frontend/build/debug/bin/Lymalink` |
| `release` | `frontend/build/release/` | `frontend/build/release/bin/Lymalink` |
| `dev` | `frontend/build/debug/` and launch | `frontend/build/debug/bin/Lymalink` |

## Run Locally

```bash
frontend/build.sh dev
```

Logs:

```bash
tail -f ~/.local/state/lymalink/lymalink-frontend.log
```

## Deploy

```bash
frontend/build.sh deploy
frontend/build.sh deploy --debug
frontend/build.sh uninstall
```

Deploy installs:

| File | Location |
|------|----------|
| Binary | `~/.local/bin/Lymalink` |
| Icon | `~/.local/share/icons/hicolor/256x256/apps/lymalink.png` |
| Desktop entry | `~/.local/share/applications/lymalink.desktop` |

## Direct CMake Build

```bash
cmake -S frontend -B frontend/build/debug -DCMAKE_BUILD_TYPE=Debug
cmake --build frontend/build/debug --parallel "$(nproc)"

cmake -S frontend -B frontend/build/release -DCMAKE_BUILD_TYPE=Release
cmake --build frontend/build/release --parallel "$(nproc)"
```

## Tests

```bash
frontend/build.sh test
frontend/build.sh test --silent
```

## Frontend Layout

```text
frontend
├── build.sh
├── CMakeLists.txt
├── common
│   ├── ConfirmationPopup.qml
│   ├── CustomBusyIndicator.qml
│   ├── CustomTooltip.qml
│   ├── ErrorImage.qml
│   ├── ErrorPopup.qml
│   └── MarkdownDocumentPopup.qml
├── Main.qml
├── README.md
├── res
│   ├── docs
│   │   ├── CREDITS.md
│   │   ├── help
│   │   │   └── user-guide-0.8.0-beta.md
│   │   ├── LICENSE.md
│   │   └── THIRD-PARTY-LICENSES-LINUX.md
│   ├── fonts
│   │   └── Inter
│   │       ├── Inter-Italic-VariableFont_opsz,wght.ttf
│   │       ├── Inter-VariableFont_opsz,wght.ttf
│   │       └── OFL.txt
│   ├── img
│   │   └── *.png
│   └── img-external
│       ├── GitHub_Invertocat_White_Clearspace.png
│       └── GitHub_notice.txt
├── src
│   ├── api
│   │   ├── SteamApi.cpp
│   │   ├── SteamApi.h
│   │   ├── SteamApiHydrationWorker.cpp
│   │   ├── SteamApiHydrationWorker.h
│   │   ├── SteamApiSearchWorker.cpp
│   │   └── SteamApiSearchWorker.h
│   ├── database
│   │   ├── SQLiteManager.cpp
│   │   └── SQLiteManager.h
│   ├── Defines.h
│   ├── Error.h
│   ├── ipc
│   │   ├── DBusService.cpp
│   │   └── DBusService.h
│   ├── Lymalink.cpp
│   ├── Lymalink.h
│   ├── main.cpp
│   ├── Settings.cpp
│   ├── Settings.h
│   ├── SysTray.cpp
│   ├── SysTray.h
│   └── tools
│       ├── Encryption.cpp
│       ├── Encryption.h
│       ├── FileManager.cpp
│       ├── FileManager.h
│       ├── ImageCacheManager.cpp
│       ├── ImageCacheManager.h
│       ├── Logger.cpp
│       ├── Logger.h
│       ├── Utils.cpp
│       └── Utils.h
├── tests
│   ├── EncryptionTests.cpp
│   ├── FileManagerTests.cpp
│   ├── SQLiteManagerTests.cpp
│   └── SteamApiTests.cpp
├── Themes.qml
└── views
    ├── Main
    │   ├── Dashboard
    │   │   ├── CardGrid
    │   │   │   ├── CardGrid.qml
    │   │   │   └── Components
    │   │   │       ├── Card.qml
    │   │   │       └── CardSmall.qml
    │   │   ├── CardList
    │   │   │   ├── CardList.qml
    │   │   │   └── Components
    │   │   │       ├── CardRowDetailed.qml
    │   │   │       └── CardRow.qml
    │   │   ├── Dashboard.qml
    │   │   ├── DashboardToolbar.qml
    │   │   ├── NewTarget
    │   │   │   ├── NewTarget.qml
    │   │   │   └── Targets
    │   │   │       ├── Custom.qml
    │   │   │       ├── Emulator.qml
    │   │   │       └── SteamImport.qml
    │   │   └── TargetDetails
    │   │       ├── TargetAchievementEditPopup.qml
    │   │       ├── TargetDetails.qml
    │   │       └── TargetSettings.qml
    │   └── Settings
    │       └── Settings.qml
    └── Sidebar
        ├── Components
        │   ├── BackendServiceElement.qml
        │   ├── GHElement.qml
        │   └── SidebarButton.qml
        └── Sidebar.qml
```
