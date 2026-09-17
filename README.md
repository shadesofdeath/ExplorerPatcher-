# ExplorerPatcher++

ExplorerPatcher++ is a fork of [ExplorerPatcher](https://github.com/valinet/ExplorerPatcher) that adds an extra
**"Essential Tweaks"** page to the Properties window. The original pages and options are left untouched; every new
tweak lives on the new page and is turned off by default.

## Essential Tweaks

The tweaks are C ports of the following [Windhawk mods](https://github.com/ramensoftware/windhawk-mods):

| Tweak | Based on | Author |
|-------|----------|--------|
| Change the system volume by scrolling over the taskbar (entire taskbar or notification area only, configurable step) | [Taskbar Volume Control](https://windhawk.net/mods/taskbar-volume-control) | m417z |
| Fix disappearing tray icons by re-broadcasting `TaskbarCreated` a few seconds after the taskbar starts | [Disappearing Tray Icons Fix](https://windhawk.net/mods/taskbar-disappearing-tray-icons-fix) | Alchemy |
| Do not ask for confirmation when changing a file name extension | [Turn off change file extension warning](https://windhawk.net/mods/extension-change-no-warning) | m417z |
| Double click an empty area in File Explorer to go up one folder | [Explorer Double Click Up](https://windhawk.net/mods/explorer-double-click-up) | wrldspawn |
| Ctrl+Shift+T reopens the last closed File Explorer tab (Windows 11 22H2 or newer) | [File Explorer Reopen Closed Tab](https://windhawk.net/mods/file-explorer-reopen-closed-tab) | Armaninyow |

The implementation lives in [`ExplorerPatcher/EssentialTweaks.c`](ExplorerPatcher/EssentialTweaks.c). Settings are
stored as `Essential*` values under `HKCU\Software\ExplorerPatcher`, next to the regular ExplorerPatcher settings.

Everything below this line is the original ExplorerPatcher documentation; it applies to ExplorerPatcher++ as well.

---

# ExplorerPatcher

This project aims to enhance the working environment on Windows.

## How to?

1. Download the latest setup program from the [Releases page](https://github.com/valinet/ExplorerPatcher/releases/latest).
   * Choose `ep_setup.exe` if your device uses an Intel or AMD processor, or `ep_setup_arm64.exe` if your device uses a Snapdragon processor.
2. Run the installer. It will automatically prompt for elevation, after which it will close `explorer.exe` and install the necessary files. When done, you will see the desktop again and the Windows 10 taskbar.
3. Right-click the taskbar and choose "Properties".
4. To change the taskbar style, go to the "Taskbar" section and look for "Taskbar style".
5. To use the Windows 10 Start menu, go to the "Start menu" section and change the Start menu style to Windows 10.
6. To use the Windows 10 Alt+Tab, go to the "Window switcher" section and change the "Window switcher (Alt+Tab) style" to Windows 10.
7. Feel free to check other configuration options.

That's it!

**Note:** Some features may be unavailable on some Windows versions.

## Uninstalling

* Right-click the taskbar then click "Properties" or search for "ExplorerPatcher", and go to "Uninstall" section or
* Use "Programs and Features" in Control Panel, or "Apps and features" in the Settings app or
* Run `ep_setup.exe /uninstall` or
* Rename `ep_setup.exe` to `ep_uninstall.exe` and run that.

## Updating

* The program features built-in updates: go to "Properties" - "Updates" to configure, check for and install the latest updates. Learn more [here](https://github.com/valinet/ExplorerPatcher/wiki/Configure-updates).
* Download the latest version's [setup file for x64](https://github.com/valinet/ExplorerPatcher/releases/latest/download/ep_setup.exe) or [setup file for ARM64](https://github.com/valinet/ExplorerPatcher/releases/latest/download/ep_setup_arm64.exe) and simply run it.

## Donate

If you find this project essential to your daily life, please consider donating to support the development through the [Sponsor](https://github.com/valinet/ExplorerPatcher?sponsor) button at the top of this page, so that we can continue to keep supporting newer Windows builds.

## Discord Server

Join our Discord server if you need support, want to chat regarding this project, or just want to hang out with us!

[![Join on Discord](https://discordapp.com/api/guilds/1155912047897350204/widget.png?style=shield)](https://discord.gg/gsPcfqHTD2)

[Read more](https://github.com/valinet/ExplorerPatcher/wiki)
