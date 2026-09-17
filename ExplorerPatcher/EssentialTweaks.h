#ifndef _H_ESSENTIALTWEAKS_H_
#define _H_ESSENTIALTWEAKS_H_
//
// ExplorerPatcher++ "Essential Tweaks"
//
// Extra tweaks ported to C from Windhawk mods (https://github.com/ramensoftware/windhawk-mods):
//   * taskbar-volume-control            (m417z)       - change the system volume by scrolling over the taskbar
//   * taskbar-disappearing-tray-icons-fix (Alchemy)   - rebroadcast TaskbarCreated so tray icons that missed it re-register
//   * extension-change-no-warning       (m417z)       - suppress the "file might become unusable" rename warning
//   * explorer-double-click-up          (wrldspawn)   - double click empty space in File Explorer to go up one folder
//   * file-explorer-reopen-closed-tab   (Armaninyow)  - Ctrl+Shift+T reopens the last closed File Explorer tab
//   * taskbar-empty-space-clicks        (m1lhaus)     - double / middle click on empty taskbar space runs an action
//   * hide-home-gallery-explorer        (registry based variant) - hide Home / Gallery / OneDrive in the navigation pane
//   * explorer-single-window-tabs       (ALMAS CP)    - new File Explorer windows open as tabs of an existing window
//   * zen-desktop-toggle-icons          (Lanbo, manual toggle only) - double click the desktop to hide / show the icons
//   * hide-desktop-icon-text            (kivsak)      - hide desktop icon labels and shortcut arrows
//   * fix-explorer-white-flash          (Mgg Sk)      - no white flash when File Explorer views open in dark mode
//   * taskbar-notification-icons-show-all (m417z)     - always show all tray icons on the Windows 11 taskbar
//   * startup-app-delay-fix             (meteoni, registry based variant) - no delay before startup apps are launched
//   * f1-blocker                        (d0gkiller87) - F1 does not open the help web page in File Explorer
//   * audio-scroll-switcher             (Georgi AI)   - Ctrl + scroll over the taskbar switches the audio output device
//   * remove-ms-store-open-with         (aubymori)    - no "Search the Microsoft Store" item in Open with
//   * disable-folder-thumbnails         (Anixx)       - folders keep their icon, files keep their thumbnails
//   * center-new-windows                (crazyboyybs, WinEvent based variant) - new windows open centered
//   * auto-theme-switcher               (tinodin, fixed hours only) - light / dark mode on a schedule
//   * context-menu-preloader            (Lockframe)   - context menu handlers are loaded ahead of the first right click
//
// All tweaks are disabled by default and are configured from the "Essential Tweaks" page of the Properties window.
//
#include <Windows.h>

#ifdef __cplusplus
extern "C"
{
#endif

// 0 = off, 1 = whole taskbar, 2 = notification area only
extern DWORD dwEssentialTaskbarVolumeScroll;
// Volume change per wheel notch, in percent (even number, 2..20)
extern DWORD dwEssentialVolumeStep;
// 0 = off, otherwise the delay in seconds before TaskbarCreated is rebroadcast
extern DWORD dwEssentialTrayIconFix;
extern DWORD bEssentialDisableExtensionWarning;
extern DWORD bEssentialExplorerDoubleClickUp;
extern DWORD bEssentialReopenClosedTab;
// 0 = nothing, 1 = show desktop, 2 = Task Manager, 3 = Start menu, 4 = mute, 5 = media play/pause,
// 6 = toggle taskbar auto-hide, 7 = Task View (Win+Tab), 8 = lock the computer
extern DWORD dwEssentialTaskbarDoubleClickAction;
extern DWORD dwEssentialTaskbarMiddleClickAction;
// Applied through the registry when the settings are loaded (System.IsPinnedToNameSpaceTree)
extern DWORD bEssentialHideExplorerHome;
extern DWORD bEssentialHideExplorerGallery;
extern DWORD bEssentialHideExplorerOneDrive;
// New File Explorer windows open as tabs of an existing window (Windows 11 22H2+; hold Shift to bypass)
extern DWORD bEssentialSingleWindowTabs;
extern DWORD bEssentialDesktopToggleIcons;
extern DWORD bEssentialHideDesktopIconText;
extern DWORD bEssentialHideShortcutArrows;
extern DWORD bEssentialFixExplorerWhiteFlash;
extern DWORD bEssentialShowAllTrayIcons;
extern DWORD bEssentialNoStartupDelay;
extern DWORD bEssentialBlockF1Help;
extern DWORD bEssentialAudioDeviceScroll;
extern DWORD bEssentialRemoveStoreOpenWith;
extern DWORD bEssentialDisableFolderThumbnails;
extern DWORD bEssentialCenterNewWindows;
// Light mode from dwEssentialAutoThemeLightHour (0..23), dark mode from dwEssentialAutoThemeDarkHour
extern DWORD bEssentialAutoTheme;
extern DWORD dwEssentialAutoThemeLightHour;
extern DWORD dwEssentialAutoThemeDarkHour;
extern DWORD bEssentialPreloadContextMenu;

// Reads the tweak values from the ExplorerPatcher registry key (called from LoadSettings).
void EssentialTweaks_LoadSettings(HKEY hKey);

// Installs inline hooks; call once per explorer.exe process, before the other hooks are committed.
void EssentialTweaks_PrepareHooks(void);

// Starts the background workers (tray icon fix, tab history); call once from the shell explorer.exe process.
void EssentialTweaks_Start(void);

// Called from the Shell_TrayWnd / Shell_SecondaryTrayWnd subclass on WM_MOUSEWHEEL.
// Returns TRUE when the message was consumed.
BOOL EssentialTweaks_OnTaskbarMouseWheel(HWND hWnd, WPARAM wParam, LPARAM lParam);

// Windows 10 style taskbars: called from the Shell_TrayWnd / Shell_SecondaryTrayWnd subclass for the (non-client)
// left double click and middle button messages, which only reach the taskbar window for clicks on empty space.
// Returns TRUE when an action was performed.
BOOL EssentialTweaks_OnTaskbarMouseMessage(HWND hWnd, UINT uMsg);

// Windows 11 taskbar on builds that deliver regular mouse messages: called from the taskbar thread's WH_MOUSE hook.
void EssentialTweaks_OnTaskbarThreadMouseHook(WPARAM wMouseMsg, const MOUSEHOOKSTRUCT* pMouse);
void EssentialTweaks_PerformTaskbarAction(DWORD dwAction);

// Windows 11 taskbar on builds that deliver WM_POINTER* messages: the window procedure of the XAML island's input
// window is hooked from a timer of the taskbar window. Call OnTaskbarWindowCreated after subclassing a taskbar window, and OnTaskbarTimer
// from its WM_TIMER handler (returns TRUE when the timer belongs to this module).
void EssentialTweaks_OnTaskbarWindowCreated(HWND hTaskbar);
BOOL EssentialTweaks_OnTaskbarTimer(HWND hTaskbar, WPARAM idTimer);

// Implemented in dllmain.c (UIAutomationClient.h defines symbols with external linkage and can only be included
// in one translation unit): returns the UI Automation class name of the element at the given screen point.
BOOL EssentialTweaks_GetUIAutomationClassNameAtPoint(POINT pt, WCHAR* wszClass, size_t cch);

// Called from the CreateWindowExW hook after a window was created (hWndParent is NULL for top-level windows).
void EssentialTweaks_OnWindowCreated(HWND hWnd, HWND hWndParent);

#ifdef __cplusplus
}
#endif

#endif
