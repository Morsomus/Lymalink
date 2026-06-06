# User Guide

###

## Interface Overview

###

### 🧭 Sidebar & Menus

|  |  |
| --- | --- |
| **Dashboard** | View tracked targets and achievement progress. |
| **Settings** | Configure app, display, background service, customization and notifications. |
| **Currently Playing** | Displays the active tracked game. |
| **Service Status** | Shows background service status. Click to open Settings. |
| **GitHub** | Opens the Lymalink GitHub repository. |
| **System Tray** | Right-click the tray icon to restore the window or fully exit Lymalink. |

###
### 🖥️ Dashboard

|  |  |
| --- | --- |
| **Search & Filters** | Find targets by name/ID, or filter by status (Completed, Emulator, Hidden, Installed). |
| **View & Sort** | Change layouts (List, Details, Small, Default card) and sort by playtime, progress, or dates. |
| **Add Target** | Setup new targets (For **Emulators**: search game, select executable and prefix). |
| **Target Card** | Click to access game details, achievement lists, and target settings. |
| **Refresh** | Reloads dashboard data. |

###
### 🎯 Target Details

|  |  |
| --- | --- |
| **Back Arrow** | Return to Dashboard (or press `Backspace` / `Escape`). |
| **Settings Gear** | Reload data, update imported progress, edit paths, hide, or delete the target entirely. |
| **Display Options** | Filter, sort, and order the displayed achievements. |
| **Achievement Icon** | Click to manually lock/unlock an achievement. |
| **Hidden Achievement** | Click a hidden achievement row to reveal or conceal its details. |

###
## 🎮 Managing Emulator Targets

In Lymalink, a **Target** refers to any tracked item. The Emulator mode focuses on **Steam** based achievement file targets and may work with achievement files created by CODEX, RUNE, and GOLDBERG.

### ➕ Add an Emulator Target

1. Open **Dashboard** and click **Add Target**.
2. Select **Emulator**.
3. Search for the game and select it from results. You can also enable **enter manually** and enter game ID and name yourself.
4. Select **Game Executable**. This `.exe` file is used to detect when game is running.
5. Select **Prefix Location**. Choose `drive_c` or equivalent folder inside your Wine, Proton, or Bottle prefix which contains or will contain emulator's achievement file.
6. Click **Confirm**.

Example prefix locations:

- **Wine:** `/home/<user>/.wine/drive_c`
- **Flatpak Bottles:** `/home/<user>/.var/app/com.usebottles.bottles/data/bottles/bottles/games/drive_c/`
- **Flatpak Heroic Launcher:** `/home/<user>/Games/Heroic/Prefixes/Helltaker/drive_c`
- **Heroic prefix scan root:** `/home/<user>/Games/Heroic/Prefixes`

Use the Heroic prefix scan root when you want Lymalink to scan all game prefixes below that folder.

### 🗑️ Remove an Emulator Target

**Warning:** Deleting a target will remove it from Lymalink, along with all downloaded assets and achievement data.

1. Open target card from **Dashboard**.
2. Click **Settings gear**.
3. Click **Delete**.
4. Type `delete`.
5. Click **Confirm**.

---

## 🌐 Managing Steam Import Targets

You can import your official Steam library and sync achievements directly. This feature requires configuring your Steam ID and API key.

### 🔑 Before Use

* Please configure both your **Steam ID** and **Web API key** in **Settings**.
* Please ensure your **Steam profile privacy** is set to **Public**.

### ➕ Add / Import Steam Games

1. Open **Dashboard** and click **Add Target**.
2. Select **Steam Import / Update**.
3. Click **Load Steam Library**.
4. Enter your passcode.
5. Select games to import (or uncheck to remove).
6. Click **Apply Selection**.

### 🔄 Update Steam Library & Achievements

1. Open **Dashboard** and click **Add Target**.
2. Select **Steam Import / Update**.
3. Press **Update**.

Or at Target Details
1. Open target card from **Dashboard**.
2. Click **Settings gear**.
3. Click **Reload Steam Progress**.

### 🗑️ Remove Steam Import Games

1. Open **Dashboard** and click **Add Target**.
2. Select **Steam Import / Update**.
3. Click **Load Steam Library**.
4. Enter your passcode.
5. Uncheck the games you want to remove.
6. Click **Apply Selection**.

*(Note: You can also delete an imported Steam game individually by using the Settings gear -> Delete -> type `delete` on its target card).*

---

## 👁️ General Target Options (Hide / Show)

Hide targets you do not want to see on Dashboard without deleting their data. Works for both Emulator and Steam targets.

1. Open target card from **Dashboard**.
2. Click **Settings gear**.
3. Click **Hide**.

Hidden targets disappear from normal Dashboard view. To show one again:

1. Open Dashboard **Filter**.
2. Select **Hidden**.
3. Open hidden target card.
4. Click **Settings gear**, then **Unhide**.

---

###
### 🔔 Achievement Notifications

Achievement notifications appear as an in-game overlay. Vulkan games should work automatically. OpenGL games need an extra launch setting:

**Note:** After modifying any launch options or environment variables or updating Lymalink, make sure to restart Steam, Heroic, Bottles and/or Wine (wineserver -k) for the changes to take effect properly.

**Note:** Flatpak support is currently limited to launchers listed below.

**Note:** Replace **\<user\>** with your home directory username.

###
###

Type | Launcher | OpenGL setup |
| --- | --- | --- |
| **NATIVE** | **Wine command line** | Run: `lymalink-overlay wine "/path/to/game.exe"` |
| **NATIVE** | **Steam** | Add launch option: `lymalink-overlay %command%` |
| **FLATPAK** | **Heroic Launcher** | Add environment variable: `LD_PRELOAD=/usr/lib/extensions/vulkan/lymalink/lib/x86_64-linux-gnu/lymalink-overlay-preloader.so` |
| **NATIVE** | **Heroic Launcher** | Add environment variable: `LD_PRELOAD=/home/<user>/.local/lib/lymalink-overlay-preloader.so` |
| **FLATPAK** | **Bottles** | Add environment variable: `LD_PRELOAD=/usr/lib/extensions/vulkan/lymalink/lib/x86_64-linux-gnu/lymalink-overlay-preloader.so` |
| **NATIVE** | **Lutris** | Add environment variable (wine): `LD_PRELOAD=/home/<user>/.local/lib/lymalink-overlay-preloader.so` |

###
# Frequently Asked Questions & Troubleshooting

###
### How do achievements appear on the Dashboard?
They update automatically when `lymalinkd` is running and the game executable path and prefix path are correctly configured.
- The prefix path **MUST** contain the achievement file within one of its subfolders.
- The executable path **MUST** be set correctly. The background service only scans when a game is actively being played; otherwise, it sleeps and only checks for the presence of the configured executables.

###
### How do I check if I have configured my target correctly?
- Start the game and check the sidebar; the running game should be detected.
- Open the target details and check that 'Status' shows the 'Installed' state.
- Open the target details and check that 'Achievement file' shows the 'Found' state. Note that this may display 'Missing' until the game is launched for the first time and the emulator creates the achievement file. Also note that some emulators do not create the achievement file until the very first achievement is unlocked in-game. If you are playing a brand-new game, the status may safely display 'Missing' until your first unlock. Just make sure that you have set correct **Prefix Location** for your target.

If all of the above conditions are met, you are good to go.

###
### Why are achievements not being detected?
If your progress does not update:
- Check the service status displayed on the sidebar; it should show a solid or breathing green indicator. You can also try restarting the background service on the Settings page.
- Ensure that your gaming session is detected by the service during gameplay by viewing the status on the sidebar. If it is not detected, verify that the executable path is set correctly by opening the target details and checking that the Status shows 'Installed'. You can change the executable installation path from the target details page by clicking the settings gear icon at the top and selecting "Edit Executable Location".
- Open the target details and check that the 'Achievement file' status shows 'Found'. If it shows 'Missing', ensure your prefix path is correct. The subfolders of the prefix path must contain the achievement file created by the emulator.
* **Note:** Some emulators do not create the achievement file until the very first achievement is unlocked in-game. If you are playing a brand-new game, the status may safely display 'Missing' until your first unlock.

###
### I can hear the notification sound, but I don't see the achievement overlay. What's wrong?
If the sound plays but the visual notification is missing, consider the following:
- **OpenGL Games:** Unlike Vulkan games which usually work automatically, OpenGL games require an extra environment variable configuration to hook the overlay. Please refer to the **Achievement Notifications** section above and ensure the `LD_PRELOAD` environment variable or launcher options are set correctly.
- **Beta Phase Limitations:** The in-game notification overlay is complex and highly experimental. Since Lymalink is currently in beta, the overlay may require platform-specific tweaks to display correctly depending on your system configuration.
- **Tested Environment:** So far, this feature has only been tested in limited environments using the Flatpak versions of Heroic Games Launcher and Bottles, installed specifically to user directories (not system-wide). If you are using system-level installations or different launchers, compatibility may vary.
- **Test the Overlay:** You can trigger a manual test notification from the **Settings** page under the **Background Service** section. Note that for the test to work, your game **must be running** and correctly configured so that Lymalink detects your active gaming session.

If you encounter any bugs, rendering issues, or if the overlay does not appear on your specific setup, please report them in our repository's **Issues** section.

###
### Can I use Lymalink without the Dashboard?
Yes. In Settings, you can allow the background service to run independently by enabling 'Track in Background'. The Dashboard is entirely optional once tracking has been configured.

###
### Lymalink was not active during gameplay, and I missed an achievement!
No worries. You can manually "unlock" achievements on the target details page. Simply click the achievement icon and set the desired date and time.

###
### Which emulator created achievement files are currently supported?
Lymalink currently supports parsing local achievement files generated by emulators listed below:
- **CODEX**
- **RUNE**
- **GOLDBERG**

###
### Do I need a Steam API key?
* **For Steam Import:** **Yes.** If you want to use the Steam Import feature to fetch your library and official achievements, you must configure your Steam ID and Steam Web API key in the Settings menu.
* **For Emulator created achievement files:** No. Achievement file tracking does not require a Steam API key, as covers and baseline achievement lists are downloaded from publicly accessible APIs.

###
### Support & Development
Development updates, releases, and source code are available on our official GitHub repository. If you encounter any issues, please report them in the repository **Issues** section. You can access it by clicking the link on the sidebar.
