#include <Windows.h>
#include <CommCtrl.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "taggerui.h" 
#include "eventlistener.h" 
#include "doscommand.h" 

#pragma comment(linker, \
  "\"/manifestdependency:type='Win32' "\
  "name='Microsoft.Windows.Common-Controls' "\
  "version='6.0.0.0' "\
  "processorArchitecture='*' "\
  "publicKeyToken='6595b64144ccf1df' "\
  "language='*'\"")

#pragma comment(lib, "ComCtl32.lib")



// Global variables
WCHAR* taggerCommandLinePath = NULL;
WCHAR* installDirectory = NULL;

int getEnv() {
	HKEY  hKey;
	RegOpenKeyEx(
		HKEY_LOCAL_MACHINE,	
		L"SOFTWARE\\TaggerUI",	
		0,	
		KEY_READ,	
		&hKey
	);

	DWORD type;
	DWORD size = 1024;
	LPWSTR data = (LPWSTR) LocalAlloc(LPTR, sizeof(WCHAR)*size);

	if( RegQueryValueEx(
		hKey,
		L"Tagger_Dir",
		0,
		&type,
		(LPBYTE) data,
		&size) != ERROR_SUCCESS ) {
		return 0;
	}
	taggerCommandLinePath = (LPWSTR) LocalAlloc(LPTR, sizeof(WCHAR) * (wcslen(data)+wcslen(L"\\tagger.exe --quiet")+1) );
	wsprintf(taggerCommandLinePath, L"%s\\tagger.exe --quiet", data);

	// reset size to a suffisant value
	size = 1024;
	if( RegQueryValueEx(
		hKey,
		L"Install_Dir",
		0,
		&type,
		(LPBYTE) data,
		&size) != ERROR_SUCCESS ) {
		return 0;
	}
	installDirectory = (LPWSTR) LocalAlloc(LPTR, sizeof(WCHAR) * (wcslen(data)+1) );
	wsprintf(installDirectory, L"%s", data);

	// close key
	LocalFree(data);

	return 1;
}


// Forward declarations of functions included in this module

// functions that will be binded to the event listener
void initDialog(HWND,WPARAM,LPARAM);
void searchFiles(HWND,WPARAM,LPARAM);
void updateTagName(HWND,WPARAM,LPARAM);
void selectTagName(HWND,WPARAM,LPARAM);
void closeDialog(HWND,WPARAM,LPARAM);
void showContext(HWND,WPARAM,LPARAM);

// context menu handlers
void menuOpenFile(HWND,WPARAM,LPARAM);
void menuTagFile(HWND,WPARAM,LPARAM);
void menuExplore(HWND,WPARAM,LPARAM);
void menuCopyPath(HWND,WPARAM,LPARAM);


int WINAPI WinMain(HINSTANCE hInstance,	HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {    	

	INITCOMMONCONTROLSEX InitCtrlEx;
	InitCtrlEx.dwSize = sizeof(INITCOMMONCONTROLSEX);
	InitCtrlEx.dwICC  = ICC_BAR_CLASSES;
	InitCommonControlsEx(&InitCtrlEx);

	HWND hWnd;    
	if (!(hWnd = CreateDialogParam(hInstance, MAKEINTRESOURCE(IDD_DIALOG), 0, (DLGPROC) EventListener::dlgProc, 0))) {
        MessageBox(NULL, L"App creation failed!", L"Tagger", NULL);
        return 1;
    }

	EventListener* eventListener = EventListener::getInstance(HWND_DIALOG);
	// since WM_INITDIALOG has been sent before binding, we need to initialize dialog manually
	initDialog(hWnd, (WPARAM) NULL, (LPARAM) NULL);
	// bind events we're interested in with appropriate handling functions
	// global events
	eventListener->bind(hWnd, 0, WM_CLOSE, closeDialog);
	// controls events
	eventListener->bind(hWnd, ID_TAGNAME, EN_CHANGE, updateTagName);
	eventListener->bind(hWnd, ID_LIST_TAGS_AVAIL, LBN_SELCHANGE, selectTagName);
	// override default IDOK behavior
	eventListener->bind(hWnd, IDOK, BN_CLICKED, searchFiles);
	// context menu events
	eventListener->bind(hWnd, ID_LIST_FILES, WM_CONTEXTMENU, showContext);
	eventListener->bind(hWnd, IDM_OPEN, 0, menuOpenFile);
	eventListener->bind(hWnd, IDM_EXPLORE, 0, menuExplore);
	eventListener->bind(hWnd, IDM_COPYPATH, 0, menuCopyPath);
	eventListener->bind(hWnd, IDM_TAG, 0, menuTagFile);

    ShowWindow(hWnd, SW_SHOW);
    UpdateWindow(hWnd);
	SetForegroundWindow(hWnd);

    // Main message loop:
	MSG msg;
    while (GetMessage(&msg, NULL, 0, 0) > 0) {
		if(!IsDialogMessage(hWnd, &msg)) {
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}
    }

    return (int) msg.wParam;
}


void initDialog(HWND hWnd, WPARAM wParam, LPARAM lParam) {
	// set icon
	HICON hIcon;
	if((hIcon = (HICON) LoadIcon(GetModuleHandle(NULL), MAKEINTRESOURCE(IDD_ICON)))) {
		SendMessage(hWnd, WM_SETICON, ICON_BIG, (LPARAM) hIcon); 
	}

	// hide global tags list
	ShowWindow( GetDlgItem( hWnd, ID_LIST_TAGS_ALL ), SW_HIDE);
	// hide available tags list
	ShowWindow( GetDlgItem( hWnd, ID_LIST_TAGS_AVAIL ), SW_HIDE);
	// disable files list
	EnableWindow( GetDlgItem( hWnd, ID_LIST_FILES ), FALSE);	
	SetFocus( GetDlgItem( hWnd, ID_TAGNAME ) );

	// try to retrieve list of available tags
	if(!getEnv()) {
		MessageBox(NULL, L"Unable to locate installation directory.\nTo solve this, try re-installing the application.", L"Error", MB_OK|MB_ICONERROR);
	}
	else {

		// retrieve all existing tags 
		LPWSTR command, output;	
		command = (LPWSTR) LocalAlloc(LPTR, sizeof(WCHAR) * (wcslen(taggerCommandLinePath)+wcslen(L" tags")+1) );
		swprintf(command, L"%s tags", taggerCommandLinePath);
		output = DosExec(command);
		for(LPWSTR line = wcstok(output, L"\n"); line; line = wcstok(NULL, L"\n")) {
			line[wcslen(line)-1] = '\0';
			SendDlgItemMessage(hWnd, ID_LIST_TAGS_ALL, LB_ADDSTRING, 0, (LPARAM) line);
		}
		LocalFree(command);
		LocalFree(output);
	}
}


/* Search for files matching given pattern.
*/
void searchFiles(HWND hWnd, WPARAM wParam, LPARAM lParam) {
	// MessageBox(hWnd, L"searchFiles", L"info", MB_OK);

	// hide and disable list of available tags
	ShowWindow( GetDlgItem( hWnd, ID_LIST_TAGS_AVAIL ), SW_HIDE);
	EnableWindow( GetDlgItem( hWnd, ID_LIST_TAGS_AVAIL ), FALSE);

	// enable files list
	EnableWindow( GetDlgItem( hWnd, ID_LIST_FILES ), TRUE);

	// retrieve input pattern
	int len = SendDlgItemMessage(hWnd, ID_TAGNAME, WM_GETTEXTLENGTH, 0, 0);
	LPWSTR pattern = (LPWSTR) LocalAlloc(LPTR, sizeof(WCHAR)*(len+1));
	GetDlgItemText(hWnd, ID_TAGNAME, (LPWSTR) pattern, len+1);

	// empty files list
	SendDlgItemMessage(hWnd, ID_LIST_FILES, LB_RESETCONTENT, 0, 0);

	// populate files list
	LPWSTR command, output;
	// retrieve matching files 
	command = (LPWSTR) LocalAlloc(LPTR, sizeof(WCHAR) * (wcslen(taggerCommandLinePath)+wcslen(L" files \"\"")+wcslen(pattern)+1) );
	swprintf(command, L"%s files \"%s\"", taggerCommandLinePath, pattern);

	output = DosExec(command);

	for(LPWSTR line = wcstok(output, L"\n"); line; line = wcstok(NULL, L"\n")) {
		line[wcslen(line)-1] = '\0';
		SendDlgItemMessage(hWnd, ID_LIST_FILES, LB_ADDSTRING, 0, (LPARAM) line);
	}

	LocalFree(command);
	LocalFree(output);
	LocalFree(pattern);
}


void selectTagName(HWND hWnd, WPARAM wParam, LPARAM lParam) {
	// MessageBox(hWnd, L"selectTagName", L"info", MB_OK);

	// retrieve selection value
	int index = SendDlgItemMessage(hWnd, ID_LIST_TAGS_AVAIL, LB_GETCURSEL, 0, (LPARAM) 0);
	if(index != LB_ERR) {
		int len = SendDlgItemMessage(hWnd, ID_LIST_TAGS_AVAIL, LB_GETTEXTLEN, (WPARAM) index, (LPARAM) 0);
		LPWSTR str = (LPWSTR) LocalAlloc(LPTR, sizeof(WCHAR)*(len+1));
		SendDlgItemMessage(hWnd, ID_LIST_TAGS_AVAIL, LB_GETTEXT, (WPARAM) index, (LPARAM) str);

		EventListener* eventListener = EventListener::getInstance(HWND_DIALOG);	
		// prevent EN_CHANGE handling for ID_TAGNAME
		eventListener->unbind(hWnd, ID_TAGNAME, EN_CHANGE);
		// copy selection to ID_TAGNAME
		SendDlgItemMessage(hWnd, ID_TAGNAME, WM_SETTEXT, (WPARAM) 0, (LPARAM) (LPWSTR) str);
		// force files list update
		searchFiles(hWnd, wParam, lParam);
		// restore event handler for ID_TAGNAME
		eventListener->bind(hWnd, ID_TAGNAME, EN_CHANGE, updateTagName);
	}
	// SetFocus( GetDlgItem( hWnd, ID_TAGNAME ) );	
}

/* Limit the list of available tags to those which name match the current pattern
*/
void updateTagName(HWND hWnd, WPARAM wParam, LPARAM lParam) {
	// MessageBox(hWnd, L"updateTagName", L"info", MB_OK);

	// reset and hide available list
	SendDlgItemMessage(hWnd, ID_LIST_TAGS_AVAIL, LB_RESETCONTENT, 0, 0);
	ShowWindow( GetDlgItem( hWnd, ID_LIST_TAGS_AVAIL ), SW_HIDE);
	EnableWindow( GetDlgItem( hWnd, ID_LIST_TAGS_AVAIL ), FALSE);

	// retrieve input pattern
	int len = SendDlgItemMessage(hWnd, ID_TAGNAME, WM_GETTEXTLENGTH, 0, 0);
	LPWSTR pattern = (LPWSTR) LocalAlloc(LPTR, sizeof(WCHAR)*(len+1));
	GetDlgItemText(hWnd, ID_TAGNAME, (LPWSTR) pattern, len+1);

	// retrieve global list count
	LPWSTR str = (LPWSTR) LocalAlloc(LPTR, sizeof(WCHAR)*1024);
	int n = 0, count = SendDlgItemMessage(hWnd, ID_LIST_TAGS_ALL, LB_GETCOUNT, 0, 0);

	// add only tags matching input pattern
	for(int i = 0; i < count; ++i) {
		SendDlgItemMessage(hWnd, ID_LIST_TAGS_ALL, LB_GETTEXT, (WPARAM) i, (LPARAM) str);
		if(wcsstr(str, pattern) == str || wcslen(pattern) == 0) {
			int index = SendDlgItemMessage(hWnd, ID_LIST_TAGS_AVAIL, LB_ADDSTRING, 0, (LPARAM) str);
			if(wcscmp(str, pattern) == 0) {
				// select full match item if any
				// SendDlgItemMessage(hWnd, ID_LIST_TAGS_AVAIL, LB_SELITEMRANGE, TRUE, (LPARAM) MAKELPARAM(index, index));				
			}
			++n;
		}
	}

	// if some match were found, show available tags
	if(n > 0) { 
		// disable files list
		EnableWindow( GetDlgItem( hWnd, ID_LIST_FILES ), FALSE);	
		// show list of available tags
		ShowWindow( GetDlgItem( hWnd, ID_LIST_TAGS_AVAIL ), SW_SHOW);
		EnableWindow( GetDlgItem( hWnd, ID_LIST_TAGS_AVAIL ), TRUE);
	}

	LocalFree(str);
	LocalFree(pattern);
}

void showContext(HWND hWnd, WPARAM wParam, LPARAM lParam) {
	// MessageBox(NULL, L"showContext", L"Notice", MB_OK);

	// extract coordinates
	UINT xPos = LOWORD(lParam); 
	UINT yPos = HIWORD(lParam);

	// check cursor position relatively to ID_LIST_FILES control
	RECT rect;
	GetWindowRect(GetDlgItem(hWnd, ID_LIST_FILES), &rect);
	int res = SendDlgItemMessage(hWnd, ID_LIST_FILES, LB_ITEMFROMPOINT, 0, (LPARAM) MAKELPARAM(xPos-rect.left, yPos-rect.top));
	// if we're not outside of the client-area
	if(!HIWORD(res)) {
		// select item at cursor position
		SendDlgItemMessage(hWnd, ID_LIST_FILES, LB_SETCURSEL, (WPARAM) LOWORD(res), (LPARAM) 0);  
		// generate popup menu from resource
		HMENU hMenu = GetSubMenu( LoadMenu(GetModuleHandle(NULL), MAKEINTRESOURCE(ID_POPUP_MENU)), 0);
		TrackPopupMenu(hMenu, TPM_TOPALIGN | TPM_LEFTALIGN, xPos, yPos, 0, hWnd, NULL); 
	}
}

void closeDialog(HWND hWnd, WPARAM wParam, LPARAM lParam) {
	if(taggerCommandLinePath != NULL) LocalFree(taggerCommandLinePath);
	if(installDirectory != NULL) LocalFree(installDirectory);
	DestroyWindow(hWnd);
	PostQuitMessage(0);
}

LPWSTR getSelectedFile() {
	HWND hWnd = GetActiveWindow();
	int index = SendDlgItemMessage(hWnd, ID_LIST_FILES, LB_GETCURSEL, (WPARAM) 0, (LPARAM) 0);
	int len = SendDlgItemMessage(hWnd, ID_LIST_FILES, LB_GETTEXTLEN, (WPARAM) index, (LPARAM) 0);
	LPWSTR path = (LPWSTR) LocalAlloc(LPTR, sizeof(WCHAR)*(len+1));
	SendDlgItemMessage(hWnd, ID_LIST_FILES, LB_GETTEXT, (WPARAM) index, (LPARAM) path);
	return path;
}

void menuOpenFile(HWND hWnd, WPARAM wParam, LPARAM lParam) {
	// retrieve selected file
	LPWSTR path = getSelectedFile();
// ?? don't know how to easily retrieve default shell menu
	// open using explorer
	ShellExecute(NULL, L"open", path, NULL, NULL, SW_SHOWNOACTIVATE	);
	LocalFree(path);
}

void menuTagFile(HWND hWnd, WPARAM wParam, LPARAM lParam) {
	// retrieve selected file
	LPWSTR path = getSelectedFile();
	// open using explorer
	LPWSTR command = (LPWSTR) LocalAlloc(LPTR, sizeof(WCHAR)*(wcslen(installDirectory)+wcslen(L"\\tftag.exe")+wcslen(L" \"\"")+wcslen(path)+1) );
	wsprintf(command, L"%s\\tftag.exe \"%s\"", installDirectory, path);
	WinExec(UNICODEtoANSI(command), SW_SHOWNORMAL);
	LocalFree(command);
	LocalFree(path);
}

void menuExplore(HWND hWnd, WPARAM wParam, LPARAM lParam) {
	// retrieve selected file
	LPWSTR path = getSelectedFile();
	// get parent directory: remove everything after last backslash
	LPWSTR ptr = wcsrchr(path, '\\');
	if(ptr) *ptr = 0;

	// launch explorer
	ShellExecute(NULL, L"explore", path, NULL, NULL, SW_SHOWMAXIMIZED);
	LocalFree(path);
}

void menuCopyPath(HWND hWnd, WPARAM wParam, LPARAM lParam) {
	// retrieve selected file
	LPWSTR path = getSelectedFile();
	int len = wcslen(path);
	// try to access clipboard
	if (!IsClipboardFormatAvailable(CF_UNICODETEXT)) return; 
	if (!OpenClipboard(hWnd)) return;  
	EmptyClipboard(); 
	// copy file path to clipboard
	HGLOBAL hGlb = GlobalAlloc(GMEM_DDESHARE, sizeof(WCHAR)*(len+1) );
    if (hGlb != NULL) {
        LPWSTR lptstrCopy = (LPWSTR) GlobalLock(hGlb); 
		memcpy(lptstrCopy, path, sizeof(WCHAR) * (len+1)); 
        GlobalUnlock(hGlb); 		
		SetClipboardData(CF_UNICODETEXT, hGlb);
		GlobalFree(hGlb);
	}   
    CloseClipboard(); 	
	LocalFree(path);
}