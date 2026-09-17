#include "EssentialTweaks.h"
#if WITH_MAIN_PATCHER
#include "hooking.h"
#endif
#include <initguid.h>
#include <Windows.h>
#include <windowsx.h>
#include <commctrl.h>
#include <shlobj.h>
#include <shlwapi.h>
#include <shellapi.h>
#include <uxtheme.h>
#include <commoncontrols.h>
#include <exdisp.h>
#include <servprov.h>
#include <mmdeviceapi.h>
#include <endpointvolume.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>
#include "osutility.h"

#pragma comment(lib, "Comctl32.lib")

DWORD dwEssentialTaskbarVolumeScroll = 0;
DWORD dwEssentialVolumeStep = 2;
DWORD dwEssentialTrayIconFix = 0;
DWORD bEssentialDisableExtensionWarning = FALSE;
DWORD bEssentialExplorerDoubleClickUp = FALSE;
DWORD bEssentialReopenClosedTab = FALSE;
DWORD dwEssentialTaskbarDoubleClickAction = 0;
DWORD dwEssentialTaskbarMiddleClickAction = 0;
DWORD bEssentialHideExplorerHome = FALSE;
DWORD bEssentialHideExplorerGallery = FALSE;
DWORD bEssentialHideExplorerOneDrive = FALSE;
DWORD bEssentialSingleWindowTabs = FALSE;
DWORD bEssentialDesktopToggleIcons = FALSE;
DWORD bEssentialHideDesktopIconText = FALSE;
DWORD bEssentialHideShortcutArrows = FALSE;
DWORD bEssentialFixExplorerWhiteFlash = FALSE;
DWORD bEssentialShowAllTrayIcons = FALSE;
DWORD bEssentialNoStartupDelay = FALSE;
DWORD bEssentialBlockF1Help = FALSE;

// GUIDs are defined locally so that the module does not depend on uuid.lib / the SDK's MIDL-generated definitions.
static const GUID ET_CLSID_MMDeviceEnumerator = { 0xBCDE0395, 0xE52F, 0x467C, { 0x8E, 0x3D, 0xC4, 0x57, 0x92, 0x91, 0x69, 0x2E } };
static const GUID ET_IID_IMMDeviceEnumerator = { 0xA95664D2, 0x9614, 0x4F35, { 0xA7, 0x46, 0xDE, 0x8D, 0xB6, 0x36, 0x17, 0xE6 } };
static const GUID ET_IID_IAudioEndpointVolume = { 0x5CDF2C82, 0x841E, 0x4546, { 0x97, 0x22, 0x0C, 0xF7, 0x40, 0x78, 0x22, 0x9A } };
static const GUID ET_CLSID_ShellWindows = { 0x9BA05972, 0xF6A8, 0x11CF, { 0xA4, 0x42, 0x00, 0xA0, 0xC9, 0x0A, 0x8F, 0x39 } };
static const GUID ET_IID_IShellWindows = { 0x85CB6900, 0x4D95, 0x11CF, { 0x96, 0x0C, 0x00, 0x80, 0xC7, 0xF4, 0xEE, 0x85 } };
static const GUID ET_IID_IServiceProvider = { 0x6D5140C1, 0x7436, 0x11CE, { 0x80, 0x34, 0x00, 0xAA, 0x00, 0x60, 0x09, 0xFA } };
static const GUID ET_SID_STopLevelBrowser = { 0x4C96BE40, 0x915C, 0x11CF, { 0x99, 0xD3, 0x00, 0xAA, 0x00, 0x4A, 0xE8, 0x37 } };
static const GUID ET_IID_IShellBrowser = { 0x000214E2, 0x0000, 0x0000, { 0xC0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x46 } };
static const GUID ET_IID_IWebBrowser2 = { 0xD30C1661, 0xCDAF, 0x11D0, { 0x8A, 0x3E, 0x00, 0xC0, 0x4F, 0xC9, 0xE2, 0x6E } };
static const GUID ET_IID_IFolderView = { 0xCDE725B0, 0xCCC9, 0x4519, { 0x91, 0x7E, 0x32, 0x5D, 0x72, 0xFA, 0xB4, 0xCE } };
static const GUID ET_IID_IPersistFolder2 = { 0x1AC3D9F0, 0x175C, 0x11D1, { 0x95, 0xBE, 0x00, 0x60, 0x97, 0x97, 0xEA, 0x4F } };

#define ET_CLASSNAME_CCH 64

// Actions that can be assigned to a double click / middle click on empty taskbar space.
#define ET_TASKBAR_ACTION_NONE 0
#define ET_TASKBAR_ACTION_SHOW_DESKTOP 1
#define ET_TASKBAR_ACTION_TASK_MANAGER 2
#define ET_TASKBAR_ACTION_START_MENU 3
#define ET_TASKBAR_ACTION_MUTE 4
#define ET_TASKBAR_ACTION_MEDIA_PLAY_PAUSE 5
#define ET_TASKBAR_ACTION_TOGGLE_AUTOHIDE 6
#define ET_TASKBAR_ACTION_TASK_VIEW 7
#define ET_TASKBAR_ACTION_LOCK 8
#define ET_TASKBAR_ACTION_COUNT 9

static BOOL ET_GetClassName(HWND hWnd, WCHAR* wszClass, size_t cch)
{
    if (!hWnd || !wszClass || cch == 0)
    {
        return FALSE;
    }
    wszClass[0] = 0;
    return GetClassNameW(hWnd, wszClass, (int)cch) > 0;
}

static BOOL ET_IsWindowOfClass(HWND hWnd, const WCHAR* wszExpected)
{
    WCHAR wszClass[ET_CLASSNAME_CCH];
    return ET_GetClassName(hWnd, wszClass, ARRAYSIZE(wszClass)) && !wcscmp(wszClass, wszExpected);
}

static BOOL ET_IsExplorerWindowForeground(void)
{
    return ET_IsWindowOfClass(GetForegroundWindow(), L"CabinetWClass");
}

#pragma region "Settings"
static void ET_ApplyNavigationPaneVisibility(HKEY hKey);
static void ET_ApplyStartupDelay(HKEY hKey);
static void ET_UpdateOnDemandHooks(void);
static void ET_RefreshDesktop(BOOL bRepaint);
static void ET_HideShortcutArrowsOnce(void);
static void ET_OnExplorerWindowCreated(HWND hWnd);
static void ET_OnDesktopListViewCreated(HWND hListView, HWND hDefView);

static DWORD ET_ReadDword(HKEY hKey, LPCWSTR lpValueName, DWORD dwDefault)
{
    DWORD dwValue = dwDefault;
    DWORD dwSize = sizeof(DWORD);
    DWORD dwType = REG_NONE;
    if (RegQueryValueExW(hKey, lpValueName, 0, &dwType, (LPBYTE)&dwValue, &dwSize) != ERROR_SUCCESS || dwType != REG_DWORD || dwSize != sizeof(DWORD))
    {
        return dwDefault;
    }
    return dwValue;
}

void EssentialTweaks_LoadSettings(HKEY hKey)
{
    if (!hKey)
    {
        return;
    }

    DWORD dwValue = ET_ReadDword(hKey, L"EssentialTaskbarVolumeScroll", 0);
    dwEssentialTaskbarVolumeScroll = (dwValue <= 2) ? dwValue : 0;

    dwValue = ET_ReadDword(hKey, L"EssentialVolumeStep", 2);
    if (dwValue < 2 || dwValue > 20)
    {
        dwValue = 2;
    }
    dwEssentialVolumeStep = dwValue - (dwValue % 2);

    dwValue = ET_ReadDword(hKey, L"EssentialTrayIconFix", 0);
    dwEssentialTrayIconFix = (dwValue <= 120) ? dwValue : 120;

    bEssentialDisableExtensionWarning = ET_ReadDword(hKey, L"EssentialDisableExtensionWarning", 0) ? TRUE : FALSE;
    bEssentialExplorerDoubleClickUp = ET_ReadDword(hKey, L"EssentialExplorerDoubleClickUp", 0) ? TRUE : FALSE;
    bEssentialReopenClosedTab = ET_ReadDword(hKey, L"EssentialReopenClosedTab", 0) ? TRUE : FALSE;

    dwValue = ET_ReadDword(hKey, L"EssentialTaskbarDoubleClickAction", 0);
    dwEssentialTaskbarDoubleClickAction = (dwValue < ET_TASKBAR_ACTION_COUNT) ? dwValue : 0;
    dwValue = ET_ReadDword(hKey, L"EssentialTaskbarMiddleClickAction", 0);
    dwEssentialTaskbarMiddleClickAction = (dwValue < ET_TASKBAR_ACTION_COUNT) ? dwValue : 0;

    bEssentialHideExplorerHome = ET_ReadDword(hKey, L"EssentialHideExplorerHome", 0) ? TRUE : FALSE;
    bEssentialHideExplorerGallery = ET_ReadDword(hKey, L"EssentialHideExplorerGallery", 0) ? TRUE : FALSE;
    bEssentialHideExplorerOneDrive = ET_ReadDword(hKey, L"EssentialHideExplorerOneDrive", 0) ? TRUE : FALSE;
    ET_ApplyNavigationPaneVisibility(hKey);

    bEssentialSingleWindowTabs = ET_ReadDword(hKey, L"EssentialSingleWindowTabs", 0) ? TRUE : FALSE;
    bEssentialDesktopToggleIcons = ET_ReadDword(hKey, L"EssentialDesktopToggleIcons", 0) ? TRUE : FALSE;
    BOOL bHideDesktopIconText = ET_ReadDword(hKey, L"EssentialHideDesktopIconText", 0) ? TRUE : FALSE;
    BOOL bRepaintDesktop = (bHideDesktopIconText != (bEssentialHideDesktopIconText != FALSE));
    bEssentialHideDesktopIconText = bHideDesktopIconText;
    bEssentialHideShortcutArrows = ET_ReadDword(hKey, L"EssentialHideShortcutArrows", 0) ? TRUE : FALSE;
    bEssentialFixExplorerWhiteFlash = ET_ReadDword(hKey, L"EssentialFixExplorerWhiteFlash", 0) ? TRUE : FALSE;
    bEssentialShowAllTrayIcons = ET_ReadDword(hKey, L"EssentialShowAllTrayIcons", 0) ? TRUE : FALSE;
    bEssentialNoStartupDelay = ET_ReadDword(hKey, L"EssentialNoStartupDelay", 0) ? TRUE : FALSE;
    bEssentialBlockF1Help = ET_ReadDword(hKey, L"EssentialBlockF1Help", 0) ? TRUE : FALSE;

    ET_ApplyStartupDelay(hKey);
    ET_UpdateOnDemandHooks();
    ET_RefreshDesktop(bRepaintDesktop);
    if (bEssentialHideShortcutArrows)
    {
        ET_HideShortcutArrowsOnce();
    }
}
#pragma endregion

#pragma region "Taskbar volume control"
// Port of the Windows 11 indicator mode of "Taskbar Volume Control": the last 2% step is done by posting a shell
// APPCOMMAND (which makes Windows show its own volume indicator); additional steps are applied through Core Audio.

static BOOL ET_AddMasterVolumeLevelScalar(float fMasterVolumeAdd)
{
    IMMDeviceEnumerator* pEnumerator = NULL;
    IMMDevice* pDevice = NULL;
    IAudioEndpointVolume* pEndpointVolume = NULL;
    float fMasterVolume = 0.0f;
    BOOL bSuccess = FALSE;

    if (FAILED(CoCreateInstance(&ET_CLSID_MMDeviceEnumerator, NULL, CLSCTX_INPROC_SERVER, &ET_IID_IMMDeviceEnumerator, (LPVOID*)&pEnumerator)) || !pEnumerator)
    {
        return FALSE;
    }
    if (SUCCEEDED(pEnumerator->lpVtbl->GetDefaultAudioEndpoint(pEnumerator, eRender, eConsole, &pDevice)) && pDevice)
    {
        if (SUCCEEDED(pDevice->lpVtbl->Activate(pDevice, &ET_IID_IAudioEndpointVolume, CLSCTX_INPROC_SERVER, NULL, (LPVOID*)&pEndpointVolume)) && pEndpointVolume)
        {
            if (SUCCEEDED(pEndpointVolume->lpVtbl->GetMasterVolumeLevelScalar(pEndpointVolume, &fMasterVolume)))
            {
                fMasterVolume += fMasterVolumeAdd;
                if (fMasterVolume < 0.0f)
                {
                    fMasterVolume = 0.0f;
                }
                else if (fMasterVolume > 1.0f)
                {
                    fMasterVolume = 1.0f;
                }
                if (SUCCEEDED(pEndpointVolume->lpVtbl->SetMasterVolumeLevelScalar(pEndpointVolume, fMasterVolume, NULL)))
                {
                    bSuccess = TRUE;
                    // Windows shows the volume rounded to the nearest percent and mutes at 0%; do the same.
                    pEndpointVolume->lpVtbl->SetMute(pEndpointVolume, (fMasterVolume < 0.005f) ? TRUE : FALSE, NULL);
                }
            }
            pEndpointVolume->lpVtbl->Release(pEndpointVolume);
        }
        pDevice->lpVtbl->Release(pDevice);
    }
    pEnumerator->lpVtbl->Release(pEnumerator);
    return bSuccess;
}

static BOOL ET_PostVolumeAppCommand(SHORT sAppCommand)
{
    static UINT uShellHookMsg = 0;
    if (!uShellHookMsg)
    {
        uShellHookMsg = RegisterWindowMessageW(L"SHELLHOOK");
    }
    if (!uShellHookMsg)
    {
        return FALSE;
    }
    HWND hTaskbar = FindWindowW(L"Shell_TrayWnd", NULL);
    HWND hReBar = hTaskbar ? FindWindowExW(hTaskbar, NULL, L"ReBarWindow32", NULL) : NULL;
    HWND hTaskSw = hReBar ? FindWindowExW(hReBar, NULL, L"MSTaskSwWClass", NULL) : NULL;
    if (!hTaskSw)
    {
        return FALSE;
    }
    return PostMessageW(hTaskSw, uShellHookMsg, HSHELL_APPCOMMAND, MAKELPARAM(0, sAppCommand));
}

static void ET_AdjustVolumeByWheelDelta(int nDelta)
{
    static DWORD dwLastScrollTime = 0;
    static int nLastScrollRemainder = 0;

    DWORD dwNow = GetTickCount();
    if (dwNow - dwLastScrollTime < 5000)
    {
        nDelta += nLastScrollRemainder;
    }

    int nClicks = nDelta / WHEEL_DELTA;
    if (nClicks)
    {
        SHORT sAppCommand = APPCOMMAND_VOLUME_UP;
        float fDirection = 1.0f;
        if (nClicks < 0)
        {
            nClicks = -nClicks;
            sAppCommand = APPCOMMAND_VOLUME_DOWN;
            fDirection = -1.0f;
        }
        if (nClicks > 50)
        {
            nClicks = 50;
        }

        // Each APPCOMMAND changes the volume by 2%; scale the amount by the configured step.
        int nSteps = nClicks * (int)(dwEssentialVolumeStep / 2);
        if (nSteps < 1)
        {
            nSteps = 1;
        }
        if (nSteps > 1)
        {
            ET_AddMasterVolumeLevelScalar(fDirection * 0.02f * (float)(nSteps - 1));
        }
        if (!ET_PostVolumeAppCommand(sAppCommand))
        {
            // No legacy taskbar window found: change the volume directly (no on-screen indicator).
            ET_AddMasterVolumeLevelScalar(fDirection * 0.02f);
        }
    }

    dwLastScrollTime = dwNow;
    nLastScrollRemainder = nDelta % WHEEL_DELTA;
}

BOOL EssentialTweaks_OnTaskbarMouseWheel(HWND hWnd, WPARAM wParam, LPARAM lParam)
{
    DWORD dwMode = dwEssentialTaskbarVolumeScroll;
    if (!dwMode || !hWnd)
    {
        return FALSE;
    }
    if (GetCapture())
    {
        return FALSE;
    }

    POINT pt;
    pt.x = GET_X_LPARAM(lParam);
    pt.y = GET_Y_LPARAM(lParam);

    RECT rc;
    if (!GetWindowRect(hWnd, &rc) || !PtInRect(&rc, pt))
    {
        return FALSE;
    }
    if (dwMode == 2)
    {
        // Notification area only: the tray on the primary taskbar, the clock on secondary taskbars.
        HWND hArea = FindWindowExW(hWnd, NULL, L"TrayNotifyWnd", NULL);
        if (!hArea)
        {
            hArea = FindWindowExW(hWnd, NULL, L"ClockButton", NULL);
        }
        if (!hArea || !GetWindowRect(hArea, &rc) || IsRectEmpty(&rc) || !PtInRect(&rc, pt))
        {
            return FALSE;
        }
    }

    ET_AdjustVolumeByWheelDelta(GET_WHEEL_DELTA_WPARAM(wParam));
    return TRUE;
}
#pragma endregion

#pragma region "Taskbar empty space click actions"
// Simplified port of "Click on empty taskbar space": one action for a double click and one for a middle click.
// Windows 10 (and ep_taskbar) taskbar: clicks on empty space reach the Shell_TrayWnd / Shell_SecondaryTrayWnd window
// procedure itself (the task list is transparent to hit testing there), so the subclass procedure is enough.
// Windows 11 taskbar: the XAML island receives its input as WM_POINTER* messages in the
// "Windows.UI.Input.InputSite.WindowClass" child window, whose window procedure is hooked inline (see
// EssentialTweaks_OnTaskbarTimer); older builds deliver regular mouse messages, which the WH_MOUSE hook of the
// taskbar thread sees. In both cases UI Automation confirms that the point is on empty space.

static BOOL ET_IsTaskbarWindow(HWND hWnd)
{
    WCHAR wszClass[ET_CLASSNAME_CCH];
    if (!ET_GetClassName(hWnd, wszClass, ARRAYSIZE(wszClass)))
    {
        return FALSE;
    }
    return !wcscmp(wszClass, L"Shell_TrayWnd") || !wcscmp(wszClass, L"Shell_SecondaryTrayWnd");
}

static void ET_SendKeyCombination(const WORD* pwKeys, UINT cKeys)
{
    INPUT inputs[8];
    if (!pwKeys || cKeys == 0 || cKeys * 2 > ARRAYSIZE(inputs))
    {
        return;
    }
    ZeroMemory(inputs, sizeof(inputs));
    for (UINT i = 0; i < cKeys; ++i)
    {
        inputs[i].type = INPUT_KEYBOARD;
        inputs[i].ki.wVk = pwKeys[i];
        // Released in reverse order.
        inputs[cKeys * 2 - 1 - i].type = INPUT_KEYBOARD;
        inputs[cKeys * 2 - 1 - i].ki.wVk = pwKeys[i];
        inputs[cKeys * 2 - 1 - i].ki.dwFlags = KEYEVENTF_KEYUP;
    }
    SendInput(cKeys * 2, inputs, sizeof(INPUT));
}

static BOOL ET_ToggleMasterMute(void)
{
    IMMDeviceEnumerator* pEnumerator = NULL;
    IMMDevice* pDevice = NULL;
    IAudioEndpointVolume* pEndpointVolume = NULL;
    BOOL bSuccess = FALSE;

    if (FAILED(CoCreateInstance(&ET_CLSID_MMDeviceEnumerator, NULL, CLSCTX_INPROC_SERVER, &ET_IID_IMMDeviceEnumerator, (LPVOID*)&pEnumerator)) || !pEnumerator)
    {
        return FALSE;
    }
    if (SUCCEEDED(pEnumerator->lpVtbl->GetDefaultAudioEndpoint(pEnumerator, eRender, eConsole, &pDevice)) && pDevice)
    {
        if (SUCCEEDED(pDevice->lpVtbl->Activate(pDevice, &ET_IID_IAudioEndpointVolume, CLSCTX_INPROC_SERVER, NULL, (LPVOID*)&pEndpointVolume)) && pEndpointVolume)
        {
            BOOL bMuted = FALSE;
            if (SUCCEEDED(pEndpointVolume->lpVtbl->GetMute(pEndpointVolume, &bMuted)))
            {
                bSuccess = SUCCEEDED(pEndpointVolume->lpVtbl->SetMute(pEndpointVolume, !bMuted, NULL));
            }
            pEndpointVolume->lpVtbl->Release(pEndpointVolume);
        }
        pDevice->lpVtbl->Release(pDevice);
    }
    pEnumerator->lpVtbl->Release(pEnumerator);
    return bSuccess;
}

static void ET_OpenTaskManager(void)
{
    WCHAR wszPath[MAX_PATH];
    UINT cch = GetSystemDirectoryW(wszPath, ARRAYSIZE(wszPath));
    if (cch == 0 || cch >= ARRAYSIZE(wszPath) || wcscat_s(wszPath, ARRAYSIZE(wszPath), L"\\Taskmgr.exe") != 0)
    {
        return;
    }
    SHELLEXECUTEINFOW sei;
    ZeroMemory(&sei, sizeof(sei));
    sei.cbSize = sizeof(sei);
    sei.fMask = SEE_MASK_ASYNCOK | SEE_MASK_FLAG_NO_UI;
    sei.lpVerb = L"open";
    sei.lpFile = wszPath;
    sei.nShow = SW_SHOWNORMAL;
    ShellExecuteExW(&sei);
}

static void ET_ToggleTaskbarAutoHide(void)
{
    APPBARDATA abd;
    ZeroMemory(&abd, sizeof(abd));
    abd.cbSize = sizeof(abd);
    BOOL bAutoHide = (SHAppBarMessage(ABM_GETSTATE, &abd) & ABS_AUTOHIDE) != 0;
    abd.lParam = bAutoHide ? 0 : ABS_AUTOHIDE;
    SHAppBarMessage(ABM_SETSTATE, &abd);
}

void EssentialTweaks_PerformTaskbarAction(DWORD dwAction)
{
    HWND hTaskbar = FindWindowW(L"Shell_TrayWnd", NULL);
    switch (dwAction)
    {
    case ET_TASKBAR_ACTION_SHOW_DESKTOP:
        if (hTaskbar)
        {
            // 407: the taskbar's "toggle desktop" command
            PostMessageW(hTaskbar, WM_COMMAND, MAKEWPARAM(407, 0), 0);
        }
        break;
    case ET_TASKBAR_ACTION_TASK_MANAGER:
        ET_OpenTaskManager();
        break;
    case ET_TASKBAR_ACTION_START_MENU:
        if (hTaskbar)
        {
            PostMessageW(hTaskbar, WM_SYSCOMMAND, SC_TASKLIST, 0);
        }
        break;
    case ET_TASKBAR_ACTION_MUTE:
        // The APPCOMMAND makes Windows show its volume indicator; fall back to Core Audio without the legacy taskbar.
        if (!ET_PostVolumeAppCommand(APPCOMMAND_VOLUME_MUTE))
        {
            ET_ToggleMasterMute();
        }
        break;
    case ET_TASKBAR_ACTION_MEDIA_PLAY_PAUSE:
    {
        const WORD wKeys[] = { VK_MEDIA_PLAY_PAUSE };
        ET_SendKeyCombination(wKeys, ARRAYSIZE(wKeys));
        break;
    }
    case ET_TASKBAR_ACTION_TOGGLE_AUTOHIDE:
        ET_ToggleTaskbarAutoHide();
        break;
    case ET_TASKBAR_ACTION_TASK_VIEW:
    {
        const WORD wKeys[] = { VK_LWIN, VK_TAB };
        ET_SendKeyCombination(wKeys, ARRAYSIZE(wKeys));
        break;
    }
    case ET_TASKBAR_ACTION_LOCK:
        LockWorkStation();
        break;
    default:
        break;
    }
}

// All gesture sources funnel through here; a build that reports the same click through two sources must not run
// the action twice.
static void ET_TriggerTaskbarGesture(DWORD dwAction)
{
    static DWORD dwLastTriggerTime = 0;
    DWORD dwNow = GetTickCount();
    if (!dwAction || dwNow - dwLastTriggerTime < 300)
    {
        return;
    }
    dwLastTriggerTime = dwNow;
    EssentialTweaks_PerformTaskbarAction(dwAction);
}

static BOOL ET_IsPointOnEmptyTaskbarSpace(POINT pt)
{
    WCHAR wszClass[ET_CLASSNAME_CCH];
    if (!EssentialTweaks_GetUIAutomationClassNameAtPoint(pt, wszClass, ARRAYSIZE(wszClass)))
    {
        return FALSE;
    }
    return !wcscmp(wszClass, L"Taskbar.TaskbarFrameAutomationPeer") ||         // Windows 11 taskbar
        !wcscmp(wszClass, L"Windows.UI.Input.InputSite.WindowClass") ||        // Windows 11 21H2 taskbar
        !wcscmp(wszClass, L"Shell_TrayWnd") ||
        !wcscmp(wszClass, L"Shell_SecondaryTrayWnd");
}

BOOL EssentialTweaks_OnTaskbarMouseMessage(HWND hWnd, UINT uMsg)
{
    static HWND hMiddleDownWnd = NULL;
    switch (uMsg)
    {
    case WM_LBUTTONDBLCLK:
    case WM_NCLBUTTONDBLCLK:
        if (dwEssentialTaskbarDoubleClickAction)
        {
            ET_TriggerTaskbarGesture(dwEssentialTaskbarDoubleClickAction);
            return TRUE;
        }
        break;
    case WM_MBUTTONDOWN:
    case WM_NCMBUTTONDOWN:
        hMiddleDownWnd = dwEssentialTaskbarMiddleClickAction ? hWnd : NULL;
        break;
    case WM_MBUTTONUP:
    case WM_NCMBUTTONUP:
        if (dwEssentialTaskbarMiddleClickAction && hMiddleDownWnd == hWnd)
        {
            hMiddleDownWnd = NULL;
            ET_TriggerTaskbarGesture(dwEssentialTaskbarMiddleClickAction);
            return TRUE;
        }
        hMiddleDownWnd = NULL;
        break;
    default:
        break;
    }
    return FALSE;
}

// Shared by the WH_MOUSE hook (button releases) and the InputSite subclass (pointer presses): returns the action
// of the gesture that this click completes. Clicks closer together than ET_DUPLICATE_CLICK_MS are the same
// physical click reported twice.
#define ET_DUPLICATE_CLICK_MS 40
static DWORD ET_GetTaskbarGestureAction(BOOL bMiddle, POINT pt)
{
    static DWORD dwLastLeftTime = 0;
    static POINT ptLastLeft;
    static BOOL bHaveLastLeft = FALSE;
    static DWORD dwLastMiddleTime = 0;

    DWORD dwNow = GetTickCount();
    if (bMiddle)
    {
        if (!dwEssentialTaskbarMiddleClickAction || dwNow - dwLastMiddleTime < ET_DUPLICATE_CLICK_MS)
        {
            return ET_TASKBAR_ACTION_NONE;
        }
        dwLastMiddleTime = dwNow;
        return dwEssentialTaskbarMiddleClickAction;
    }

    if (!dwEssentialTaskbarDoubleClickAction)
    {
        return ET_TASKBAR_ACTION_NONE;
    }
    if (bHaveLastLeft)
    {
        DWORD dwElapsed = dwNow - dwLastLeftTime;
        if (dwElapsed < ET_DUPLICATE_CLICK_MS)
        {
            return ET_TASKBAR_ACTION_NONE;
        }
        if (dwElapsed <= GetDoubleClickTime() &&
            abs(pt.x - ptLastLeft.x) <= GetSystemMetrics(SM_CXDOUBLECLK) &&
            abs(pt.y - ptLastLeft.y) <= GetSystemMetrics(SM_CYDOUBLECLK))
        {
            bHaveLastLeft = FALSE;
            return dwEssentialTaskbarDoubleClickAction;
        }
    }
    bHaveLastLeft = TRUE;
    dwLastLeftTime = dwNow;
    ptLastLeft = pt;
    return ET_TASKBAR_ACTION_NONE;
}

void EssentialTweaks_OnTaskbarThreadMouseHook(WPARAM wMouseMsg, const MOUSEHOOKSTRUCT* pMouse)
{
    if (!pMouse || (!dwEssentialTaskbarDoubleClickAction && !dwEssentialTaskbarMiddleClickAction))
    {
        return;
    }
    if (wMouseMsg != WM_LBUTTONUP && wMouseMsg != WM_MBUTTONUP)
    {
        return;
    }
    if (!ET_IsTaskbarWindow(GetAncestor(pMouse->hwnd, GA_ROOT)))
    {
        return;
    }
    DWORD dwAction = ET_GetTaskbarGestureAction(wMouseMsg == WM_MBUTTONUP, pMouse->pt);
    if (dwAction && ET_IsPointOnEmptyTaskbarSpace(pMouse->pt))
    {
        ET_TriggerTaskbarGesture(dwAction);
    }
}

// Windows 11 taskbar input window. The window cannot be subclassed (InputHost.dll verifies its window procedure and
// fails fast when it was replaced), so the window procedure itself is hooked inline instead. The procedure is
// shared by every InputSite window of the process, hence the cheap message checks first and the taskbar check after.
typedef LRESULT(CALLBACK* ET_WndProc_t)(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
static ET_WndProc_t ET_InputSiteWndProcFunc = NULL;

static LRESULT CALLBACK ET_InputSiteWndProcHook(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    if (uMsg == WM_POINTERWHEEL)
    {
        if (dwEssentialTaskbarVolumeScroll)
        {
            // Same parameter layout as WM_MOUSEWHEEL: wheel delta in the high word, screen coordinates in lParam.
            HWND hTaskbar = GetAncestor(hWnd, GA_ROOT);
            if (ET_IsTaskbarWindow(hTaskbar) && EssentialTweaks_OnTaskbarMouseWheel(hTaskbar, wParam, lParam))
            {
                return 0;
            }
        }
    }
    else if (uMsg == WM_POINTERDOWN && (dwEssentialTaskbarDoubleClickAction || dwEssentialTaskbarMiddleClickAction))
    {
        BOOL bLeft = IS_POINTER_FIRSTBUTTON_WPARAM(wParam) != 0;
        BOOL bMiddle = IS_POINTER_THIRDBUTTON_WPARAM(wParam) != 0;
        if ((bLeft || bMiddle) && ET_IsTaskbarWindow(GetAncestor(hWnd, GA_ROOT)))
        {
            POINT pt;
            pt.x = GET_X_LPARAM(lParam);
            pt.y = GET_Y_LPARAM(lParam);
            DWORD dwAction = ET_GetTaskbarGestureAction(bMiddle, pt);
            if (dwAction && ET_IsPointOnEmptyTaskbarSpace(pt))
            {
                ET_TriggerTaskbarGesture(dwAction);
            }
        }
    }
    return ET_InputSiteWndProcFunc(hWnd, uMsg, wParam, lParam);
}

#define ET_TASKBAR_TIMER_ID 0xE5C0
#define ET_TASKBAR_TIMER_INTERVAL 3000

void EssentialTweaks_OnTaskbarWindowCreated(HWND hTaskbar)
{
#if WITH_MAIN_PATCHER
    // The XAML island is created after the taskbar window; look for it periodically until its procedure is hooked.
    if (hTaskbar && IsWindows11() && !ET_InputSiteWndProcFunc)
    {
        SetTimer(hTaskbar, ET_TASKBAR_TIMER_ID, ET_TASKBAR_TIMER_INTERVAL, NULL);
    }
#else
    UNREFERENCED_PARAMETER(hTaskbar);
#endif
}

BOOL EssentialTweaks_OnTaskbarTimer(HWND hTaskbar, WPARAM idTimer)
{
    if (idTimer != ET_TASKBAR_TIMER_ID)
    {
        return FALSE;
    }
#if WITH_MAIN_PATCHER
    if (ET_InputSiteWndProcFunc)
    {
        KillTimer(hTaskbar, ET_TASKBAR_TIMER_ID);
        return TRUE;
    }
    HWND hBridge = FindWindowExW(hTaskbar, NULL, L"Windows.UI.Composition.DesktopWindowContentBridge", NULL);
    HWND hInputSite = hBridge ? FindWindowExW(hBridge, NULL, L"Windows.UI.Input.InputSite.WindowClass", NULL) : NULL;
    if (!hInputSite || !IsWindowUnicode(hInputSite))
    {
        return TRUE;
    }
    // Same process and a Unicode window, so this is the address of the procedure; make sure it is code of a module.
    void* pWndProc = (void*)GetWindowLongPtrW(hInputSite, GWLP_WNDPROC);
    HMODULE hModule = NULL;
    if (pWndProc && GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT, (LPCWSTR)pWndProc, &hModule) && hModule)
    {
        ET_InputSiteWndProcFunc = (ET_WndProc_t)pWndProc;
        if (funchook_prepare(funchook, (void**)&ET_InputSiteWndProcFunc, ET_InputSiteWndProcHook) != 0)
        {
            ET_InputSiteWndProcFunc = NULL;
        }
    }
    // One attempt only: either the hook is in place, or it cannot be installed on this build.
    KillTimer(hTaskbar, ET_TASKBAR_TIMER_ID);
#else
    KillTimer(hTaskbar, ET_TASKBAR_TIMER_ID);
#endif
    return TRUE;
}
#pragma endregion

#pragma region "Hide Home / Gallery / OneDrive in the navigation pane"
// Registry based (no hooks): the per-user "System.IsPinnedToNameSpaceTree" value of the namespace extension decides
// whether it is shown in the navigation pane.
#define ET_NAVPANE_VALUE L"System.IsPinnedToNameSpaceTree"
#define ET_NAVPANE_APPLIED_VALUE L"EssentialNavPaneApplied"

typedef struct _ET_NavPaneItem
{
    DWORD dwBit;
    const WCHAR* wszClsid;
    // OneDrive pins itself through the same per-user value, so it is restored by writing 1 instead of deleting it.
    BOOL bPerUserRegistration;
} ET_NavPaneItem;

static const ET_NavPaneItem g_essentialNavPaneItems[] = {
    { 0x1, L"{f874310e-b6b7-47dc-bc84-b9e6b38f5903}", FALSE }, // Home
    { 0x2, L"{e88865ea-0e1c-4e20-9aa6-edcd0212c87c}", FALSE }, // Gallery
    { 0x4, L"{018D5C66-4533-4307-9B53-224DE2ED1FE6}", TRUE },  // OneDrive
};

static BOOL ET_FormatNavPaneKey(const ET_NavPaneItem* pItem, UINT uView, WCHAR* wszKey, size_t cch)
{
    static const WCHAR* const wszFormats[] = {
        L"Software\\Classes\\CLSID\\%s",
        L"Software\\Classes\\Wow6432Node\\CLSID\\%s",
    };
    return uView < ARRAYSIZE(wszFormats) && _snwprintf_s(wszKey, cch, _TRUNCATE, wszFormats[uView], pItem->wszClsid) > 0;
}

// Returns TRUE when the entry is already hidden for this user (by the user or by another tool).
static BOOL ET_IsNavPaneItemHidden(const ET_NavPaneItem* pItem)
{
    WCHAR wszKey[128];
    DWORD dwPinned = 1, dwSize = sizeof(DWORD);
    if (!ET_FormatNavPaneKey(pItem, 0, wszKey, ARRAYSIZE(wszKey)))
    {
        return FALSE;
    }
    return RegGetValueW(HKEY_CURRENT_USER, wszKey, ET_NAVPANE_VALUE, RRF_RT_REG_DWORD, NULL, &dwPinned, &dwSize) == ERROR_SUCCESS && dwPinned == 0;
}

static void ET_SetNavPanePinned(const ET_NavPaneItem* pItem, BOOL bHide)
{
    // Only OneDrive is registered for 32-bit applications as well.
    UINT cViews = pItem->bPerUserRegistration ? 2 : 1;
    for (UINT i = 0; i < cViews; ++i)
    {
        WCHAR wszKey[128];
        if (!ET_FormatNavPaneKey(pItem, i, wszKey, ARRAYSIZE(wszKey)))
        {
            continue;
        }
        if (bHide || pItem->bPerUserRegistration)
        {
            DWORD dwPinned = bHide ? 0 : 1;
            RegSetKeyValueW(HKEY_CURRENT_USER, wszKey, ET_NAVPANE_VALUE, REG_DWORD, &dwPinned, sizeof(DWORD));
        }
        else
        {
            RegDeleteKeyValueW(HKEY_CURRENT_USER, wszKey, ET_NAVPANE_VALUE);
        }
    }
}

// The stored state has two bit fields: the low byte is the last processed setting mask, the second byte tells
// which entries were actually hidden by this module. Entries that were already hidden by other means are left
// alone, both when hiding and when restoring.
static void ET_ApplyNavigationPaneVisibility(HKEY hKey)
{
    DWORD dwDesired =
        (bEssentialHideExplorerHome ? 0x1 : 0) |
        (bEssentialHideExplorerGallery ? 0x2 : 0) |
        (bEssentialHideExplorerOneDrive ? 0x4 : 0);
    DWORD dwState = ET_ReadDword(hKey, ET_NAVPANE_APPLIED_VALUE, 0);
    DWORD dwProcessed = dwState & 0x7;
    DWORD dwOwned = (dwState >> 8) & 0x7;
    if (dwDesired == dwProcessed)
    {
        return;
    }
    BOOL bChanged = FALSE;
    for (UINT i = 0; i < ARRAYSIZE(g_essentialNavPaneItems); ++i)
    {
        const ET_NavPaneItem* pItem = &g_essentialNavPaneItems[i];
        if (!((dwDesired ^ dwProcessed) & pItem->dwBit))
        {
            continue;
        }
        if (dwDesired & pItem->dwBit)
        {
            if (!ET_IsNavPaneItemHidden(pItem))
            {
                ET_SetNavPanePinned(pItem, TRUE);
                dwOwned |= pItem->dwBit;
                bChanged = TRUE;
            }
        }
        else if (dwOwned & pItem->dwBit)
        {
            ET_SetNavPanePinned(pItem, FALSE);
            dwOwned &= ~pItem->dwBit;
            bChanged = TRUE;
        }
    }
    dwState = dwDesired | (dwOwned << 8);
    RegSetValueExW(hKey, ET_NAVPANE_APPLIED_VALUE, 0, REG_DWORD, (const BYTE*)&dwState, sizeof(DWORD));
    if (bChanged)
    {
        SHChangeNotify(SHCNE_ASSOCCHANGED, SHCNF_IDLIST, NULL, NULL);
    }
}
#pragma endregion

#pragma region "Disappearing tray icons fix"
static DWORD WINAPI ET_TrayIconFixThread(LPVOID lpParam)
{
    DWORD dwDelaySeconds = (DWORD)(DWORD_PTR)lpParam;
    UINT uTaskbarCreatedMsg = RegisterWindowMessageW(L"TaskbarCreated");
    if (!uTaskbarCreatedMsg)
    {
        return 0;
    }

    // Wait for the taskbar to be created, and make sure it belongs to this process.
    BOOL bFound = FALSE;
    for (int i = 0; i < 240 && !bFound; ++i)
    {
        HWND hTaskbar = FindWindowW(L"Shell_TrayWnd", NULL);
        if (hTaskbar)
        {
            DWORD dwProcessId = 0;
            GetWindowThreadProcessId(hTaskbar, &dwProcessId);
            if (dwProcessId != GetCurrentProcessId())
            {
                return 0;
            }
            bFound = TRUE;
        }
        else
        {
            Sleep(500);
        }
    }
    if (!bFound)
    {
        return 0;
    }

    Sleep(dwDelaySeconds * 1000);
    PostMessageW(HWND_BROADCAST, uTaskbarCreatedMsg, 0, 0);
    return 0;
}
#pragma endregion

#pragma region "File extension change warning"
// ShellMessageBoxW is variadic. Variadic arguments cannot be forwarded in C, so the hooks declare a generous number
// of extra integer-sized parameters and pass them all along; the callee only reads the ones that belong to it, and
// the extra slots simply mirror the caller's stack.
typedef int(__cdecl* ET_ShellMessageBoxW_t)(
    HINSTANCE hAppInst, HWND hWnd, LPCWSTR lpcText, LPCWSTR lpcTitle, UINT fuStyle,
    UINT_PTR a6, UINT_PTR a7, UINT_PTR a8, UINT_PTR a9, UINT_PTR a10, UINT_PTR a11, UINT_PTR a12, UINT_PTR a13, UINT_PTR a14, UINT_PTR a15, UINT_PTR a16
);
typedef int(__cdecl* ET_ShellMessageBoxInternal_t)(
    HINSTANCE hAppInst, HWND hWnd, DWORD dwFlags, LPCWSTR lpcText, LPCWSTR lpcTitle, UINT fuStyle,
    UINT_PTR a7, UINT_PTR a8, UINT_PTR a9, UINT_PTR a10, UINT_PTR a11, UINT_PTR a12, UINT_PTR a13, UINT_PTR a14, UINT_PTR a15, UINT_PTR a16
);

static ET_ShellMessageBoxW_t ET_shlwapi_ShellMessageBoxWFunc = NULL;
static ET_ShellMessageBoxW_t ET_shell32_ShellMessageBoxWFunc = NULL;
static ET_ShellMessageBoxInternal_t ET_shlwapi_ShellMessageBoxInternalFunc = NULL;

static BOOL ET_IsRenameExtensionWarning(HINSTANCE hAppInst, LPCWSTR lpcText, LPCWSTR lpcTitle, UINT fuStyle)
{
    if (!bEssentialDisableExtensionWarning)
    {
        return FALSE;
    }
    // shell32.dll string 4112: "If you change a file name extension, the file might become unusable. Are you sure you want to change it?"
    // shell32.dll string 4148: "Rename"
    return hAppInst && IS_INTRESOURCE(lpcText) && IS_INTRESOURCE(lpcTitle) &&
        lpcText == MAKEINTRESOURCEW(4112) && lpcTitle == MAKEINTRESOURCEW(4148) &&
        fuStyle == (MB_ICONEXCLAMATION | MB_YESNO) &&
        hAppInst == (HINSTANCE)GetModuleHandleW(L"shell32.dll");
}

static int __cdecl ET_shlwapi_ShellMessageBoxWHook(
    HINSTANCE hAppInst, HWND hWnd, LPCWSTR lpcText, LPCWSTR lpcTitle, UINT fuStyle,
    UINT_PTR a6, UINT_PTR a7, UINT_PTR a8, UINT_PTR a9, UINT_PTR a10, UINT_PTR a11, UINT_PTR a12, UINT_PTR a13, UINT_PTR a14, UINT_PTR a15, UINT_PTR a16
)
{
    if (ET_IsRenameExtensionWarning(hAppInst, lpcText, lpcTitle, fuStyle))
    {
        return IDYES;
    }
    return ET_shlwapi_ShellMessageBoxWFunc(hAppInst, hWnd, lpcText, lpcTitle, fuStyle, a6, a7, a8, a9, a10, a11, a12, a13, a14, a15, a16);
}

static int __cdecl ET_shell32_ShellMessageBoxWHook(
    HINSTANCE hAppInst, HWND hWnd, LPCWSTR lpcText, LPCWSTR lpcTitle, UINT fuStyle,
    UINT_PTR a6, UINT_PTR a7, UINT_PTR a8, UINT_PTR a9, UINT_PTR a10, UINT_PTR a11, UINT_PTR a12, UINT_PTR a13, UINT_PTR a14, UINT_PTR a15, UINT_PTR a16
)
{
    if (ET_IsRenameExtensionWarning(hAppInst, lpcText, lpcTitle, fuStyle))
    {
        return IDYES;
    }
    return ET_shell32_ShellMessageBoxWFunc(hAppInst, hWnd, lpcText, lpcTitle, fuStyle, a6, a7, a8, a9, a10, a11, a12, a13, a14, a15, a16);
}

static int __cdecl ET_shlwapi_ShellMessageBoxInternalHook(
    HINSTANCE hAppInst, HWND hWnd, DWORD dwFlags, LPCWSTR lpcText, LPCWSTR lpcTitle, UINT fuStyle,
    UINT_PTR a7, UINT_PTR a8, UINT_PTR a9, UINT_PTR a10, UINT_PTR a11, UINT_PTR a12, UINT_PTR a13, UINT_PTR a14, UINT_PTR a15, UINT_PTR a16
)
{
    if (ET_IsRenameExtensionWarning(hAppInst, lpcText, lpcTitle, fuStyle))
    {
        return IDYES;
    }
    return ET_shlwapi_ShellMessageBoxInternalFunc(hAppInst, hWnd, dwFlags, lpcText, lpcTitle, fuStyle, a7, a8, a9, a10, a11, a12, a13, a14, a15, a16);
}

#if WITH_MAIN_PATCHER
static void ET_PrepareShellMessageBoxHooks(void)
{
    HMODULE hShlwapi = LoadLibraryExW(L"shlwapi.dll", NULL, LOAD_LIBRARY_SEARCH_SYSTEM32);
    HMODULE hShell32 = LoadLibraryExW(L"shell32.dll", NULL, LOAD_LIBRARY_SEARCH_SYSTEM32);

    ET_ShellMessageBoxW_t pfnShlwapiOriginal = NULL;
    if (hShlwapi)
    {
        pfnShlwapiOriginal = (ET_ShellMessageBoxW_t)GetProcAddress(hShlwapi, "ShellMessageBoxW");
        ET_shlwapi_ShellMessageBoxWFunc = pfnShlwapiOriginal;
        if (ET_shlwapi_ShellMessageBoxWFunc)
        {
            if (funchook_prepare(funchook, (void**)&ET_shlwapi_ShellMessageBoxWFunc, ET_shlwapi_ShellMessageBoxWHook) != 0)
            {
                ET_shlwapi_ShellMessageBoxWFunc = NULL;
            }
        }
        // Newer builds (April 2025 and later) route the message box through ShellMessageBoxInternal.
        ET_shlwapi_ShellMessageBoxInternalFunc = (ET_ShellMessageBoxInternal_t)GetProcAddress(hShlwapi, "ShellMessageBoxInternal");
        if (ET_shlwapi_ShellMessageBoxInternalFunc)
        {
            if (funchook_prepare(funchook, (void**)&ET_shlwapi_ShellMessageBoxInternalFunc, ET_shlwapi_ShellMessageBoxInternalHook) != 0)
            {
                ET_shlwapi_ShellMessageBoxInternalFunc = NULL;
            }
        }
    }
    if (hShell32)
    {
        ET_ShellMessageBoxW_t pfnShell32 = (ET_ShellMessageBoxW_t)GetProcAddress(hShell32, "ShellMessageBoxW");
        if (!pfnShell32)
        {
            pfnShell32 = (ET_ShellMessageBoxW_t)GetProcAddress(hShell32, (LPCSTR)182);
        }
        // Only hook shell32's export when it is a separate implementation (not a forwarder to shlwapi).
        if (pfnShell32 && pfnShell32 != pfnShlwapiOriginal)
        {
            ET_shell32_ShellMessageBoxWFunc = pfnShell32;
            if (funchook_prepare(funchook, (void**)&ET_shell32_ShellMessageBoxWFunc, ET_shell32_ShellMessageBoxWHook) != 0)
            {
                ET_shell32_ShellMessageBoxWFunc = NULL;
            }
        }
    }
}
#endif
#pragma endregion

#pragma region "Shell windows helpers"
// Invokes the callback for every registered File Explorer browser; return FALSE from the callback to stop.
typedef BOOL(*ET_ShellWindowCallback)(IShellBrowser* pShellBrowser, IDispatch* pDispatch, HWND hBrowserWnd, LPVOID lpContext);

static void ET_EnumShellWindows(ET_ShellWindowCallback pfnCallback, LPVOID lpContext)
{
    IShellWindows* pShellWindows = NULL;
    if (FAILED(CoCreateInstance(&ET_CLSID_ShellWindows, NULL, CLSCTX_ALL, &ET_IID_IShellWindows, (LPVOID*)&pShellWindows)) || !pShellWindows)
    {
        return;
    }

    long lCount = 0;
    if (FAILED(pShellWindows->lpVtbl->get_Count(pShellWindows, &lCount)))
    {
        lCount = 0;
    }
    for (long i = 0; i < lCount; ++i)
    {
        VARIANT vIndex;
        VariantInit(&vIndex);
        vIndex.vt = VT_I4;
        vIndex.lVal = i;

        IDispatch* pDispatch = NULL;
        if (FAILED(pShellWindows->lpVtbl->Item(pShellWindows, vIndex, &pDispatch)) || !pDispatch)
        {
            continue;
        }

        BOOL bContinue = TRUE;
        IServiceProvider* pServiceProvider = NULL;
        if (SUCCEEDED(pDispatch->lpVtbl->QueryInterface(pDispatch, &ET_IID_IServiceProvider, (LPVOID*)&pServiceProvider)) && pServiceProvider)
        {
            IShellBrowser* pShellBrowser = NULL;
            if (SUCCEEDED(pServiceProvider->lpVtbl->QueryService(pServiceProvider, &ET_SID_STopLevelBrowser, &ET_IID_IShellBrowser, (LPVOID*)&pShellBrowser)) && pShellBrowser)
            {
                HWND hBrowserWnd = NULL;
                if (FAILED(pShellBrowser->lpVtbl->GetWindow(pShellBrowser, &hBrowserWnd)))
                {
                    hBrowserWnd = NULL;
                }
                // Only File Explorer windows (skips the desktop, which is also registered).
                if (hBrowserWnd && ET_IsWindowOfClass(GetAncestor(hBrowserWnd, GA_ROOT), L"CabinetWClass"))
                {
                    bContinue = pfnCallback(pShellBrowser, pDispatch, hBrowserWnd, lpContext);
                }
                pShellBrowser->lpVtbl->Release(pShellBrowser);
            }
            pServiceProvider->lpVtbl->Release(pServiceProvider);
        }
        pDispatch->lpVtbl->Release(pDispatch);

        if (!bContinue)
        {
            break;
        }
    }
    pShellWindows->lpVtbl->Release(pShellWindows);
}

// Returns the absolute PIDL of the folder shown by a browser; free it with CoTaskMemFree.
static PIDLIST_ABSOLUTE ET_GetBrowserFolderPidl(IShellBrowser* pShellBrowser)
{
    PIDLIST_ABSOLUTE pidl = NULL;
    IShellView* pShellView = NULL;
    if (SUCCEEDED(pShellBrowser->lpVtbl->QueryActiveShellView(pShellBrowser, &pShellView)) && pShellView)
    {
        IFolderView* pFolderView = NULL;
        if (SUCCEEDED(pShellView->lpVtbl->QueryInterface(pShellView, &ET_IID_IFolderView, (LPVOID*)&pFolderView)) && pFolderView)
        {
            IPersistFolder2* pPersistFolder2 = NULL;
            if (SUCCEEDED(pFolderView->lpVtbl->GetFolder(pFolderView, &ET_IID_IPersistFolder2, (LPVOID*)&pPersistFolder2)) && pPersistFolder2)
            {
                if (FAILED(pPersistFolder2->lpVtbl->GetCurFolder(pPersistFolder2, &pidl)))
                {
                    pidl = NULL;
                }
                pPersistFolder2->lpVtbl->Release(pPersistFolder2);
            }
            pFolderView->lpVtbl->Release(pFolderView);
        }
        pShellView->lpVtbl->Release(pShellView);
    }
    return pidl;
}

// Returns the parsing name (file system path, or ::{CLSID} for virtual folders) of the folder shown by a browser.
static BOOL ET_GetBrowserFolderPath(IShellBrowser* pShellBrowser, WCHAR* wszPath, size_t cch)
{
    BOOL bSuccess = FALSE;
    if (!wszPath || cch == 0)
    {
        return FALSE;
    }
    wszPath[0] = 0;
    PIDLIST_ABSOLUTE pidl = ET_GetBrowserFolderPidl(pShellBrowser);
    if (pidl)
    {
        LPWSTR pszName = NULL;
        if (SUCCEEDED(SHGetNameFromIDList(pidl, SIGDN_DESKTOPABSOLUTEPARSING, &pszName)) && pszName)
        {
            if (wcsncpy_s(wszPath, cch, pszName, _TRUNCATE) == 0 && wszPath[0])
            {
                bSuccess = TRUE;
            }
            CoTaskMemFree(pszName);
        }
        CoTaskMemFree(pidl);
    }
    return bSuccess;
}
#pragma endregion

#pragma region "Double click empty space to go up"
typedef struct _ET_BrowseUpContext
{
    HWND hShellTab;
    BOOL bExactMatch;
    BOOL bNavigated;
} ET_BrowseUpContext;

static BOOL ET_BrowseUpCallback(IShellBrowser* pShellBrowser, IDispatch* pDispatch, HWND hBrowserWnd, LPVOID lpContext)
{
    ET_BrowseUpContext* pContext = (ET_BrowseUpContext*)lpContext;
    UNREFERENCED_PARAMETER(pDispatch);
    if (hBrowserWnd == pContext->hShellTab)
    {
        pContext->bExactMatch = TRUE;
        pContext->bNavigated = SUCCEEDED(pShellBrowser->lpVtbl->BrowseObject(pShellBrowser, NULL, SBSP_SAMEBROWSER | SBSP_PARENT));
        return FALSE;
    }
    return TRUE;
}

static BOOL ET_BrowseUpChildCallback(IShellBrowser* pShellBrowser, IDispatch* pDispatch, HWND hBrowserWnd, LPVOID lpContext)
{
    ET_BrowseUpContext* pContext = (ET_BrowseUpContext*)lpContext;
    UNREFERENCED_PARAMETER(pDispatch);
    if (IsChild(pContext->hShellTab, hBrowserWnd))
    {
        pContext->bNavigated = SUCCEEDED(pShellBrowser->lpVtbl->BrowseObject(pShellBrowser, NULL, SBSP_SAMEBROWSER | SBSP_PARENT));
        return FALSE;
    }
    return TRUE;
}

static HWND ET_FindShellTabAncestor(HWND hWnd)
{
    HWND hParent = hWnd;
    for (int i = 0; hParent && i < 16; ++i)
    {
        if (ET_IsWindowOfClass(hParent, L"ShellTabWindowClass"))
        {
            return hParent;
        }
        hParent = GetParent(hParent);
    }
    return NULL;
}

static void ET_BrowseToParentFolder(HWND hWnd)
{
    ET_BrowseUpContext context;
    ZeroMemory(&context, sizeof(context));
    context.hShellTab = ET_FindShellTabAncestor(hWnd);
    if (!context.hShellTab)
    {
        return;
    }
    ET_EnumShellWindows(ET_BrowseUpCallback, &context);
    if (!context.bExactMatch)
    {
        ET_EnumShellWindows(ET_BrowseUpChildCallback, &context);
    }
}

static DWORD g_dwEssentialLastClickTime = 0;
static HWND g_hEssentialLastClickWnd = NULL;
static WCHAR g_wszEssentialLastClickClass[ET_CLASSNAME_CCH];

// Modern (DirectUI) item view: the click lands on the DirectUIHWND child, the parent SHELLDLL_DefView receives
// WM_PARENTNOTIFY; UI Automation tells us whether the click was on empty space.
static void ET_OnDefViewClick(HWND hDefView)
{
    DWORD dwNow = GetTickCount();
    POINT pt;
    WCHAR wszClass[ET_CLASSNAME_CCH];

    if (!GetCursorPos(&pt) || !EssentialTweaks_GetUIAutomationClassNameAtPoint(pt, wszClass, ARRAYSIZE(wszClass)))
    {
        g_hEssentialLastClickWnd = NULL;
        return;
    }

    BOOL bEmptySpace = !wcscmp(wszClass, L"UIItemsView") || !wcscmp(wszClass, L"UIGroupItem");
    if (bEmptySpace &&
        g_hEssentialLastClickWnd == hDefView &&
        !wcscmp(wszClass, g_wszEssentialLastClickClass) &&
        dwNow - g_dwEssentialLastClickTime <= GetDoubleClickTime())
    {
        g_hEssentialLastClickWnd = NULL;
        g_dwEssentialLastClickTime = 0;
        ET_BrowseToParentFolder(hDefView);
        return;
    }

    g_dwEssentialLastClickTime = dwNow;
    g_hEssentialLastClickWnd = bEmptySpace ? hDefView : NULL;
    wcsncpy_s(g_wszEssentialLastClickClass, ARRAYSIZE(g_wszEssentialLastClickClass), wszClass, _TRUNCATE);
}

static LRESULT CALLBACK ET_DefViewSubclassProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam, UINT_PTR uIdSubclass, DWORD_PTR dwRefData)
{
    UNREFERENCED_PARAMETER(dwRefData);
    if (uMsg == WM_NCDESTROY)
    {
        RemoveWindowSubclass(hWnd, ET_DefViewSubclassProc, uIdSubclass);
        if (g_hEssentialLastClickWnd == hWnd)
        {
            g_hEssentialLastClickWnd = NULL;
        }
    }
    else if (uMsg == WM_PARENTNOTIFY && LOWORD(wParam) == WM_LBUTTONDOWN && bEssentialExplorerDoubleClickUp)
    {
        ET_OnDefViewClick(hWnd);
    }
    return DefSubclassProc(hWnd, uMsg, wParam, lParam);
}

// Classic (SysListView32) item view, used when the "SysListView32" option is enabled.
static LRESULT CALLBACK ET_ListViewSubclassProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam, UINT_PTR uIdSubclass, DWORD_PTR dwRefData)
{
    UNREFERENCED_PARAMETER(dwRefData);
    if (uMsg == WM_NCDESTROY)
    {
        RemoveWindowSubclass(hWnd, ET_ListViewSubclassProc, uIdSubclass);
    }
    else if (uMsg == WM_LBUTTONDBLCLK && bEssentialExplorerDoubleClickUp)
    {
        LVHITTESTINFO hitTestInfo;
        ZeroMemory(&hitTestInfo, sizeof(hitTestInfo));
        hitTestInfo.pt.x = GET_X_LPARAM(lParam);
        hitTestInfo.pt.y = GET_Y_LPARAM(lParam);
        hitTestInfo.flags = LVHT_NOWHERE;
        if (ListView_SubItemHitTest(hWnd, &hitTestInfo) == -1)
        {
            ET_BrowseToParentFolder(hWnd);
            return 0;
        }
    }
    return DefSubclassProc(hWnd, uMsg, wParam, lParam);
}

void EssentialTweaks_OnWindowCreated(HWND hWnd, HWND hWndParent)
{
    WCHAR wszClass[ET_CLASSNAME_CCH];
    if (!hWnd || !ET_GetClassName(hWnd, wszClass, ARRAYSIZE(wszClass)))
    {
        return;
    }
    if (!hWndParent)
    {
        if (!wcscmp(wszClass, L"CabinetWClass"))
        {
            ET_OnExplorerWindowCreated(hWnd);
        }
        return;
    }

    BOOL bIsDirectUI = !wcscmp(wszClass, L"DirectUIHWND");
    BOOL bIsListView = !wcscmp(wszClass, L"SysListView32");
    if (!bIsDirectUI && !bIsListView)
    {
        return;
    }
    if (!ET_IsWindowOfClass(hWndParent, L"SHELLDLL_DefView"))
    {
        return;
    }
    HWND hViewParent = GetParent(hWndParent);
    if (bIsListView && (ET_IsWindowOfClass(hViewParent, L"Progman") || ET_IsWindowOfClass(hViewParent, L"WorkerW")))
    {
        ET_OnDesktopListViewCreated(hWnd, hWndParent);
        return;
    }
    // The rest is for File Explorer views only.
    if (!ET_IsWindowOfClass(hViewParent, L"ShellTabWindowClass"))
    {
        return;
    }

    if (bIsDirectUI)
    {
        SetWindowSubclass(hWndParent, ET_DefViewSubclassProc, (UINT_PTR)ET_DefViewSubclassProc, 0);
    }
    else
    {
        SetWindowSubclass(hWnd, ET_ListViewSubclassProc, (UINT_PTR)ET_ListViewSubclassProc, 0);
    }
}
#pragma endregion

#pragma region "Reopen closed tab (Ctrl+Shift+T)"
#define ET_MAX_TABS 64
#define ET_TAB_HISTORY 50
#define ET_PATH_CCH 512
#define ET_NEW_TAB_COMMAND 0xA21B

typedef struct _ET_TabInfo
{
    HWND hWnd;
    WCHAR wszPath[ET_PATH_CCH];
} ET_TabInfo;

typedef struct _ET_TabsContext
{
    CRITICAL_SECTION cs;
    ET_TabInfo known[ET_MAX_TABS];
    int cKnown;
    ET_TabInfo current[ET_MAX_TABS];
    int cCurrent;
    WCHAR history[ET_TAB_HISTORY][ET_PATH_CCH];
    int cHistory;
    HANDLE hRestoreEvent;
    HHOOK hKeyboardHook;
} ET_TabsContext;

static ET_TabsContext* g_pEssentialTabs = NULL;

typedef struct _ET_CollectTabsContext
{
    ET_TabInfo* pTabs;
    int cMax;
    int cCount;
} ET_CollectTabsContext;

static BOOL ET_CollectTabsCallback(IShellBrowser* pShellBrowser, IDispatch* pDispatch, HWND hBrowserWnd, LPVOID lpContext)
{
    ET_CollectTabsContext* pContext = (ET_CollectTabsContext*)lpContext;
    UNREFERENCED_PARAMETER(pDispatch);
    if (pContext->cCount >= pContext->cMax)
    {
        return FALSE;
    }
    ET_TabInfo* pTab = &pContext->pTabs[pContext->cCount];
    if (ET_GetBrowserFolderPath(pShellBrowser, pTab->wszPath, ARRAYSIZE(pTab->wszPath)))
    {
        pTab->hWnd = hBrowserWnd;
        pContext->cCount++;
    }
    return TRUE;
}

static int ET_CollectTabs(ET_TabInfo* pTabs, int cMax)
{
    ET_CollectTabsContext context;
    context.pTabs = pTabs;
    context.cMax = cMax;
    context.cCount = 0;
    ET_EnumShellWindows(ET_CollectTabsCallback, &context);
    return context.cCount;
}

static BOOL ET_TabListContains(const ET_TabInfo* pTabs, int cTabs, HWND hWnd)
{
    for (int i = 0; i < cTabs; ++i)
    {
        if (pTabs[i].hWnd == hWnd)
        {
            return TRUE;
        }
    }
    return FALSE;
}

// Must be called with the critical section held.
static void ET_PushHistory(ET_TabsContext* pContext, const WCHAR* wszPath)
{
    if (pContext->cHistory >= ET_TAB_HISTORY)
    {
        memmove(pContext->history[0], pContext->history[1], sizeof(pContext->history[0]) * (ET_TAB_HISTORY - 1));
        pContext->cHistory = ET_TAB_HISTORY - 1;
    }
    wcsncpy_s(pContext->history[pContext->cHistory], ET_PATH_CCH, wszPath, _TRUNCATE);
    pContext->cHistory++;
}

// Compares the currently open tabs with the last known set and records the ones that were closed.
static void ET_PollTabs(ET_TabsContext* pContext)
{
    pContext->cCurrent = ET_CollectTabs(pContext->current, ET_MAX_TABS);

    EnterCriticalSection(&pContext->cs);
    for (int i = 0; i < pContext->cKnown; ++i)
    {
        if (!ET_TabListContains(pContext->current, pContext->cCurrent, pContext->known[i].hWnd))
        {
            ET_PushHistory(pContext, pContext->known[i].wszPath);
        }
    }
    if (pContext->cCurrent == 0 && pContext->cKnown != 0)
    {
        // All File Explorer windows were closed: start a new session.
        pContext->cHistory = 0;
    }
    memcpy(pContext->known, pContext->current, sizeof(ET_TabInfo) * pContext->cCurrent);
    pContext->cKnown = pContext->cCurrent;
    LeaveCriticalSection(&pContext->cs);
}

static HWND ET_GetTargetExplorerWindow(void)
{
    HWND hForeground = GetForegroundWindow();
    if (ET_IsWindowOfClass(hForeground, L"CabinetWClass"))
    {
        return hForeground;
    }
    return FindWindowW(L"CabinetWClass", NULL);
}

typedef struct _ET_FindNewTabContext
{
    const ET_TabInfo* pBefore;
    int cBefore;
    IDispatch* pNewTab;
} ET_FindNewTabContext;

static BOOL ET_FindNewTabCallback(IShellBrowser* pShellBrowser, IDispatch* pDispatch, HWND hBrowserWnd, LPVOID lpContext)
{
    ET_FindNewTabContext* pContext = (ET_FindNewTabContext*)lpContext;
    UNREFERENCED_PARAMETER(pShellBrowser);
    if (!ET_TabListContains(pContext->pBefore, pContext->cBefore, hBrowserWnd))
    {
        pDispatch->lpVtbl->AddRef(pDispatch);
        pContext->pNewTab = pDispatch;
        return FALSE;
    }
    return TRUE;
}

// Navigating by PIDL also works for virtual folders (::{CLSID}...), which Navigate2 does not accept as a string.
static BOOL ET_NavigateBrowserToPidl(IWebBrowser2* pWebBrowser, PCIDLIST_ABSOLUTE pidl)
{
    BOOL bSuccess = FALSE;
    UINT cbPidl = ILGetSize(pidl);
    SAFEARRAY* pArray = cbPidl ? SafeArrayCreateVector(VT_UI1, 0, cbPidl) : NULL;
    if (pArray)
    {
        void* pData = NULL;
        if (SUCCEEDED(SafeArrayAccessData(pArray, &pData)) && pData)
        {
            memcpy(pData, pidl, cbPidl);
            SafeArrayUnaccessData(pArray);

            VARIANT vTarget, vEmpty;
            VariantInit(&vTarget);
            VariantInit(&vEmpty);
            vTarget.vt = VT_ARRAY | VT_UI1;
            vTarget.parray = pArray;
            bSuccess = SUCCEEDED(pWebBrowser->lpVtbl->Navigate2(pWebBrowser, &vTarget, &vEmpty, &vEmpty, &vEmpty, &vEmpty));
        }
        SafeArrayDestroy(pArray);
    }
    return bSuccess;
}

// pScratch: ET_MAX_TABS entries of working memory owned by the caller.
static BOOL ET_OpenPidlAsTab(ET_TabInfo* pScratch, HWND hExplorer, PCIDLIST_ABSOLUTE pidl)
{
    // The new tab command has to go to the tab strip (ShellTabWindowClass), not the top-level window;
    // otherwise the tab is not registered with IShellWindows.
    HWND hTabStrip = FindWindowExW(hExplorer, NULL, L"ShellTabWindowClass", NULL);
    if (!hTabStrip || !pScratch || !pidl)
    {
        return FALSE;
    }

    // Snapshot the tabs that exist now, so that the new one can be told apart afterwards.
    ET_FindNewTabContext findContext;
    findContext.pBefore = pScratch;
    findContext.cBefore = ET_CollectTabs(pScratch, ET_MAX_TABS);
    findContext.pNewTab = NULL;

    DWORD_PTR dwResult = 0;
    if (!SendMessageTimeoutW(hTabStrip, WM_COMMAND, ET_NEW_TAB_COMMAND, 0, SMTO_ABORTIFHUNG | SMTO_NORMAL, 3000, &dwResult))
    {
        return FALSE;
    }

    DWORD dwDeadline = GetTickCount() + 3000;
    while (!findContext.pNewTab && (LONG)(dwDeadline - GetTickCount()) > 0)
    {
        Sleep(50);
        ET_EnumShellWindows(ET_FindNewTabCallback, &findContext);
    }
    if (!findContext.pNewTab)
    {
        return FALSE;
    }

    BOOL bSuccess = FALSE;
    IWebBrowser2* pWebBrowser = NULL;
    if (SUCCEEDED(findContext.pNewTab->lpVtbl->QueryInterface(findContext.pNewTab, &ET_IID_IWebBrowser2, (LPVOID*)&pWebBrowser)) && pWebBrowser)
    {
        bSuccess = ET_NavigateBrowserToPidl(pWebBrowser, pidl);
        pWebBrowser->lpVtbl->Release(pWebBrowser);
    }
    findContext.pNewTab->lpVtbl->Release(findContext.pNewTab);
    return bSuccess;
}

static BOOL ET_OpenPathAsTab(ET_TabsContext* pContext, HWND hExplorer, const WCHAR* wszPath)
{
    PIDLIST_ABSOLUTE pidl = NULL;
    if (FAILED(SHParseDisplayName(wszPath, NULL, &pidl, 0, NULL)) || !pidl)
    {
        return FALSE;
    }
    BOOL bSuccess = ET_OpenPidlAsTab(pContext->current, hExplorer, pidl);
    CoTaskMemFree(pidl);
    return bSuccess;
}

// Drops the most recent history entry when it is the given folder.
static void ET_ForgetClosedTab(const WCHAR* wszPath)
{
    ET_TabsContext* pContext = g_pEssentialTabs;
    if (!pContext)
    {
        return;
    }
    EnterCriticalSection(&pContext->cs);
    if (pContext->cHistory > 0 && !_wcsicmp(pContext->history[pContext->cHistory - 1], wszPath))
    {
        pContext->cHistory--;
    }
    LeaveCriticalSection(&pContext->cs);
}

static void ET_ReopenLastClosedTab(ET_TabsContext* pContext)
{
    WCHAR wszPath[ET_PATH_CCH];

    EnterCriticalSection(&pContext->cs);
    if (pContext->cHistory <= 0)
    {
        LeaveCriticalSection(&pContext->cs);
        return;
    }
    pContext->cHistory--;
    wcsncpy_s(wszPath, ARRAYSIZE(wszPath), pContext->history[pContext->cHistory], _TRUNCATE);
    LeaveCriticalSection(&pContext->cs);

    HWND hExplorer = ET_GetTargetExplorerWindow();
    if (!hExplorer || !ET_OpenPathAsTab(pContext, hExplorer, wszPath))
    {
        // Nothing to restore into (or the restore failed): keep the entry for a later attempt.
        EnterCriticalSection(&pContext->cs);
        ET_PushHistory(pContext, wszPath);
        LeaveCriticalSection(&pContext->cs);
        return;
    }

    // Make sure the restored tab is not recorded as "closed" by the next poll.
    ET_PollTabs(pContext);
}

static LRESULT CALLBACK ET_LowLevelKeyboardProc(int nCode, WPARAM wParam, LPARAM lParam)
{
    if (nCode == HC_ACTION && (wParam == WM_KEYDOWN || wParam == WM_SYSKEYDOWN) && bEssentialReopenClosedTab)
    {
        const KBDLLHOOKSTRUCT* pKeyboard = (const KBDLLHOOKSTRUCT*)lParam;
        if (pKeyboard && pKeyboard->vkCode == 'T')
        {
            BOOL bCtrl = (GetAsyncKeyState(VK_CONTROL) & 0x8000) != 0;
            BOOL bShift = (GetAsyncKeyState(VK_SHIFT) & 0x8000) != 0;
            BOOL bAlt = (GetAsyncKeyState(VK_MENU) & 0x8000) != 0;
            if (bCtrl && bShift && !bAlt && ET_IsExplorerWindowForeground())
            {
                ET_TabsContext* pContext = g_pEssentialTabs;
                if (pContext && pContext->hRestoreEvent)
                {
                    SetEvent(pContext->hRestoreEvent);
                }
                return 1;
            }
        }
    }
    return CallNextHookEx(NULL, nCode, wParam, lParam);
}

static DWORD WINAPI ET_KeyboardHookThread(LPVOID lpParam)
{
    ET_TabsContext* pContext = (ET_TabsContext*)lpParam;
    pContext->hKeyboardHook = SetWindowsHookExW(WH_KEYBOARD_LL, ET_LowLevelKeyboardProc, GetModuleHandleW(NULL), 0);
    if (!pContext->hKeyboardHook)
    {
        return 1;
    }

    MSG msg;
    while (GetMessageW(&msg, NULL, 0, 0) > 0)
    {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    UnhookWindowsHookEx(pContext->hKeyboardHook);
    pContext->hKeyboardHook = NULL;
    return 0;
}

static DWORD WINAPI ET_TabsPollThread(LPVOID lpParam)
{
    ET_TabsContext* pContext = (ET_TabsContext*)lpParam;
    if (FAILED(CoInitializeEx(NULL, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE)))
    {
        return 1;
    }

    // Seed the initial state; the shell windows may not be ready yet right after startup.
    for (int i = 0; i < 30; ++i)
    {
        ET_PollTabs(pContext);
        if (pContext->cKnown > 0)
        {
            break;
        }
        Sleep(1000);
    }
    EnterCriticalSection(&pContext->cs);
    pContext->cHistory = 0;
    LeaveCriticalSection(&pContext->cs);

    for (;;)
    {
        BOOL bExplorerForeground = ET_IsExplorerWindowForeground();
        DWORD dwWait = WaitForSingleObject(pContext->hRestoreEvent, bExplorerForeground ? 750 : 400);
        if (dwWait == WAIT_OBJECT_0)
        {
            ET_PollTabs(pContext);
            ET_ReopenLastClosedTab(pContext);
        }
        else if (dwWait == WAIT_TIMEOUT)
        {
            // Only poll while File Explorer is in the foreground, to keep the cost negligible otherwise.
            if (bExplorerForeground)
            {
                ET_PollTabs(pContext);
            }
        }
        else
        {
            break;
        }
    }

    CoUninitialize();
    return 0;
}

static void ET_StartReopenClosedTab(void)
{
    if (g_pEssentialTabs)
    {
        return;
    }
    ET_TabsContext* pContext = (ET_TabsContext*)calloc(1, sizeof(ET_TabsContext));
    if (!pContext)
    {
        return;
    }
    InitializeCriticalSection(&pContext->cs);
    pContext->hRestoreEvent = CreateEventW(NULL, FALSE, FALSE, NULL);
    if (!pContext->hRestoreEvent)
    {
        DeleteCriticalSection(&pContext->cs);
        free(pContext);
        return;
    }
    g_pEssentialTabs = pContext;

    HANDLE hThread = CreateThread(NULL, 0, ET_TabsPollThread, pContext, 0, NULL);
    if (hThread)
    {
        CloseHandle(hThread);
    }
    hThread = CreateThread(NULL, 0, ET_KeyboardHookThread, pContext, 0, NULL);
    if (hThread)
    {
        CloseHandle(hThread);
    }
}
#pragma endregion

#pragma region "Open new File Explorer windows as tabs"
// Port of "Explorer Single Window Tabs": a new File Explorer window is kept invisible while a worker thread reads
// the folder it shows, opens that folder as a tab of an existing window and then closes the new window. Holding
// Shift while the window opens bypasses the redirection. Control Panel and search windows are left alone.
#define ET_REDIRECT_PROP L"EPEssentialTabRedirect"
#define ET_CONTROL_PANEL_CLSID L"::{26EE0668-A00A-44D7-9371-BEB064C98683}"

typedef struct _ET_FindWindowFolderContext
{
    HWND hRoot;
    PIDLIST_ABSOLUTE pidl;
} ET_FindWindowFolderContext;

static BOOL ET_FindWindowFolderCallback(IShellBrowser* pShellBrowser, IDispatch* pDispatch, HWND hBrowserWnd, LPVOID lpContext)
{
    ET_FindWindowFolderContext* pContext = (ET_FindWindowFolderContext*)lpContext;
    UNREFERENCED_PARAMETER(pDispatch);
    if (GetAncestor(hBrowserWnd, GA_ROOT) == pContext->hRoot)
    {
        pContext->pidl = ET_GetBrowserFolderPidl(pShellBrowser);
        return FALSE;
    }
    return TRUE;
}

static BOOL ET_IsRedirectableFolder(const WCHAR* wszPath)
{
    return wszPath[0] &&
        !StrStrIW(wszPath, ET_CONTROL_PANEL_CLSID) &&
        _wcsnicmp(wszPath, L"search-ms:", 10) != 0 &&
        _wcsnicmp(wszPath, L"shell:", 6) != 0;
}

typedef struct _ET_FindPrimaryWindowContext
{
    HWND hExclude;
    HWND hPrimary;
} ET_FindPrimaryWindowContext;

static BOOL ET_FindPrimaryWindowCallback(IShellBrowser* pShellBrowser, IDispatch* pDispatch, HWND hBrowserWnd, LPVOID lpContext)
{
    ET_FindPrimaryWindowContext* pContext = (ET_FindPrimaryWindowContext*)lpContext;
    UNREFERENCED_PARAMETER(pDispatch);
    HWND hRoot = GetAncestor(hBrowserWnd, GA_ROOT);
    if (!hRoot || hRoot == pContext->hExclude || !IsWindowVisible(hRoot) || GetPropW(hRoot, ET_REDIRECT_PROP))
    {
        return TRUE;
    }
    WCHAR wszPath[ET_PATH_CCH];
    if (ET_GetBrowserFolderPath(pShellBrowser, wszPath, ARRAYSIZE(wszPath)) && ET_IsRedirectableFolder(wszPath))
    {
        pContext->hPrimary = hRoot;
        return FALSE;
    }
    return TRUE;
}

static void ET_AbortTabRedirect(HWND hWnd)
{
    if (!IsWindow(hWnd))
    {
        return;
    }
    RemovePropW(hWnd, ET_REDIRECT_PROP);
    LONG_PTR lExStyle = GetWindowLongPtrW(hWnd, GWL_EXSTYLE);
    if (lExStyle & WS_EX_LAYERED)
    {
        SetWindowLongPtrW(hWnd, GWL_EXSTYLE, lExStyle & ~WS_EX_LAYERED);
    }
    SetWindowPos(hWnd, NULL, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE | SWP_FRAMECHANGED);
    RedrawWindow(hWnd, NULL, NULL, RDW_INVALIDATE | RDW_ERASE | RDW_FRAME | RDW_ALLCHILDREN);
}

static DWORD WINAPI ET_TabRedirectThread(LPVOID lpParam)
{
    HWND hNewWnd = (HWND)lpParam;
    if (FAILED(CoInitializeEx(NULL, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE)))
    {
        ET_AbortTabRedirect(hNewWnd);
        return 1;
    }

    BOOL bRedirected = FALSE;
    ET_TabInfo* pScratch = (ET_TabInfo*)calloc(ET_MAX_TABS, sizeof(ET_TabInfo));

    // Wait for the new window to finish its first navigation.
    ET_FindWindowFolderContext folderContext;
    folderContext.hRoot = hNewWnd;
    folderContext.pidl = NULL;
    for (int i = 0; i < 60 && !folderContext.pidl && IsWindow(hNewWnd); ++i)
    {
        Sleep(50);
        ET_EnumShellWindows(ET_FindWindowFolderCallback, &folderContext);
    }

    if (pScratch && folderContext.pidl && IsWindow(hNewWnd))
    {
        WCHAR wszPath[ET_PATH_CCH];
        wszPath[0] = 0;
        LPWSTR pszName = NULL;
        if (SUCCEEDED(SHGetNameFromIDList(folderContext.pidl, SIGDN_DESKTOPABSOLUTEPARSING, &pszName)) && pszName)
        {
            wcsncpy_s(wszPath, ARRAYSIZE(wszPath), pszName, _TRUNCATE);
            CoTaskMemFree(pszName);
        }
        if (ET_IsRedirectableFolder(wszPath))
        {
            ET_FindPrimaryWindowContext primaryContext;
            primaryContext.hExclude = hNewWnd;
            primaryContext.hPrimary = NULL;
            ET_EnumShellWindows(ET_FindPrimaryWindowCallback, &primaryContext);
            // The tab is opened first; the new window is only closed once that worked.
            if (primaryContext.hPrimary && ET_OpenPidlAsTab(pScratch, primaryContext.hPrimary, folderContext.pidl))
            {
                bRedirected = TRUE;
                PostMessageW(hNewWnd, WM_CLOSE, 0, 0);
                if (IsIconic(primaryContext.hPrimary))
                {
                    ShowWindowAsync(primaryContext.hPrimary, SW_RESTORE);
                }
                SetForegroundWindow(primaryContext.hPrimary);

                // The closed window is not a "closed tab" the user would want to reopen.
                Sleep(1500);
                ET_ForgetClosedTab(wszPath);
            }
        }
    }

    if (!bRedirected)
    {
        ET_AbortTabRedirect(hNewWnd);
    }
    if (folderContext.pidl)
    {
        CoTaskMemFree(folderContext.pidl);
    }
    free(pScratch);
    CoUninitialize();
    return 0;
}

static void ET_OnExplorerWindowCreated(HWND hWnd)
{
    if (!bEssentialSingleWindowTabs || !IsWindows11Version22H2OrHigher())
    {
        return;
    }
    if (GetAsyncKeyState(VK_SHIFT) & 0x8000)
    {
        return;
    }
    // Is there another File Explorer window that could take the tab?
    BOOL bHaveOther = FALSE;
    HWND hOther = NULL;
    for (int i = 0; i < 64 && !bHaveOther; ++i)
    {
        hOther = FindWindowExW(NULL, hOther, L"CabinetWClass", NULL);
        if (!hOther)
        {
            break;
        }
        bHaveOther = hOther != hWnd && IsWindowVisible(hOther) && !GetPropW(hOther, ET_REDIRECT_PROP);
    }
    if (!bHaveOther)
    {
        return;
    }

    // Fully transparent instead of hidden: the window still registers with IShellWindows, which is how the
    // worker thread learns which folder it shows.
    SetPropW(hWnd, ET_REDIRECT_PROP, (HANDLE)1);
    SetWindowLongPtrW(hWnd, GWL_EXSTYLE, GetWindowLongPtrW(hWnd, GWL_EXSTYLE) | WS_EX_LAYERED);
    SetLayeredWindowAttributes(hWnd, 0, 0, LWA_ALPHA);

    HANDLE hThread = CreateThread(NULL, 0, ET_TabRedirectThread, (LPVOID)hWnd, 0, NULL);
    if (hThread)
    {
        CloseHandle(hThread);
    }
    else
    {
        ET_AbortTabRedirect(hWnd);
    }
}
#pragma endregion

#pragma region "Desktop: double click to hide icons, hide icon labels"
// Ports of "ZenDesktop: Desktop Icon Toggle" (manual toggle only) and "Hide Desktop Icon Text and Shortcut Arrows".
// The desktop list view and its SHELLDLL_DefView parent are subclassed when they are created.
static HWND g_hEssentialDesktopListView = NULL;
static HWND g_hEssentialHiddenDesktopListView = NULL;
// Thread that is currently painting the desktop list view (0 = none); read by the text drawing hooks.
static volatile LONG g_lEssentialDesktopPaintThread = 0;

// WM_LBUTTONDBLCLK is only sent to windows whose class has CS_DBLCLKS; otherwise two button presses are paired.
static BOOL ET_IsDesktopDoubleClick(HWND hWnd, UINT uMsg, LPARAM lParam)
{
    static DWORD dwLastClickTime = 0;
    static POINT ptLastClick;

    if (uMsg == WM_LBUTTONDBLCLK)
    {
        return TRUE;
    }
    if (uMsg != WM_LBUTTONDOWN || (GetClassLongPtrW(hWnd, GCL_STYLE) & CS_DBLCLKS))
    {
        return FALSE;
    }
    DWORD dwNow = GetTickCount();
    POINT pt;
    pt.x = GET_X_LPARAM(lParam);
    pt.y = GET_Y_LPARAM(lParam);
    if (dwNow - dwLastClickTime <= GetDoubleClickTime() &&
        abs(pt.x - ptLastClick.x) <= GetSystemMetrics(SM_CXDOUBLECLK) / 2 &&
        abs(pt.y - ptLastClick.y) <= GetSystemMetrics(SM_CYDOUBLECLK) / 2)
    {
        dwLastClickTime = 0;
        return TRUE;
    }
    dwLastClickTime = dwNow;
    ptLastClick = pt;
    return FALSE;
}

static LRESULT CALLBACK ET_DesktopListViewSubclassProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam, UINT_PTR uIdSubclass, DWORD_PTR dwRefData)
{
    UNREFERENCED_PARAMETER(dwRefData);
    if (uMsg == WM_NCDESTROY)
    {
        RemoveWindowSubclass(hWnd, ET_DesktopListViewSubclassProc, uIdSubclass);
        if (g_hEssentialDesktopListView == hWnd) g_hEssentialDesktopListView = NULL;
        if (g_hEssentialHiddenDesktopListView == hWnd) g_hEssentialHiddenDesktopListView = NULL;
    }
    else if ((uMsg == WM_LBUTTONDBLCLK || uMsg == WM_LBUTTONDOWN) && bEssentialDesktopToggleIcons)
    {
        if (ET_IsDesktopDoubleClick(hWnd, uMsg, lParam))
        {
            LVHITTESTINFO hitTestInfo;
            ZeroMemory(&hitTestInfo, sizeof(hitTestInfo));
            hitTestInfo.pt.x = GET_X_LPARAM(lParam);
            hitTestInfo.pt.y = GET_Y_LPARAM(lParam);
            if (ListView_HitTest(hWnd, &hitTestInfo) == -1)
            {
                g_hEssentialHiddenDesktopListView = hWnd;
                ShowWindow(hWnd, SW_HIDE);
                return 0;
            }
        }
    }
    else if (uMsg == WM_PAINT && bEssentialHideDesktopIconText)
    {
        InterlockedExchange(&g_lEssentialDesktopPaintThread, (LONG)GetCurrentThreadId());
        LRESULT lResult = DefSubclassProc(hWnd, uMsg, wParam, lParam);
        InterlockedExchange(&g_lEssentialDesktopPaintThread, 0);
        return lResult;
    }
    return DefSubclassProc(hWnd, uMsg, wParam, lParam);
}

// While the icons are hidden the clicks land on the SHELLDLL_DefView window behind them.
static LRESULT CALLBACK ET_DesktopDefViewSubclassProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam, UINT_PTR uIdSubclass, DWORD_PTR dwRefData)
{
    UNREFERENCED_PARAMETER(dwRefData);
    if (uMsg == WM_NCDESTROY)
    {
        RemoveWindowSubclass(hWnd, ET_DesktopDefViewSubclassProc, uIdSubclass);
    }
    else if ((uMsg == WM_LBUTTONDBLCLK || uMsg == WM_LBUTTONDOWN) && g_hEssentialHiddenDesktopListView)
    {
        HWND hListView = g_hEssentialHiddenDesktopListView;
        if (GetParent(hListView) == hWnd && !IsWindowVisible(hListView) && ET_IsDesktopDoubleClick(hWnd, uMsg, lParam))
        {
            g_hEssentialHiddenDesktopListView = NULL;
            ShowWindow(hListView, SW_SHOW);
            return 0;
        }
    }
    return DefSubclassProc(hWnd, uMsg, wParam, lParam);
}

static void ET_OnDesktopListViewCreated(HWND hListView, HWND hDefView)
{
    g_hEssentialDesktopListView = hListView;
    SetWindowSubclass(hListView, ET_DesktopListViewSubclassProc, (UINT_PTR)ET_DesktopListViewSubclassProc, 0);
    SetWindowSubclass(hDefView, ET_DesktopDefViewSubclassProc, (UINT_PTR)ET_DesktopDefViewSubclassProc, 0);
}

// Called when the settings were (re)loaded; may run on any thread.
static void ET_RefreshDesktop(BOOL bRepaint)
{
    HWND hHidden = g_hEssentialHiddenDesktopListView;
    if (hHidden && !bEssentialDesktopToggleIcons)
    {
        // The option was turned off while the icons were hidden: bring them back.
        g_hEssentialHiddenDesktopListView = NULL;
        ShowWindowAsync(hHidden, SW_SHOW);
    }
    HWND hListView = g_hEssentialDesktopListView;
    if (bRepaint && hListView)
    {
        InvalidateRect(hListView, NULL, TRUE);
    }
}

typedef int(WINAPI* ET_DrawTextW_t)(HDC hdc, LPCWSTR lpchText, int cchText, LPRECT lprc, UINT format);
typedef HRESULT(WINAPI* ET_DrawThemeTextEx_t)(HTHEME hTheme, HDC hdc, int iPartId, int iStateId, LPCWSTR pszText, int cchText, DWORD dwTextFlags, LPRECT pRect, const DTTOPTS* pOptions);
static ET_DrawTextW_t ET_DrawTextWFunc = NULL;
static ET_DrawThemeTextEx_t ET_DrawThemeTextExFunc = NULL;

static BOOL ET_IsPaintingDesktopLabels(void)
{
    LONG lThread = g_lEssentialDesktopPaintThread;
    return lThread && bEssentialHideDesktopIconText && lThread == (LONG)GetCurrentThreadId();
}

static int WINAPI ET_DrawTextWHook(HDC hdc, LPCWSTR lpchText, int cchText, LPRECT lprc, UINT format)
{
    if (lpchText && ET_IsPaintingDesktopLabels())
    {
        return ET_DrawTextWFunc(hdc, L"", 0, lprc, format);
    }
    return ET_DrawTextWFunc(hdc, lpchText, cchText, lprc, format);
}

static HRESULT WINAPI ET_DrawThemeTextExHook(HTHEME hTheme, HDC hdc, int iPartId, int iStateId, LPCWSTR pszText, int cchText, DWORD dwTextFlags, LPRECT pRect, const DTTOPTS* pOptions)
{
    if (pszText && ET_IsPaintingDesktopLabels())
    {
        return ET_DrawThemeTextExFunc(hTheme, hdc, iPartId, iStateId, L"", 0, dwTextFlags, pRect, pOptions);
    }
    return ET_DrawThemeTextExFunc(hTheme, hdc, iPartId, iStateId, pszText, cchText, dwTextFlags, pRect, pOptions);
}
#pragma endregion

#pragma region "Hide shortcut arrows"
// The shortcut overlay of the process' system image lists is replaced with a fully transparent icon. The image
// lists are per process, so the arrows come back when the option is off and File Explorer is restarted.
static const GUID ET_IID_IImageList = { 0x46EB5926, 0x582E, 0x4017, { 0x9F, 0xDF, 0xE8, 0x99, 0x8D, 0xAA, 0x09, 0x50 } };
#define ET_IDO_SHGIOI_LINK 0x0FFFFFFE
static volatile LONG g_lEssentialShortcutArrowsHidden = 0;

static HICON ET_CreateTransparentIcon(int cx, int cy)
{
    if (cx <= 0 || cy <= 0 || cx > 1024 || cy > 1024)
    {
        return NULL;
    }
    BITMAPINFO bi;
    ZeroMemory(&bi, sizeof(bi));
    bi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bi.bmiHeader.biWidth = cx;
    bi.bmiHeader.biHeight = cy;
    bi.bmiHeader.biPlanes = 1;
    bi.bmiHeader.biBitCount = 32;
    bi.bmiHeader.biCompression = BI_RGB;

    HICON hIcon = NULL;
    void* pBits = NULL;
    HDC hdc = GetDC(NULL);
    HBITMAP hColor = CreateDIBSection(hdc, &bi, DIB_RGB_COLORS, &pBits, NULL, 0);
    ReleaseDC(NULL, hdc);
    if (hColor && pBits)
    {
        ZeroMemory(pBits, (size_t)cx * cy * 4);
        size_t cbMask = (size_t)(((cx + 15) / 16) * 2) * cy;
        BYTE* pMask = (BYTE*)malloc(cbMask);
        if (pMask)
        {
            memset(pMask, 0xFF, cbMask);
            HBITMAP hMask = CreateBitmap(cx, cy, 1, 1, pMask);
            if (hMask)
            {
                ICONINFO ii;
                ZeroMemory(&ii, sizeof(ii));
                ii.fIcon = TRUE;
                ii.hbmColor = hColor;
                ii.hbmMask = hMask;
                hIcon = CreateIconIndirect(&ii);
                DeleteObject(hMask);
            }
            free(pMask);
        }
    }
    if (hColor)
    {
        DeleteObject(hColor);
    }
    return hIcon;
}

static DWORD WINAPI ET_HideShortcutArrowsThread(LPVOID lpParam)
{
    UNREFERENCED_PARAMETER(lpParam);
    // The system image list is initialized by the shell; wait for the desktop before touching it.
    for (int i = 0; i < 240 && !FindWindowW(L"Progman", NULL); ++i)
    {
        Sleep(500);
    }

    int iLinkOverlay = SHGetIconOverlayIndexW(NULL, ET_IDO_SHGIOI_LINK);
    if (iLinkOverlay <= 0)
    {
        InterlockedExchange(&g_lEssentialShortcutArrowsHidden, 0);
        return 0;
    }
    static const int shilSizes[] = { SHIL_LARGE, SHIL_SMALL, SHIL_EXTRALARGE, SHIL_SYSSMALL, SHIL_JUMBO };
    for (UINT i = 0; i < ARRAYSIZE(shilSizes); ++i)
    {
        IImageList* pImageList = NULL;
        if (FAILED(SHGetImageList(shilSizes[i], &ET_IID_IImageList, (void**)&pImageList)) || !pImageList)
        {
            continue;
        }
        int cx = 0, cy = 0;
        if (SUCCEEDED(pImageList->lpVtbl->GetIconSize(pImageList, &cx, &cy)))
        {
            HICON hBlank = ET_CreateTransparentIcon(cx, cy);
            if (hBlank)
            {
                int iIndex = -1;
                if (SUCCEEDED(pImageList->lpVtbl->ReplaceIcon(pImageList, -1, hBlank, &iIndex)) && iIndex >= 0)
                {
                    pImageList->lpVtbl->SetOverlayImage(pImageList, iIndex, iLinkOverlay);
                }
                DestroyIcon(hBlank);
            }
        }
        pImageList->lpVtbl->Release(pImageList);
    }
    SHChangeNotify(SHCNE_ASSOCCHANGED, SHCNF_IDLIST, NULL, NULL);
    return 0;
}

static void ET_HideShortcutArrowsOnce(void)
{
    if (InterlockedCompareExchange(&g_lEssentialShortcutArrowsHidden, 1, 0) != 0)
    {
        return;
    }
    HANDLE hThread = CreateThread(NULL, 0, ET_HideShortcutArrowsThread, NULL, 0, NULL);
    if (hThread)
    {
        CloseHandle(hThread);
    }
    else
    {
        InterlockedExchange(&g_lEssentialShortcutArrowsHidden, 0);
    }
}
#pragma endregion

#pragma region "Fix white flashes in File Explorer (dark mode)"
// Port of "Fix white flashes in explorer": in dark mode, File Explorer fills an off-screen surface with white and
// blits it to the DirectUIHWND view before the real content is ready. The white fill is remembered per thread, and
// the matching blit (plus up to two follow-ups) is dropped.
typedef int(WINAPI* ET_FillRect_t)(HDC hdc, const RECT* lprc, HBRUSH hbr);
typedef BOOL(WINAPI* ET_BitBlt_t)(HDC hdcDest, int x, int y, int cx, int cy, HDC hdcSrc, int x1, int y1, DWORD rop);
typedef BOOL(WINAPI* ET_ShouldAppsUseDarkMode_t)(void);
static ET_FillRect_t ET_FillRectFunc = NULL;
static ET_BitBlt_t ET_BitBltFunc = NULL;
static ET_ShouldAppsUseDarkMode_t ET_ShouldAppsUseDarkModeFunc = NULL;

typedef struct _ET_RecentWhiteFill
{
    HDC hdc;
    int w;
    int h;
    DWORD dwTick;
} ET_RecentWhiteFill;

typedef struct _ET_FollowupBlock
{
    HWND hWndDest;
    HDC hdcSrc;
    int cx;
    int cy;
    DWORD dwTick;
    int nBlocksLeft;
    BOOL bArmed;
} ET_FollowupBlock;

static __declspec(thread) ET_RecentWhiteFill t_essentialRecentFills[32];
static __declspec(thread) ET_FollowupBlock t_essentialFollowups[8];

static BOOL ET_IsWhiteFlashFixActive(void)
{
    return bEssentialFixExplorerWhiteFlash && ET_ShouldAppsUseDarkModeFunc && ET_ShouldAppsUseDarkModeFunc();
}

static BOOL ET_IsCandidateFlashSize(int w, int h)
{
    return w >= 80 && h >= 50 && (w * h) >= 6000;
}

static BOOL ET_IsWhiteishBrush(HBRUSH hBrush)
{
    LOGBRUSH logBrush;
    if (!hBrush || GetObjectW(hBrush, sizeof(logBrush), &logBrush) != sizeof(logBrush) || logBrush.lbStyle != BS_SOLID)
    {
        return FALSE;
    }
    return GetRValue(logBrush.lbColor) >= 230 && GetGValue(logBrush.lbColor) >= 230 && GetBValue(logBrush.lbColor) >= 230;
}

static int ET_FlashTolerance(int nSize, int nSmall, int nMedium, int nLarge)
{
    return nSize < 180 ? nSmall : (nSize < 300 ? nMedium : nLarge);
}

static void ET_RememberWhiteFill(HDC hdc, int w, int h)
{
    DWORD dwNow = GetTickCount();
    UINT uBest = 0;
    DWORD dwOldest = t_essentialRecentFills[0].dwTick;
    for (UINT i = 0; i < ARRAYSIZE(t_essentialRecentFills); ++i)
    {
        if (t_essentialRecentFills[i].hdc == hdc)
        {
            uBest = i;
            break;
        }
        if (t_essentialRecentFills[i].dwTick < dwOldest)
        {
            dwOldest = t_essentialRecentFills[i].dwTick;
            uBest = i;
        }
    }
    t_essentialRecentFills[uBest].hdc = hdc;
    t_essentialRecentFills[uBest].w = w;
    t_essentialRecentFills[uBest].h = h;
    t_essentialRecentFills[uBest].dwTick = dwNow;
}

// Returns TRUE (and forgets the fill) when the source DC was recently filled with white at about this size.
static BOOL ET_ConsumeRecentWhiteFill(HDC hdc, int w, int h)
{
    DWORD dwNow = GetTickCount();
    BOOL bMatch = FALSE;
    for (UINT i = 0; i < ARRAYSIZE(t_essentialRecentFills); ++i)
    {
        ET_RecentWhiteFill* pFill = &t_essentialRecentFills[i];
        if (pFill->hdc != hdc || dwNow - pFill->dwTick > 650)
        {
            continue;
        }
        if (abs(pFill->w - w) <= ET_FlashTolerance(w, 24, 18, 12) && abs(pFill->h - h) <= ET_FlashTolerance(h, 24, 18, 12))
        {
            bMatch = TRUE;
            break;
        }
    }
    if (bMatch)
    {
        for (UINT i = 0; i < ARRAYSIZE(t_essentialRecentFills); ++i)
        {
            if (t_essentialRecentFills[i].hdc == hdc)
            {
                ZeroMemory(&t_essentialRecentFills[i], sizeof(t_essentialRecentFills[i]));
            }
        }
    }
    return bMatch;
}

static void ET_ArmFollowupBlock(HWND hWndDest, HDC hdcSrc, int cx, int cy)
{
    DWORD dwNow = GetTickCount();
    UINT uBest = 0;
    DWORD dwOldest = t_essentialFollowups[0].dwTick;
    for (UINT i = 0; i < ARRAYSIZE(t_essentialFollowups); ++i)
    {
        if (t_essentialFollowups[i].bArmed && t_essentialFollowups[i].hWndDest == hWndDest)
        {
            uBest = i;
            break;
        }
        if (!t_essentialFollowups[i].bArmed)
        {
            uBest = i;
            break;
        }
        if (t_essentialFollowups[i].dwTick < dwOldest)
        {
            dwOldest = t_essentialFollowups[i].dwTick;
            uBest = i;
        }
    }
    t_essentialFollowups[uBest].hWndDest = hWndDest;
    t_essentialFollowups[uBest].hdcSrc = hdcSrc;
    t_essentialFollowups[uBest].cx = cx;
    t_essentialFollowups[uBest].cy = cy;
    t_essentialFollowups[uBest].dwTick = dwNow;
    t_essentialFollowups[uBest].nBlocksLeft = 2;
    t_essentialFollowups[uBest].bArmed = TRUE;
}

static BOOL ET_ShouldBlockFollowupBlit(HWND hWndDest, HDC hdcSrc, int cx, int cy)
{
    DWORD dwNow = GetTickCount();
    for (UINT i = 0; i < ARRAYSIZE(t_essentialFollowups); ++i)
    {
        ET_FollowupBlock* pBlock = &t_essentialFollowups[i];
        if (!pBlock->bArmed)
        {
            continue;
        }
        DWORD dwAge = dwNow - pBlock->dwTick;
        if (dwAge > 120 || pBlock->nBlocksLeft <= 0)
        {
            pBlock->bArmed = FALSE;
            continue;
        }
        if (pBlock->hWndDest != hWndDest ||
            abs(pBlock->cx - cx) > ET_FlashTolerance(cx, 28, 20, 14) ||
            abs(pBlock->cy - cy) > ET_FlashTolerance(cy, 28, 20, 14))
        {
            continue;
        }
        if (pBlock->hdcSrc != hdcSrc && dwAge > 45)
        {
            continue;
        }
        pBlock->hdcSrc = hdcSrc;
        pBlock->cx = cx;
        pBlock->cy = cy;
        pBlock->dwTick = dwNow;
        if (--pBlock->nBlocksLeft <= 0)
        {
            pBlock->bArmed = FALSE;
        }
        return TRUE;
    }
    return FALSE;
}

// The blit has to target (nearly) the whole client area of a File Explorer DirectUIHWND view.
static BOOL ET_IsExplorerViewSurfaceBlit(HWND hWndDest, int x, int y, int cx, int cy)
{
    RECT rcClient;
    if (!hWndDest || !ET_IsWindowOfClass(hWndDest, L"DirectUIHWND"))
    {
        return FALSE;
    }
    HWND hRoot = GetAncestor(hWndDest, GA_ROOT);
    if (!ET_IsWindowOfClass(hRoot, L"CabinetWClass") && !ET_IsWindowOfClass(hRoot, L"ExploreWClass"))
    {
        return FALSE;
    }
    if (!GetClientRect(hWndDest, &rcClient))
    {
        return FALSE;
    }
    int nClientW = rcClient.right - rcClient.left, nClientH = rcClient.bottom - rcClient.top;
    if (nClientW <= 0 || nClientH <= 0 || abs(x) > 8 || abs(y) > 8)
    {
        return FALSE;
    }
    int nWidthSlack = max(24, nClientW / 8), nHeightSlack = max(24, nClientH / 8);
    if (cx < nClientW - nWidthSlack || cy < nClientH - nHeightSlack)
    {
        return FALSE;
    }
    return (LONGLONG)cx * cy * 100 >= (LONGLONG)nClientW * nClientH * 70;
}

static int WINAPI ET_FillRectHook(HDC hdc, const RECT* lprc, HBRUSH hbr)
{
    if (lprc && hbr && ET_IsWhiteFlashFixActive())
    {
        int w = lprc->right - lprc->left, h = lprc->bottom - lprc->top;
        // Only memory DCs (no window) are of interest.
        if (ET_IsCandidateFlashSize(w, h) && !WindowFromDC(hdc) && ET_IsWhiteishBrush(hbr))
        {
            ET_RememberWhiteFill(hdc, w, h);
        }
    }
    return ET_FillRectFunc(hdc, lprc, hbr);
}

static BOOL WINAPI ET_BitBltHook(HDC hdcDest, int x, int y, int cx, int cy, HDC hdcSrc, int x1, int y1, DWORD rop)
{
    if (rop == SRCCOPY && ET_IsCandidateFlashSize(cx, cy) && ET_IsWhiteFlashFixActive())
    {
        HWND hWndDest = WindowFromDC(hdcDest);
        if (ET_IsExplorerViewSurfaceBlit(hWndDest, x, y, cx, cy))
        {
            if (ET_ShouldBlockFollowupBlit(hWndDest, hdcSrc, cx, cy))
            {
                return TRUE;
            }
            if (ET_ConsumeRecentWhiteFill(hdcSrc, cx, cy))
            {
                ET_ArmFollowupBlock(hWndDest, hdcSrc, cx, cy);
                return TRUE;
            }
        }
    }
    return ET_BitBltFunc(hdcDest, x, y, cx, cy, hdcSrc, x1, y1, rop);
}
#pragma endregion

#pragma region "Always show all tray icons"
// Port of "Always show all taskbar tray icons": Windows 11 keeps one "IsPromoted" value per tray icon under
// HKCU\Control Panel\NotifyIconSettings\<id>. While the option is on, reads of that value return 1 and writes to it
// are swallowed, so the user's own choices are still there when the option is turned off again.
typedef LSTATUS(WINAPI* ET_RegGetValueW_t)(HKEY hkey, LPCWSTR lpSubKey, LPCWSTR lpValue, DWORD dwFlags, LPDWORD pdwType, PVOID pvData, LPDWORD pcbData);
typedef LSTATUS(WINAPI* ET_RegSetValueExW_t)(HKEY hKey, LPCWSTR lpValueName, DWORD Reserved, DWORD dwType, const BYTE* lpData, DWORD cbData);
typedef LONG(NTAPI* ET_NtQueryKey_t)(HANDLE KeyHandle, int KeyInformationClass, PVOID KeyInformation, ULONG Length, PULONG ResultLength);
static ET_RegGetValueW_t ET_RegGetValueWFunc = NULL;
static ET_RegSetValueExW_t ET_RegSetValueExWFunc = NULL;
static ET_NtQueryKey_t ET_NtQueryKeyFunc = NULL;

// TRUE when the key is \REGISTRY\USER\<sid>\Control Panel\NotifyIconSettings\<id>.
static BOOL ET_IsNotifyIconSettingsKey(HKEY hKey)
{
    struct
    {
        ULONG NameLength;
        WCHAR Name[300];
    } info;
    static const WCHAR wszPrefix[] = L"\\REGISTRY\\USER\\";
    static const WCHAR wszMiddle[] = L"\\Control Panel\\NotifyIconSettings\\";
    ULONG cbResult = 0;

    // Predefined keys (HKEY_CURRENT_USER, ...) are not real handles; NtQueryKey simply fails for them.
    if (!hKey || !ET_NtQueryKeyFunc)
    {
        return FALSE;
    }
    // 3 = KeyNameInformation; longer names cannot be the key in question, so a failure is a "no".
    if (ET_NtQueryKeyFunc(hKey, 3, &info, sizeof(info) - sizeof(WCHAR), &cbResult) != 0)
    {
        return FALSE;
    }
    ULONG cch = info.NameLength / sizeof(WCHAR);
    if (cch >= ARRAYSIZE(info.Name))
    {
        return FALSE;
    }
    info.Name[cch] = 0;
    if (_wcsnicmp(info.Name, wszPrefix, ARRAYSIZE(wszPrefix) - 1) != 0)
    {
        return FALSE;
    }
    const WCHAR* pSid = info.Name + ARRAYSIZE(wszPrefix) - 1;
    const WCHAR* pMiddle = wcschr(pSid, L'\\');
    if (!pMiddle || _wcsnicmp(pMiddle, wszMiddle, ARRAYSIZE(wszMiddle) - 1) != 0)
    {
        return FALSE;
    }
    const WCHAR* pEntry = pMiddle + ARRAYSIZE(wszMiddle) - 1;
    return pEntry[0] && !wcschr(pEntry, L'\\');
}

static LSTATUS WINAPI ET_RegGetValueWHook(HKEY hkey, LPCWSTR lpSubKey, LPCWSTR lpValue, DWORD dwFlags, LPDWORD pdwType, PVOID pvData, LPDWORD pcbData)
{
    if (bEssentialShowAllTrayIcons && !lpSubKey && lpValue && (dwFlags & RRF_RT_REG_DWORD) && pvData && pcbData &&
        *pcbData >= sizeof(DWORD) && !_wcsicmp(lpValue, L"IsPromoted") && ET_IsNotifyIconSettingsKey(hkey))
    {
        if (pdwType)
        {
            *pdwType = REG_DWORD;
        }
        *(DWORD*)pvData = 1;
        *pcbData = sizeof(DWORD);
        return ERROR_SUCCESS;
    }
    return ET_RegGetValueWFunc(hkey, lpSubKey, lpValue, dwFlags, pdwType, pvData, pcbData);
}

static LSTATUS WINAPI ET_RegSetValueExWHook(HKEY hKey, LPCWSTR lpValueName, DWORD Reserved, DWORD dwType, const BYTE* lpData, DWORD cbData)
{
    if (bEssentialShowAllTrayIcons && lpValueName && !_wcsicmp(lpValueName, L"IsPromoted") && ET_IsNotifyIconSettingsKey(hKey))
    {
        return ERROR_SUCCESS;
    }
    return ET_RegSetValueExWFunc(hKey, lpValueName, Reserved, dwType, lpData, cbData);
}

// Explorer watches every NotifyIconSettings subkey; writing and removing a temporary value makes it read the
// (hooked) IsPromoted values again.
static void ET_TouchAllNotifyIconSettings(void)
{
    static const WCHAR wszTempValue[] = L"_temp_ExplorerPatcher_EssentialShowAllTrayIcons";
    HKEY hKey = NULL;
    if (RegOpenKeyExW(HKEY_CURRENT_USER, L"Control Panel\\NotifyIconSettings", 0, KEY_READ, &hKey) != ERROR_SUCCESS)
    {
        return;
    }
    for (DWORD dwIndex = 0; dwIndex < 4096; ++dwIndex)
    {
        WCHAR wszSubKey[128];
        DWORD cchSubKey = ARRAYSIZE(wszSubKey);
        LSTATUS lStatus = RegEnumKeyExW(hKey, dwIndex, wszSubKey, &cchSubKey, NULL, NULL, NULL, NULL);
        if (lStatus == ERROR_MORE_DATA)
        {
            continue;
        }
        if (lStatus != ERROR_SUCCESS)
        {
            break;
        }
        HKEY hSubKey = NULL;
        if (RegOpenKeyExW(hKey, wszSubKey, 0, KEY_SET_VALUE, &hSubKey) == ERROR_SUCCESS)
        {
            if (RegSetValueExW(hSubKey, wszTempValue, 0, REG_SZ, (const BYTE*)L"", sizeof(WCHAR)) == ERROR_SUCCESS)
            {
                RegDeleteValueW(hSubKey, wszTempValue);
            }
            RegCloseKey(hSubKey);
        }
    }
    RegCloseKey(hKey);
}
#pragma endregion

#pragma region "Remove the startup apps delay"
// Registry based variant of "Startup App Delay Fix": Explorer reads these two values before it launches the
// startup applications. Values that already exist are left alone, and only the ones created here are removed again.
#define ET_SERIALIZE_KEY L"Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\Serialize"
#define ET_STARTUP_DELAY_STATE_VALUE L"EssentialNoStartupDelayApplied"

static void ET_ApplyStartupDelay(HKEY hKey)
{
    static const WCHAR* const wszValues[] = { L"StartupDelayInMSec", L"WaitforIdleState" };
    // Bit 0: the option was processed as "on"; bits 8..: values created by this module.
    DWORD dwState = ET_ReadDword(hKey, ET_STARTUP_DELAY_STATE_VALUE, 0);
    BOOL bProcessedOn = (dwState & 1) != 0;
    if ((bEssentialNoStartupDelay != FALSE) == bProcessedOn)
    {
        return;
    }
    DWORD dwOwned = (dwState >> 8) & 0x3;
    for (UINT i = 0; i < ARRAYSIZE(wszValues); ++i)
    {
        if (bEssentialNoStartupDelay)
        {
            DWORD dwSize = 0;
            if (RegGetValueW(HKEY_CURRENT_USER, ET_SERIALIZE_KEY, wszValues[i], RRF_RT_ANY, NULL, NULL, &dwSize) == ERROR_SUCCESS)
            {
                continue;
            }
            DWORD dwZero = 0;
            if (RegSetKeyValueW(HKEY_CURRENT_USER, ET_SERIALIZE_KEY, wszValues[i], REG_DWORD, &dwZero, sizeof(DWORD)) == ERROR_SUCCESS)
            {
                dwOwned |= (1u << i);
            }
        }
        else if (dwOwned & (1u << i))
        {
            RegDeleteKeyValueW(HKEY_CURRENT_USER, ET_SERIALIZE_KEY, wszValues[i]);
            dwOwned &= ~(1u << i);
        }
    }
    dwState = (bEssentialNoStartupDelay ? 1 : 0) | (dwOwned << 8);
    RegSetValueExW(hKey, ET_STARTUP_DELAY_STATE_VALUE, 0, REG_DWORD, (const BYTE*)&dwState, sizeof(DWORD));
}
#pragma endregion

#pragma region "Block F1 help"
// Port of "F1 Blocker" for File Explorer: a plain F1 key press is swallowed before the accelerator table turns it
// into the "get help" command (which opens a web browser).
typedef int(WINAPI* ET_TranslateAcceleratorW_t)(HWND hWnd, HACCEL hAccTable, LPMSG lpMsg);
static ET_TranslateAcceleratorW_t ET_TranslateAcceleratorWFunc = NULL;

static int WINAPI ET_TranslateAcceleratorWHook(HWND hWnd, HACCEL hAccTable, LPMSG lpMsg)
{
    if (bEssentialBlockF1Help && lpMsg && lpMsg->message == WM_KEYDOWN && lpMsg->wParam == VK_F1 &&
        !(GetKeyState(VK_CONTROL) & 0x8000) && !(GetKeyState(VK_SHIFT) & 0x8000) && !(GetKeyState(VK_MENU) & 0x8000))
    {
        return TRUE;
    }
    return ET_TranslateAcceleratorWFunc(hWnd, hAccTable, lpMsg);
}
#pragma endregion

#pragma region "Hooks installed on demand"
// These hooks sit on very hot functions, so each group is only installed the first time its option is on
// (SlimDetours hooks take effect immediately). They stay installed afterwards and check the option themselves.
#if WITH_MAIN_PATCHER
static BOOL g_bEssentialHooksReady = FALSE;
static SRWLOCK g_essentialHooksLock = SRWLOCK_INIT;

static void ET_InstallHook(HMODULE hModule, LPCSTR pszName, void** ppFunc, void* pHook)
{
    if (*ppFunc || !hModule)
    {
        return;
    }
    *ppFunc = (void*)GetProcAddress(hModule, pszName);
    if (*ppFunc && funchook_prepare(funchook, ppFunc, pHook) != 0)
    {
        *ppFunc = NULL;
    }
}
#endif

static void ET_UpdateOnDemandHooks(void)
{
#if WITH_MAIN_PATCHER
    static BOOL bLastShowAllTrayIcons = FALSE;
    BOOL bTouchTrayIcons = FALSE;

    AcquireSRWLockExclusive(&g_essentialHooksLock);
    if (g_bEssentialHooksReady)
    {
        if (bEssentialBlockF1Help)
        {
            ET_InstallHook(GetModuleHandleW(L"user32.dll"), "TranslateAcceleratorW", (void**)&ET_TranslateAcceleratorWFunc, ET_TranslateAcceleratorWHook);
        }
        if (bEssentialHideDesktopIconText)
        {
            ET_InstallHook(GetModuleHandleW(L"user32.dll"), "DrawTextW", (void**)&ET_DrawTextWFunc, ET_DrawTextWHook);
            ET_InstallHook(LoadLibraryExW(L"uxtheme.dll", NULL, LOAD_LIBRARY_SEARCH_SYSTEM32), "DrawThemeTextEx", (void**)&ET_DrawThemeTextExFunc, ET_DrawThemeTextExHook);
        }
        if (bEssentialFixExplorerWhiteFlash)
        {
            if (!ET_ShouldAppsUseDarkModeFunc)
            {
                HMODULE hUxtheme = LoadLibraryExW(L"uxtheme.dll", NULL, LOAD_LIBRARY_SEARCH_SYSTEM32);
                ET_ShouldAppsUseDarkModeFunc = hUxtheme ? (ET_ShouldAppsUseDarkMode_t)GetProcAddress(hUxtheme, MAKEINTRESOURCEA(132)) : NULL;
            }
            if (ET_ShouldAppsUseDarkModeFunc)
            {
                ET_InstallHook(GetModuleHandleW(L"user32.dll"), "FillRect", (void**)&ET_FillRectFunc, ET_FillRectHook);
                ET_InstallHook(GetModuleHandleW(L"gdi32.dll"), "BitBlt", (void**)&ET_BitBltFunc, ET_BitBltHook);
            }
        }
        if (bEssentialShowAllTrayIcons && IsWindows11Version22H2OrHigher())
        {
            if (!ET_NtQueryKeyFunc)
            {
                ET_NtQueryKeyFunc = (ET_NtQueryKey_t)GetProcAddress(GetModuleHandleW(L"ntdll.dll"), "NtQueryKey");
            }
            if (ET_NtQueryKeyFunc)
            {
                ET_InstallHook(GetModuleHandleW(L"kernelbase.dll"), "RegGetValueW", (void**)&ET_RegGetValueWFunc, ET_RegGetValueWHook);
                ET_InstallHook(GetModuleHandleW(L"kernelbase.dll"), "RegSetValueExW", (void**)&ET_RegSetValueExWFunc, ET_RegSetValueExWHook);
            }
        }
        if (ET_RegGetValueWFunc && bLastShowAllTrayIcons != (bEssentialShowAllTrayIcons != FALSE))
        {
            bLastShowAllTrayIcons = (bEssentialShowAllTrayIcons != FALSE);
            bTouchTrayIcons = TRUE;
        }
    }
    ReleaseSRWLockExclusive(&g_essentialHooksLock);

    if (bTouchTrayIcons)
    {
        ET_TouchAllNotifyIconSettings();
    }
#endif
}
#pragma endregion

void EssentialTweaks_PrepareHooks(void)
{
#if WITH_MAIN_PATCHER
    if (!funchook)
    {
        return;
    }
    // The hook is always installed; the setting is evaluated when the message box is about to be shown,
    // so that the option takes effect without restarting File Explorer.
    ET_PrepareShellMessageBoxHooks();

    AcquireSRWLockExclusive(&g_essentialHooksLock);
    g_bEssentialHooksReady = TRUE;
    ReleaseSRWLockExclusive(&g_essentialHooksLock);
    ET_UpdateOnDemandHooks();
#endif
}

void EssentialTweaks_Start(void)
{
    if (dwEssentialTrayIconFix)
    {
        HANDLE hThread = CreateThread(NULL, 0, ET_TrayIconFixThread, (LPVOID)(DWORD_PTR)dwEssentialTrayIconFix, 0, NULL);
        if (hThread)
        {
            CloseHandle(hThread);
        }
    }
    if (bEssentialReopenClosedTab && IsWindows11Version22H2OrHigher())
    {
        ET_StartReopenClosedTab();
    }
}
