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

// Returns the parsing name (file system path, or ::{CLSID} for virtual folders) of the folder shown by a browser.
static BOOL ET_GetBrowserFolderPath(IShellBrowser* pShellBrowser, WCHAR* wszPath, size_t cch)
{
    BOOL bSuccess = FALSE;
    IShellView* pShellView = NULL;
    if (!wszPath || cch == 0)
    {
        return FALSE;
    }
    wszPath[0] = 0;
    if (SUCCEEDED(pShellBrowser->lpVtbl->QueryActiveShellView(pShellBrowser, &pShellView)) && pShellView)
    {
        IFolderView* pFolderView = NULL;
        if (SUCCEEDED(pShellView->lpVtbl->QueryInterface(pShellView, &ET_IID_IFolderView, (LPVOID*)&pFolderView)) && pFolderView)
        {
            IPersistFolder2* pPersistFolder2 = NULL;
            if (SUCCEEDED(pFolderView->lpVtbl->GetFolder(pFolderView, &ET_IID_IPersistFolder2, (LPVOID*)&pPersistFolder2)) && pPersistFolder2)
            {
                PIDLIST_ABSOLUTE pidl = NULL;
                if (SUCCEEDED(pPersistFolder2->lpVtbl->GetCurFolder(pPersistFolder2, &pidl)) && pidl)
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
                pPersistFolder2->lpVtbl->Release(pPersistFolder2);
            }
            pFolderView->lpVtbl->Release(pFolderView);
        }
        pShellView->lpVtbl->Release(pShellView);
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
    if (!hWnd || !hWndParent || !ET_GetClassName(hWnd, wszClass, ARRAYSIZE(wszClass)))
    {
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
    // Only File Explorer views (the desktop also has a SHELLDLL_DefView, whose parent is not ShellTabWindowClass).
    if (!ET_IsWindowOfClass(GetParent(hWndParent), L"ShellTabWindowClass"))
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

static BOOL ET_OpenPathAsTab(ET_TabsContext* pContext, HWND hExplorer, const WCHAR* wszPath)
{
    // The new tab command has to go to the tab strip (ShellTabWindowClass), not the top-level window;
    // otherwise the tab is not registered with IShellWindows.
    HWND hTabStrip = FindWindowExW(hExplorer, NULL, L"ShellTabWindowClass", NULL);
    if (!hTabStrip)
    {
        return FALSE;
    }

    // Snapshot the tabs that exist now, so that the new one can be told apart afterwards.
    pContext->cCurrent = ET_CollectTabs(pContext->current, ET_MAX_TABS);

    DWORD_PTR dwResult = 0;
    if (!SendMessageTimeoutW(hTabStrip, WM_COMMAND, ET_NEW_TAB_COMMAND, 0, SMTO_ABORTIFHUNG | SMTO_NORMAL, 3000, &dwResult))
    {
        return FALSE;
    }

    ET_FindNewTabContext findContext;
    findContext.pBefore = pContext->current;
    findContext.cBefore = pContext->cCurrent;
    findContext.pNewTab = NULL;

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
        VARIANT vPath, vEmpty;
        VariantInit(&vPath);
        VariantInit(&vEmpty);
        vPath.vt = VT_BSTR;
        vPath.bstrVal = SysAllocString(wszPath);
        if (vPath.bstrVal)
        {
            bSuccess = SUCCEEDED(pWebBrowser->lpVtbl->Navigate2(pWebBrowser, &vPath, &vEmpty, &vEmpty, &vEmpty, &vEmpty));
            SysFreeString(vPath.bstrVal);
        }
        pWebBrowser->lpVtbl->Release(pWebBrowser);
    }
    findContext.pNewTab->lpVtbl->Release(findContext.pNewTab);
    return bSuccess;
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
