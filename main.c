#define _WIN32_WINNT 0x0400
#include <windows.h>
#include <stdio.h>
#include <tchar.h>
#include <Psapi.h>
#include <strsafe.h>
#include "resource.h"

#define TRAY_ICON_ID    1
#define WM_TRAY_ICON    WM_USER

typedef struct
{
  TCHAR  lpszName[128];
  HWND   hWnd;
  BOOL   bShown;
  BOOL   bIconic;
} WINDOWDATA;

typedef WINDOWDATA* LPWINDOWDATA;

LPWINDOWDATA CreateWindowData();
VOID ReleaseWindowData(LPWINDOWDATA);

typedef struct
{
  HWND         fgWnd;
  LPWINDOWDATA wndData;
  LONG         wndDataSize;
} WINDOWDATALIST;

typedef WINDOWDATALIST* LPWINDOWDATALIST;

LPWINDOWDATALIST CreateWindowDataList();
VOID ReleaseWindowDataList(LPWINDOWDATALIST);

HINSTANCE  ghInst;
LPWINDOWDATALIST gWndDataList;
LONG       iDesktopNumber;
LONG       iDesktopActivated;
UINT       uActivationMsg;
UINT       uUnActivationMsg;
UINT       uDestroyMsg;
UINT       uMoveMsg;
LPCTSTR    szActivationMsg = _T("WM_WINHIDE_ACTIVATION");
LPCTSTR    szUnActivationMsg = _T("WM_WINHIDE_UNACTIVATION");
LPCTSTR    szDestroyMsg = _T("WM_WINHIDE_DESTROY");
LPCTSTR    szMoveMsg = _T("WM_WINHIDE_MOVE");
HANDLE     hSlideTimer   = NULL;
LONG       iPressedTime = 0;

BOOL AddIcon(HWND, UINT, LPTSTR, LPCTSTR);
BOOL RetIcon(HWND, UINT);
BOOL ChangeIcon(HWND, UINT, LPTSTR);
VOID RegisterHotKeys(HWND);
VOID UnRegisterHotKeys(HWND);
VOID UpdateDataList();
VOID CopyWindowData(LPWINDOWDATA, LPWINDOWDATA);
VOID ClearMenu(HMENU);
VOID FillMenu(HMENU);
VOID UpdateMenu(HMENU);
VOID HideWindow(LONG);
VOID DisplayWindow(LONG);
BOOL CompareProcessName(DWORD, LPCTSTR);
VOID DetermineDesktopNumber();
LONG GetSlidePixel();

void ErrorExit(LPTSTR lpszFunction) 
{ 
    // Retrieve the system error message for the last-error code
    LPVOID lpMsgBuf;
    LPVOID lpDisplayBuf;
    DWORD dw = GetLastError(); 

    FormatMessage(
        FORMAT_MESSAGE_ALLOCATE_BUFFER | 
        FORMAT_MESSAGE_FROM_SYSTEM |
        FORMAT_MESSAGE_IGNORE_INSERTS,
        NULL,
        dw,
        MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
        (LPTSTR) &lpMsgBuf,
        0, NULL );

    // Display the error message and exit the process
    lpDisplayBuf = (LPVOID)LocalAlloc(LMEM_ZEROINIT, 
        (lstrlen((LPCTSTR)lpMsgBuf) + lstrlen((LPCTSTR)lpszFunction) + 40) * sizeof(TCHAR)); 
    StringCchPrintf((LPTSTR)lpDisplayBuf, 
        LocalSize(lpDisplayBuf) / sizeof(TCHAR),
        TEXT("%s failed with error %d: %s"), 
        lpszFunction, dw, lpMsgBuf); 
    MessageBox(NULL, (LPCTSTR)lpDisplayBuf, TEXT("Error"), MB_OK); 

    LocalFree(lpMsgBuf);
    LocalFree(lpDisplayBuf);
    //ExitProcess(dw); 
}

BOOL AddIcon(HWND hWnd,
			 UINT id,
			 LPTSTR IconName,
			 LPCTSTR tooltip)
{
	BOOL res;

	NOTIFYICONDATA tnid;
	tnid.cbSize = sizeof(NOTIFYICONDATA);
	tnid.hWnd = hWnd;
	tnid.uID = id;
	tnid.uFlags =NIF_MESSAGE | NIF_ICON | NIF_TIP;	
	tnid.uCallbackMessage = WM_TRAY_ICON;
    tnid.hIcon = LoadIcon(ghInst,IconName);
    lstrcpyn(tnid.szTip, tooltip, sizeof(tnid.szTip));
	res = Shell_NotifyIcon(NIM_ADD, &tnid);
	return res;
}

BOOL ChangeIcon(HWND hWnd,
                UINT id,
                LPTSTR IconName)
{
	BOOL res;

	NOTIFYICONDATA tnid;
	tnid.cbSize = sizeof(NOTIFYICONDATA);
	tnid.hWnd = hWnd;
	tnid.uID = id;
	tnid.uFlags = NIF_ICON;	
	tnid.uCallbackMessage = WM_TRAY_ICON;
    tnid.hIcon = LoadIcon(ghInst,IconName);

	res = Shell_NotifyIcon(NIM_MODIFY, &tnid);
	return res;
}

BOOL RetIcon(HWND hWnd,	
			 UINT id)
{
	BOOL res;

	NOTIFYICONDATA tnid;
	tnid.cbSize = sizeof(NOTIFYICONDATA);
	tnid.hWnd = hWnd;
	tnid.uID = id;

	res = Shell_NotifyIcon(NIM_DELETE, &tnid);
	return res;
}

VOID RegisterHotKeys(HWND hWnd)
{
    if (!RegisterHotKey(hWnd, IDM_NONE, MOD_ALT, '0'))
        ErrorExit(_T("RegisterHotKey ALT+0"));
    if (!RegisterHotKey(hWnd, IDM_ALL, MOD_ALT, '9'))
        ErrorExit(_T("RegisterHotKey ALT+9"));
    if (!RegisterHotKey(hWnd, IDM_INVERT, MOD_ALT, '8'))
        ErrorExit(_T("RegisterHotKey ALT+8"));
    if (!RegisterHotKey(hWnd, IDM_TAB, MOD_ALT, 'T'))
        ErrorExit(_T("RegisterHotKey ALT+T"));

    if (!RegisterHotKey(hWnd, IDM_RIGHT, MOD_CONTROL, VK_RIGHT))
        ErrorExit(_T("RegisterHotKey CTRL+RIGHT"));
    if (!RegisterHotKey(hWnd, IDM_LEFT, MOD_CONTROL, VK_LEFT))
        ErrorExit(_T("RegisterHotKey CTRL+LEFT"));
    if (!RegisterHotKey(hWnd, IDM_UP, MOD_CONTROL, VK_UP))
        ErrorExit(_T("RegisterHotKey CTRL+UP"));
    if (!RegisterHotKey(hWnd, IDM_DOWN, MOD_CONTROL, VK_DOWN))
        ErrorExit(_T("RegisterHotKey CTRL+DOWN"));
}

VOID UnRegisterHotKeys(HWND hWnd)
{
    if (!UnregisterHotKey(hWnd, IDM_NONE))
        ErrorExit(_T("UnregisterHotKey ALT+0"));
    if (!UnregisterHotKey(hWnd, IDM_ALL))
        ErrorExit(_T("UnregisterHotKey ALT+9"));
    if (!UnregisterHotKey(hWnd, IDM_INVERT))
        ErrorExit(_T("UnregisterHotKey ALT+8"));
    if (!UnregisterHotKey(hWnd, IDM_TAB))
        ErrorExit(_T("UnregisterHotKey ALT+T"));

    if (!UnregisterHotKey(hWnd, IDM_RIGHT))
        ErrorExit(_T("UnregisterHotKey CTRL+RIGHT"));
    if (!UnregisterHotKey(hWnd, IDM_LEFT))
        ErrorExit(_T("UnregisterHotKey CTRL+LEFT"));
    if (!UnregisterHotKey(hWnd, IDM_UP))
        ErrorExit(_T("UnregisterHotKey CTRL+UP"));
    if (!UnregisterHotKey(hWnd, IDM_DOWN))
        ErrorExit(_T("UnregisterHotKey CTRL+DOWN"));
}

BOOL CALLBACK EnumWindowsProc(HWND hWnd, LPARAM lParam) 
{
    TCHAR szName[128];
    TCHAR szClass[128];
    LPWINDOWDATALIST list = (LPWINDOWDATALIST) lParam;
    BOOL bShown = IsWindowVisible(hWnd);
    BOOL bIconic = IsIconic(hWnd);
    LONG style = GetWindowLong(hWnd, GWL_STYLE);
    GetWindowText(hWnd, szName, 128);
    GetClassName(hWnd, szClass, 128);
    if (list != NULL && 
        bShown && 
        (_tcslen(szName) > 0 ||
        (style&WS_CHILDWINDOW) == WS_CHILDWINDOW || 
        (style&WS_SYSMENU) == WS_SYSMENU ||
         _tccmp(_T("#32770"), szClass) == 0) &&
        list->wndDataSize<128)
    {
        list->wndData[list->wndDataSize].hWnd = hWnd;
        if (_tcslen(szName) > 0)
            _tcscpy_s(list->wndData[list->wndDataSize].lpszName, 128, szName);
        else
            _tcscpy_s(list->wndData[list->wndDataSize].lpszName, 128, szClass);
        list->wndData[list->wndDataSize].bShown  = bShown;
        list->wndData[list->wndDataSize].bIconic = bIconic;
        list->wndDataSize++;
    }
    return TRUE;
}

VOID CopyWindowData(LPWINDOWDATA dest, LPWINDOWDATA src)
{
    if (dest != NULL && src != NULL)
    {
        dest->hWnd = src->hWnd;
        _tcscpy_s(dest->lpszName, 128, src->lpszName);
        dest->bShown = src->bShown;
        dest->bIconic = src->bIconic;
    }
}

LPWINDOWDATA CreateWindowData()
{
    LPWINDOWDATA data =(LPWINDOWDATA) malloc(sizeof(WINDOWDATA)*128);
    memset(data, 0, sizeof(WINDOWDATA)*128);
    return data;
}

VOID ReleaseWindowData(LPWINDOWDATA data)
{
    if (data!=0)
    {
        free(data);
    }
}

VOID UpdateDataList()
{
    LONG i = 0;
    LONG dataSaveSize = 0;
    LPWINDOWDATA dataSave = CreateWindowData();
    for (i=0; i<gWndDataList->wndDataSize; ++i)
    {
        if (gWndDataList->wndData[i].bShown == FALSE)
        {
            CopyWindowData(&dataSave[dataSaveSize], &gWndDataList->wndData[i]);
            dataSaveSize++;
        }
    }
    for (i=0; i<dataSaveSize; ++i)
    {
        CopyWindowData(&gWndDataList->wndData[i], &dataSave[i]);
    }
    gWndDataList->wndDataSize = dataSaveSize;
    EnumWindows(EnumWindowsProc, (LPARAM) gWndDataList);
    gWndDataList->fgWnd = GetForegroundWindow();
    ReleaseWindowData(dataSave);
}

LPWINDOWDATALIST CreateWindowDataList()
{
    LPWINDOWDATALIST data =(LPWINDOWDATALIST) malloc(sizeof(WINDOWDATALIST));
    memset(data, 0, sizeof(WINDOWDATALIST));
    data->wndData = CreateWindowData();
    return data;
}

VOID ReleaseWindowDataList(LPWINDOWDATALIST data)
{
    if (data!=0)
    {
        ReleaseWindowData(data->wndData);
        free(data);
    }
}

VOID ClearMenu(HMENU hMenu)
{
    LONG i = 0;
    int count = GetMenuItemCount(hMenu);
    for (i=0; i<count-1; ++i)
    {
        DeleteMenu(hMenu, 1, MF_BYPOSITION);
    }
}

VOID FillMenu(HMENU hMenu)
{
    LONG i=0;
    if (gWndDataList->wndDataSize > 0)
    {
        AppendMenu(hMenu, MF_STRING, IDM_NONE,   _T("Cacher Aucune\tALT+0"));
        AppendMenu(hMenu, MF_STRING, IDM_ALL,    _T("Cacher Toutes\tALT+9"));
        AppendMenu(hMenu, MF_STRING, IDM_INVERT, _T("Inverser\tALT+8"));
        AppendMenu(hMenu, MF_SEPARATOR, 0, NULL);
    }
    for (i=0; i<gWndDataList->wndDataSize; ++i)
    {
        AppendMenu(hMenu, MF_STRING|gWndDataList->wndData[i].bShown?MF_UNCHECKED:MF_CHECKED, IDM_APP+i,gWndDataList->wndData[i].lpszName);
    }    
}

VOID UpdateMenu(HMENU hMenu)
{
    ClearMenu(hMenu);
    if (iDesktopActivated == iDesktopNumber)
    {
        UpdateDataList();
        FillMenu(hMenu);
    }
}

VOID HideWindow(LONG i)
{
    ShowWindowAsync(gWndDataList->wndData[i].hWnd, SW_HIDE);
}

VOID DisplayWindow(LONG i)
{
    ShowWindowAsync(gWndDataList->wndData[i].hWnd, gWndDataList->wndData[i].bIconic?SW_MINIMIZE:SW_SHOW);
}

LONG GetSlidePixel()
{
    LONG iPixel = 1;
    LARGE_INTEGER liDueTime;
    liDueTime.QuadPart = -500000LL;
    if (WaitForSingleObject(hSlideTimer, 0) == WAIT_TIMEOUT)
    {
        if (iPressedTime >= 60)
        {
            iPixel = 10;
        }
        else 
        {
            iPixel = 5;
        }
        iPressedTime++;
    }
    else
    {
        iPressedTime = 0;
    }
    SetWaitableTimer(hSlideTimer, &liDueTime, 0, NULL, NULL, 0);
    return iPixel;
}

LRESULT CALLBACK WindowProc	(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    POINT point;
    static HMENU hMenu;
    static LPCTSTR szTooltip = _T("WinHide\nAllows to hide windows app.");
 
    if (message == uActivationMsg)      // Activation Message
    {
        if (wParam != iDesktopNumber)   // Other Desktop
        {
            if (iDesktopActivated == iDesktopNumber)    // old Activated Desktop
            {
                LONG i = 0;
                // Hide
                UpdateDataList();
                for (i=0; i<gWndDataList->wndDataSize; ++i)
                {
                    if (gWndDataList->wndData[i].bShown == TRUE)
                    {
                        HideWindow(i);
                    }
                }
                UnRegisterHotKeys(hwnd);
                ChangeIcon(hwnd, TRAY_ICON_ID, MAKEINTRESOURCE(IDI_ICON1+iDesktopNumber));
                PostMessage(HWND_BROADCAST, uUnActivationMsg, iDesktopNumber, (LPARAM) NULL);
            }
        }
        iDesktopActivated = (LONG) wParam;
        return 0;
    }

    if (message == uUnActivationMsg)
    {
        if (iDesktopActivated == iDesktopNumber)    // New Activated Desktop
        {
            LONG i = 0;
            // Show
            for (i=0; i<gWndDataList->wndDataSize; ++i)
            {
                if (gWndDataList->wndData[i].bShown == TRUE)
                {
                    DisplayWindow(i);
                }
            }
            SetForegroundWindow(gWndDataList->fgWnd);
            RegisterHotKeys(hwnd);
            ChangeIcon(hwnd, TRAY_ICON_ID, MAKEINTRESOURCE(IDI_ENABLE_ICON1+iDesktopNumber));
        }
        return 0;
    }

    if (message == uDestroyMsg)
    {
        if ((LONG)wParam+1 == iDesktopNumber)   // Other Desktop
        {
            iDesktopNumber--;
            if (!UnregisterHotKey(hwnd, IDM_ACTIVATE))
                ErrorExit(_T("UnregisterHotKey ALT+X"));
            if (!RegisterHotKey(hwnd, IDM_ACTIVATE, MOD_ALT, '1'+iDesktopNumber))
                ErrorExit(_T("RegisterHotKey ALT+X"));
            if (!UnregisterHotKey(hwnd, IDM_ADD))
                ErrorExit(_T("UnregisterHotKey CTRL+X"));
            if (!RegisterHotKey(hwnd, IDM_ADD, MOD_CONTROL, '1'+iDesktopNumber))
                ErrorExit(_T("RegisterHotKey CTRL+X"));
            if (iDesktopActivated != (LONG)wParam+1)
            {
                ChangeIcon(hwnd, TRAY_ICON_ID, MAKEINTRESOURCE(IDI_ICON1+iDesktopNumber));
            }
            else
            {
                UnRegisterHotKeys(hwnd);
            }
            PostMessage(HWND_BROADCAST, uDestroyMsg, wParam+1, (LPARAM) NULL);
        }
        if (wParam == iDesktopActivated)
        {
            LONG iActive = (LONG) wParam - 1;
            iDesktopActivated = -1;
            if (iActive < 0)
            {
                iActive = 0;
            }
            if (iDesktopNumber == iActive)
            {
                LONG i = 0;
                PostMessage(HWND_BROADCAST, uActivationMsg, iDesktopNumber, (LPARAM) NULL);
                // Show
                for (i=0; i<gWndDataList->wndDataSize; ++i)
                {
                    if (gWndDataList->wndData[i].bShown == TRUE)
                    {
                        DisplayWindow(i);
                    }
                }
                SetForegroundWindow(gWndDataList->fgWnd);
                RegisterHotKeys(hwnd);
                ChangeIcon(hwnd, TRAY_ICON_ID, MAKEINTRESOURCE(IDI_ENABLE_ICON1+iDesktopNumber));
            }
        }
        return 0;
    }

    if (message == uMoveMsg)
    {
        if (iDesktopActivated == iDesktopNumber)
        {
            LONG i = 0;
            UpdateDataList();
            for (i=0; i<gWndDataList->wndDataSize; ++i)
            {
                if (gWndDataList->wndData[i].hWnd == GetForegroundWindow())
                {
                    HideWindow(i);
                    break;
                }
            }
        }
        return 0;
    }
   	
	switch (message)
	{
             
        case WM_TRAY_ICON:
             if (lParam==WM_RBUTTONUP)// ou lParam==WM_RBUTTONDBLCLK)	
             {
                GetCursorPos(&point);
                SetForegroundWindow(hwnd);
                UpdateMenu(GetSubMenu(hMenu,0));
                TrackPopupMenu (GetSubMenu(hMenu,0), TPM_RIGHTBUTTON, point.x, point.y, 0, hwnd, NULL) ;
             }
             else if (lParam==WM_LBUTTONUP)
             {
                 PostMessage(hwnd, WM_COMMAND, IDM_ACTIVATE, (LPARAM) NULL);
             }
             return 0;
		
        case WM_CREATE:
            hMenu = LoadMenu(ghInst, MAKEINTRESOURCE(IDR_MENU)); 
            if (!RegisterHotKey(hwnd, IDM_ACTIVATE, MOD_ALT, '1'+iDesktopNumber))
                ErrorExit(_T("RegisterHotKey ALT+X"));
            if (!RegisterHotKey(hwnd, IDM_ADD, MOD_CONTROL, '1'+iDesktopNumber))
                ErrorExit(_T("RegisterHotKey CTRL+X"));
            AddIcon(hwnd, TRAY_ICON_ID, MAKEINTRESOURCE(IDI_ENABLE_ICON1+iDesktopNumber), szTooltip);
            if (iDesktopNumber == 0)
            {
                RegisterHotKeys(hwnd);
                iDesktopActivated = 0;
            }
            else
            {
                PostMessage(HWND_BROADCAST, uActivationMsg, iDesktopNumber, (LPARAM) NULL);
            }
			return 0;
		
        case WM_DESTROY:
            {
                LONG i = 0;
                if (!UnregisterHotKey(hwnd, IDM_ACTIVATE))
                    ErrorExit(_T("RegisterHotKey ALT+X"));
                if (!UnregisterHotKey(hwnd, IDM_ADD))
                    ErrorExit(_T("UnregisterHotKey CTRL+X"));
                if (iDesktopActivated == iDesktopNumber)
                {
                    UnRegisterHotKeys(hwnd);
                }
                for (i=0; i<gWndDataList->wndDataSize; ++i)
                {
                    DisplayWindow(i);
                }
                RetIcon(hwnd, TRAY_ICON_ID);
                PostMessage(HWND_BROADCAST, uDestroyMsg, iDesktopNumber, (LPARAM) NULL);
			    PostQuitMessage (0);
			    DestroyMenu(hMenu);
			    return 0;
            }
		
        case WM_HOTKEY:
		case WM_COMMAND:
		 switch (LOWORD (wParam))
          {
			case IDM_QUIT:
                PostMessage(hwnd, WM_DESTROY, (WPARAM) NULL, (LPARAM) NULL);
                return 0;

			case IDM_ALL:
                {
                    LONG i = 0;
                    UpdateDataList();
                    for (i=0; i<gWndDataList->wndDataSize; ++i)
                    {
                        if (gWndDataList->wndData[i].bShown == TRUE)
                        {
                            HideWindow(i);
                            gWndDataList->wndData[i].bShown = FALSE;
                        }
                    }
                    return 0;
                }

			case IDM_NONE:
                {
                    LONG i = 0;
                    UpdateDataList();
                    for (i=0; i<gWndDataList->wndDataSize; ++i)
                    {
                        if (gWndDataList->wndData[i].bShown == FALSE)
                        {
                            DisplayWindow(i);
                            gWndDataList->wndData[i].bShown = TRUE;
                        }
                    }
                    SetForegroundWindow(gWndDataList->fgWnd);
                    return 0;
                }

			case IDM_INVERT:
                {
                    LONG i = 0;
                    UpdateDataList();
                    for (i=0; i<gWndDataList->wndDataSize; ++i)
                    {
                        if (gWndDataList->wndData[i].bShown == TRUE)
                        {
                            HideWindow(i);
                            gWndDataList->wndData[i].bShown = FALSE;
                        }
                        else
                        {
                            DisplayWindow(i);
                            gWndDataList->wndData[i].bShown = TRUE;
                        }
                    }
                    return 0;
                }

			case IDM_TAB:
                {
                    LONG i = 0;
                    LONG fg = 0;
                    UpdateDataList();
                    for (i=0; i<gWndDataList->wndDataSize; ++i)
                    {
                        if (gWndDataList->wndData[i].bShown == TRUE)
                        {
                            HideWindow(i);
                            gWndDataList->wndData[i].bShown = FALSE;
                            fg = (i+1==gWndDataList->wndDataSize?0:i+1);
                        }
                    }
                    DisplayWindow(fg);
                    SetForegroundWindow(gWndDataList->wndData[fg].hWnd);
                    gWndDataList->wndData[fg].bShown = TRUE;
                    return 0;
                }

            case IDM_ACTIVATE:
                {
                    if (iDesktopActivated != iDesktopNumber)
                    {
                        PostMessage(HWND_BROADCAST, uActivationMsg, iDesktopNumber, (LPARAM) NULL);
                    }
                    return 0;
                }

            case IDM_ADD:
                {
                    if (iDesktopActivated != iDesktopNumber)
                    {
                        TCHAR szName[128];
                        HWND fg = GetForegroundWindow();
                        BOOL bShown = IsWindowVisible(fg);
                        BOOL bIconic = IsIconic(fg);
                        GetWindowText(fg, szName, 128);
                        gWndDataList->wndData[gWndDataList->wndDataSize].hWnd = fg;
                        _tcscpy_s(gWndDataList->wndData[gWndDataList->wndDataSize].lpszName, 128, szName);
                        gWndDataList->wndData[gWndDataList->wndDataSize].bShown  = bShown;
                        gWndDataList->wndData[gWndDataList->wndDataSize].bIconic = bIconic;
                        gWndDataList->wndDataSize++;
                        PostMessage(HWND_BROADCAST, uMoveMsg, iDesktopNumber, (LPARAM) NULL);
                    }
                    return 0;
                }

            case IDM_RIGHT:
                {
                    RECT rect;
                    HWND fg = GetForegroundWindow();
                    if (GetWindowRect(fg, &rect) == TRUE)
                    {
                        LONG iSlidePixel = GetSlidePixel();
                        MoveWindow(fg, rect.left+iSlidePixel, rect.top, rect.right - rect.left, rect.bottom - rect.top, TRUE); 
                    }
                    return 0;
                }

            case IDM_LEFT:
                {
                    RECT rect;
                    HWND fg = GetForegroundWindow();
                    if (GetWindowRect(fg, &rect) == TRUE)
                    {
                        LONG iSlidePixel = GetSlidePixel();
                        MoveWindow(fg, rect.left-iSlidePixel, rect.top, rect.right - rect.left, rect.bottom - rect.top, TRUE);
                    }
                    return 0;
                }

            case IDM_UP:
                {
                    RECT rect;
                    HWND fg = GetForegroundWindow();
                    if (GetWindowRect(fg, &rect) == TRUE)
                    {
                        LONG iSlidePixel = GetSlidePixel();
                        MoveWindow(fg, rect.left, rect.top-iSlidePixel, rect.right - rect.left, rect.bottom - rect.top, TRUE);
                    }
                    return 0;
                }

            case IDM_DOWN:
                {
                    RECT rect;
                    HWND fg = GetForegroundWindow();
                    if (GetWindowRect(fg, &rect) == TRUE)
                    {
                        LONG iSlidePixel = GetSlidePixel();
                        MoveWindow(fg, rect.left, rect.top+iSlidePixel, rect.right - rect.left, rect.bottom - rect.top, TRUE);
                    }
                    return 0;
                }

			default:
                {
                    LONG i = (LONG) wParam-IDM_APP;
                    if (i >= 0 &&
                        i < gWndDataList->wndDataSize)
                    {
                        if (gWndDataList->wndData[i].bShown == FALSE)
                        {
                            DisplayWindow(i);
                            gWndDataList->wndData[i].bShown = TRUE;
                        }
                        else
                        {
                            HideWindow(i);
                            gWndDataList->wndData[i].bShown = FALSE;
                        }
                    }
                    return 0;
                }
		 }
	}
	return DefWindowProc (hwnd,message,wParam,lParam);
}

BOOL CompareProcessName(DWORD processID, LPCTSTR lpszCurrentProcessName)
{
    BOOL bEqual = FALSE;
    TCHAR szProcessName[MAX_PATH] = TEXT("<unknown>");

    HANDLE hProcess = OpenProcess( PROCESS_QUERY_INFORMATION |
                                   PROCESS_VM_READ,
                                   FALSE, processID );
    if (NULL != hProcess )
    {
        HMODULE hMod;
        DWORD cbNeeded;

        if ( EnumProcessModules( hProcess, &hMod, sizeof(hMod), 
             &cbNeeded) )
        {
            GetModuleBaseName( hProcess, hMod, szProcessName, 
                               sizeof(szProcessName)/sizeof(TCHAR) );
        }
    }
    bEqual = _tcscmp(szProcessName, lpszCurrentProcessName)==0;
    CloseHandle( hProcess );
    return bEqual;
}

VOID DetermineDesktopNumber()
{
    DWORD aProcesses[1024], cbNeeded, cProcesses;
    unsigned int i;

    TCHAR szCurrentProcessName[MAX_PATH] = TEXT("<unknown>");
    GetModuleBaseName(GetCurrentProcess(), NULL, szCurrentProcessName, 
                  sizeof(szCurrentProcessName)/sizeof(TCHAR) );

    EnumProcesses( aProcesses, sizeof(aProcesses), &cbNeeded );
    cProcesses = cbNeeded / sizeof(DWORD);
    for ( i = 0; i < cProcesses; i++ )
    {
        if( aProcesses[i] != 0 &&
            CompareProcessName(aProcesses[i], szCurrentProcessName) == TRUE)
        {
            iDesktopNumber++;
        }
    }
}


int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance,
				   LPSTR lpCmdLine, int nCmdShow )
{
	MSG msg;
	WNDCLASS wc;
	HWND hwnd;
    LPCTSTR szClassName = _T("WinHideApplication");
	
	ghInst = hInstance;
    gWndDataList = CreateWindowDataList();
    iDesktopNumber = -1;
    iDesktopActivated = -1;

    DetermineDesktopNumber();
    if (iDesktopNumber >= 4)
    {
        MessageBox(NULL, TEXT("Maximum desktop number is reached."), TEXT("Error"), MB_OK); 
        ExitProcess(0); 
    }

	wc.style = CS_HREDRAW|CS_VREDRAW;
	wc.lpfnWndProc = WindowProc;
	wc.cbClsExtra = 0;
	wc.cbWndExtra = 0;
	wc.hInstance = hInstance;
	wc.hIcon = LoadIcon (NULL, IDI_APPLICATION);
	wc.hCursor = LoadCursor (NULL, IDC_ARROW);
	wc.hbrBackground = (HBRUSH) (COLOR_WINDOW+1);
	wc.lpszMenuName = NULL;
	wc.lpszClassName = szClassName;

	RegisterClass (&wc);

    uActivationMsg = RegisterWindowMessage(szActivationMsg);
    uUnActivationMsg = RegisterWindowMessage(szUnActivationMsg);
    uDestroyMsg = RegisterWindowMessage(szDestroyMsg);
    uMoveMsg = RegisterWindowMessage(szMoveMsg);

	hwnd = CreateWindow(szClassName,_T("WinHide"),WS_POPUP,
		CW_USEDEFAULT,CW_USEDEFAULT,CW_USEDEFAULT,CW_USEDEFAULT,
		NULL,NULL,hInstance,NULL);
    
    hSlideTimer = CreateWaitableTimer(NULL, TRUE, NULL);
	//ShowWindow (hwnd,nCmdShow);
	//UpdateWindow (hwnd);
	    
    while(GetMessage(&msg, NULL, 0, 0) > 0)
    {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    ReleaseWindowDataList(gWndDataList);
	return (int) msg.wParam;
}


