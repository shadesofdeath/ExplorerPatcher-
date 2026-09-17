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

// Reads the tweak values from the ExplorerPatcher registry key (called from LoadSettings).
void EssentialTweaks_LoadSettings(HKEY hKey);

// Installs inline hooks; call once per explorer.exe process, before the other hooks are committed.
void EssentialTweaks_PrepareHooks(void);

// Starts the background workers (tray icon fix, tab history); call once from the shell explorer.exe process.
void EssentialTweaks_Start(void);

// Called from the Shell_TrayWnd / Shell_SecondaryTrayWnd subclass on WM_MOUSEWHEEL.
// Returns TRUE when the message was consumed.
BOOL EssentialTweaks_OnTaskbarMouseWheel(HWND hWnd, WPARAM wParam, LPARAM lParam);

// Called from the CreateWindowExW hook after a window was created.
void EssentialTweaks_OnWindowCreated(HWND hWnd, HWND hWndParent);

#ifdef __cplusplus
}
#endif

#endif
