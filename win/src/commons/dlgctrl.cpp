/* dlgctrl.cpp - interface for easing creation and use of common dialog controls

    This file is part of the tagger-ui suite <http://www.github.com/cedricfrancoys/tagger-ui>
    Copyright (C) Cedric Francoys, 2016, Yegen
    Some Right Reserved, GNU GPL 3 license <http://www.gnu.org/licenses/>
*/

#include <windows.h>
#include <commctrl.h>
#include <uxtheme.h>

#include "dlgctrl.h"

/* Generic callback for TCN_SELCHANGE events.
Bring to top child window associated with newly selected tab. 
This function expects given Window objet to have TAB_CONTROL and TAB_PANES properties set (@see function DlgCtrl_InitTabs).
Children windows (tab panes) must have WS_CLIPSIBLINGS style set.
*/
void DlgCtrl_EvChangeTab(HWND hWnd, WPARAM, LPARAM) {
	HWND hTabControl = (HWND) GetProp(hWnd, L"TAB_CONTROL");
    if(!hTabControl) return;
	int selectedTab = TabCtrl_GetCurSel(hTabControl);
	HWND* dlgTabPanes = (HWND*) GetProp(hWnd, L"TAB_PANES");
	if(dlgTabPanes) {
		HWND hNewPane = dlgTabPanes[selectedTab];
		if(!IsWindowVisible(hNewPane)) {
			ShowWindow(hNewPane, SW_SHOW);
		}
		BringWindowToTop(hNewPane);
		UpdateWindow(hNewPane);
	}
}

/* Create tabControl tabs and populate it with custom panes.
This function expects the given Window object to be a dialog defined in a .rc file having the following features
 - dialog must own a SysTabControl32 with ID nIDTabCtrl
 - dialogs must be defined for each pane, having consecutive ID, starting with nIDFirstTab (ex. IDD_PANE_ONE DIALOGEX ...)
 - for each dialog ID, a string with same ID must be defined 
	(ex. :
		STRINGTABLE 
		BEGIN
		 IDD_PANE_ONE, "Filesystem activity"
		 IDD_PANE_TWO, "Tagger logs"
		END
	)
*/
void DlgCtrl_InitTabs(HWND hWnd, UINT nIDTabCtrl, UINT nPanesCount, UINT nIDFirstTab, DLGPROC lpDialogFunc) {
	SetProp(hWnd, L"TAB_PANES_COUNT", (HANDLE) (UINT) nPanesCount);	
	if(nPanesCount) {
		HWND hTabControl = GetDlgItem(hWnd, nIDTabCtrl);
        SetProp(hWnd, L"TAB_CONTROL", (HANDLE) (HWND) hTabControl);
		HWND* hTabPanes = (HWND*) GlobalAlloc(GPTR, sizeof(HWND) * nPanesCount);
		SetProp(hWnd, L"TAB_PANES", (HANDLE) (HWND*) hTabPanes);
		TCITEM TabCtrlItem;
		WCHAR szText[100];
		TabCtrlItem.mask = TCIF_TEXT;
		TabCtrlItem.pszText = szText;
		for(UINT i = 0; i < nPanesCount; ++i) {
			hTabPanes[i] = CreateDialogParam(GetModuleHandle(NULL), MAKEINTRESOURCE(nIDFirstTab+i), hTabControl, lpDialogFunc, 0);
			LoadString(GetModuleHandle(NULL), nIDFirstTab+i, szText, sizeof(szText));
			SendMessage(GetDlgItem( hWnd, nIDTabCtrl ), TCM_INSERTITEM, i, (LPARAM) &TabCtrlItem);
			EnableThemeDialogTexture(hTabPanes[i], ETDT_ENABLETAB);
		}
	}
}



UINT DlgCtrl_CountChildren(HWND hWnd) {
	UINT i = 0;
	HWND hCurWnd = GetWindow(hWnd, GW_CHILD);
	while(hCurWnd) {
		++i;
		hCurWnd = GetNextWindow(hCurWnd, GW_HWNDNEXT);
	}
	return i;
}

BOOL CALLBACK EnumChildProc(HWND hWnd, LPARAM lParam) {
	MSG* msg = (MSG*) lParam;
	SendMessage(hWnd, msg->message, msg->wParam, msg->lParam);
	EnumChildWindows(hWnd, EnumChildProc, (LPARAM) (MSG*) msg);
	return TRUE;
}

BOOL CALLBACK EnumCtrlProc(HWND hWnd, LPARAM lParam) {
	MSG* msg = (MSG*) lParam;
	// memo: msg.time is used for storing the control ID
	if(msg->hwnd) {
		if(msg->time == (DWORD) GetWindowLong(hWnd, GWL_ID)) {
			// if we reached the targeted control, deliver the message and stop the loop
			SendMessage(hWnd, msg->message, msg->wParam, msg->lParam);
			msg->hwnd = NULL;
			return FALSE;
		}
		EnumChildWindows(hWnd, EnumCtrlProc, (LPARAM) (MSG*) msg);
	}	
	return TRUE;
}

/* Send a message to all descendant windows (top-down broadcast).

This function walks recursively through all children windows and descendants
and sends specified message to each of them.

Note: Some windows might receive the message several times.
*/
void DlgCtrl_SendChildrenMessage(HWND hWnd, UINT Msg, WPARAM wParam, LPARAM lParam) {
	MSG msg;
	msg.message = Msg;
	msg.wParam = wParam;
	msg.lParam = lParam;
	EnumChildWindows(hWnd, EnumChildProc, (LPARAM) (MSG*) &msg);
}

/* Send a message to first descendant control of given ID.

This function performs a recursive search of the first control matching given ID
among children windows and descendants, and sends it specified message.

This is a convenient replacement for SendDlgItemMessage as long as user takes care of:
 - giving unique ID to all controls
 - maintaining consistency in windows hierarchy
*/
/*
void DlgCtrl_SendMessage(HWND hWnd, int nIDDlgItem, UINT Msg, WPARAM wParam, LPARAM lParam) {
	MSG msg;
	// recursion stops when msg.hwnd is set to NULL
   	msg.hwnd = hWnd;
	msg.message = Msg;
	msg.wParam = wParam;
	msg.lParam = lParam;
	// msg.time is used for storing the control ID
	msg.time = (DWORD) nIDDlgItem;
	EnumChildWindows(hWnd, EnumCtrlProc, (LPARAM) (MSG*) &msg);
}
*/
int DlgCtrl_SendMessage(HWND hWnd, int nIDDlgItem, UINT Msg, WPARAM wParam, LPARAM lParam) {
	HWND hChild = FindWindowEx(hWnd, NULL, NULL, NULL);
	while(hChild){
		if(nIDDlgItem == (DWORD) GetWindowLong(hChild, GWL_ID)) {
			SendMessage(hChild, Msg, wParam, lParam);
			return 1;
		}
		if(DlgCtrl_SendMessage(hChild, nIDDlgItem, Msg, wParam, lParam)) {
			return 1;
		}
		hChild = FindWindowEx(hWnd, hChild, NULL, NULL);
	}
	return 0;
}