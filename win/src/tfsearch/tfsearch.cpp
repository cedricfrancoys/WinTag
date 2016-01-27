/* tfsearch.cpp - win32 application for searching files matching tags criteria.

    This file is part of the tagger-ui suite <http://www.github.com/cedricfrancoys/tagger-ui>
    Copyright (C) Cedric Francoys, 2016, Yegen
    Some Right Reserved, GNU GPL 3 license <http://www.gnu.org/licenses/>
*/

#include <windows.h>
#include <commctrl.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "tfsearch.h" 
#include "../commons/eventlistener.h" 
#include "../commons/dosexec.h" 

#pragma comment(linker, \
  "\"/manifestdependency:type='Win32' "\
  "name='Microsoft.Windows.Common-Controls' "\
  "version='6.0.0.0' "\
  "processorArchitecture='*' "\
  "publicKeyToken='6595b64144ccf1df' "\
  "language='*'\"")

#pragma comment(lib, "ComCtl32.lib")

#define FILE_NAME_MAX 1024

// Global variables
WCHAR* taggerCommandLinePath = NULL;
WCHAR* installDirectory = NULL;

int getEnv() {
	HKEY  hKey;
	RegOpenKeyEx(HKEY_LOCAL_MACHINE, L"SOFTWARE\\TaggerUI",	0, KEY_READ, &hKey);

	DWORD type, size = FILE_NAME_MAX;
	LPWSTR data = (LPWSTR) LocalAlloc(LPTR, sizeof(WCHAR)*size);

	if( RegQueryValueEx(hKey, L"Tagger_Dir", 0, &type, (LPBYTE) data, &size) != ERROR_SUCCESS ) return 0;
	taggerCommandLinePath = (LPWSTR) LocalAlloc(LPTR, sizeof(WCHAR) * (wcslen(data)+wcslen(L"\\tagger.exe --quiet")+1) );
	wsprintf(taggerCommandLinePath, L"%s\\tagger.exe --quiet", data);

	// reset size to a suffisant value
	size = FILE_NAME_MAX;
	if( RegQueryValueEx(hKey, L"Install_Dir", 0, &type, (LPBYTE) data, &size) != ERROR_SUCCESS ) return 0;
	installDirectory = (LPWSTR) LocalAlloc(LPTR, sizeof(WCHAR) * (wcslen(data)+1) );
	wsprintf(installDirectory, L"%s", data);

	LocalFree(data);
	RegCloseKey(hKey);
	return 1;
}


// Forward declarations of functions included in this module

// functions that will be binded to the event listener
void initDialog(HWND,WPARAM,LPARAM);
void searchFiles(HWND,WPARAM,LPARAM);
void updateTagName(HWND,WPARAM,LPARAM);
void selectTagName(HWND,WPARAM,LPARAM);
void sortFilesList(HWND,WPARAM,LPARAM);
void closeDialog(HWND,WPARAM,LPARAM);
void showSaveAs(HWND, WPARAM, LPARAM);
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
	eventListener->bind(hWnd, ID_EXPORT, BN_CLICKED, showSaveAs);
	eventListener->bind(hWnd, ID_LIST_FILES, LVN_COLUMNCLICK, sortFilesList);			   	
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
	// ShowWindow( GetDlgItem( hWnd, ID_LIST_TAGS_ALL ), SW_HIDE);
	// hide available tags list
	ShowWindow( GetDlgItem( hWnd, ID_LIST_TAGS_AVAIL ), SW_HIDE);
	// disable files list
	EnableWindow( GetDlgItem( hWnd, ID_LIST_FILES ), FALSE);	
	SetFocus( GetDlgItem( hWnd, ID_TAGNAME ) );

	// Create listview Column Header 
	HWND hListView = GetDlgItem(hWnd, ID_LIST_FILES);
	WCHAR szText[100];
	LVCOLUMN columnInfo;
	columnInfo.mask = LVCF_WIDTH | LVCF_TEXT | LVCF_SUBITEM | LVCF_FMT;
	columnInfo.fmt = LVCFMT_LEFT;
	columnInfo.pszText = szText;
	columnInfo.iSubItem = 0;
	columnInfo.cx = 200;
	lstrcpy(szText, L"File name");
	ListView_InsertColumn(hListView, 0,  &columnInfo);
	columnInfo.iSubItem = 1;
	lstrcpy(szText, L"Full path");
	columnInfo.cx = 300;
	ListView_InsertColumn(hListView, 1,  &columnInfo);
	SendMessage(hListView, LVM_SETEXTENDEDLISTVIEWSTYLE, 0, LVS_EX_FULLROWSELECT);
	// set system image list
	SHFILEINFO shfi;
	HIMAGELIST hSystemSmallImageList  = (HIMAGELIST)SHGetFileInfo(    (LPCTSTR)L"C:\\", 0, &shfi, sizeof(SHFILEINFO), SHGFI_SYSICONINDEX | SHGFI_SMALLICON);
	ListView_SetImageList(GetDlgItem( hWnd, ID_LIST_FILES ), hSystemSmallImageList, LVSIL_SMALL);


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

int CALLBACK CompareFunc(LPARAM lParam1, LPARAM lParam2, LPARAM lParamSort) {
	if(lParamSort) {
		return wcscmp((LPWSTR) lParam1, (LPWSTR) lParam2);
	}
	return wcscmp((LPWSTR) lParam2, (LPWSTR) lParam1);	
}

void sortFilesList(HWND hWnd, WPARAM wParam, LPARAM lParam) {
	static BOOL bSortAscending[2] = {TRUE, FALSE};
	NM_LISTVIEW *phdn = (NM_LISTVIEW *) lParam;
    bSortAscending[phdn->iSubItem] = !bSortAscending[phdn->iSubItem];
	ListView_SortItems( GetDlgItem( hWnd, ID_LIST_FILES ), CompareFunc, (LPARAM) bSortAscending[phdn->iSubItem]);
}

/* Search for files matching given pattern.
Update files list.
*/
void searchFiles(HWND hWnd, WPARAM wParam, LPARAM lParam) {
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
	// SendDlgItemMessage(hWnd, ID_LIST_FILES, LB_RESETCONTENT, 0, 0);
	SendDlgItemMessage(hWnd, ID_LIST_FILES, LVM_DELETEALLITEMS, 0, 0);
														 
	// populate files list
	LPWSTR command, output;
	// retrieve matching files 
	command = (LPWSTR) LocalAlloc(LPTR, sizeof(WCHAR) * (wcslen(taggerCommandLinePath)+wcslen(L" files \"\"")+wcslen(pattern)+1) );
	swprintf(command, L"%s files \"%s\"", taggerCommandLinePath, pattern);

	output = DosExec(command);
	int index = 0;
	for(LPWSTR line = wcstok(output, L"\n"); line; line = wcstok(NULL, L"\n")) {
		line[wcslen(line)-1] = '\0';
		// SendDlgItemMessage(hWnd, ID_LIST_FILES, LB_ADDSTRING, 0, (LPARAM) line);
		// retrieve file's icon index
		SHFILEINFO info;
        DWORD result = SHGetFileInfo(line, 0, &info, sizeof(SHFILEINFO), SHGFI_ICON | SHGFI_SMALLICON | SHGFI_SHELLICONSIZE | SHGFI_SYSICONINDEX);
		// separate filename and path
		WCHAR* path = wcsdup(line);
		WCHAR* backslash = wcsrchr(line, (int)'\\');
		WCHAR* filename = backslash + 1;
		*backslash = '\0';
		LVITEM lvItem;
		memset(&lvItem,0,sizeof(LVITEM));
		lvItem.mask = LVIF_TEXT | LVIF_IMAGE | LVIF_PARAM;   
		lvItem.cchTextMax = FILE_NAME_MAX;
		lvItem.iItem = index;
		lvItem.iSubItem = 0;
		lvItem.iImage = info.iIcon;
		lvItem.pszText = filename;
		lvItem.lParam = (LPARAM) path;	// value to be sorted on
		// update listview
		ListView_InsertItem(GetDlgItem( hWnd, ID_LIST_FILES ), &lvItem);
		ListView_SetItemText(GetDlgItem( hWnd, ID_LIST_FILES ), index, 1, line);
		++index;
	}

	LocalFree(command);
	LocalFree(output);
	LocalFree(pattern);
}


void selectTagName(HWND hWnd, WPARAM wParam, LPARAM lParam) {
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
	// reset and hide available list
	SendDlgItemMessage(hWnd, ID_LIST_TAGS_AVAIL, LB_RESETCONTENT, 0, 0);
	ShowWindow( GetDlgItem( hWnd, ID_LIST_TAGS_AVAIL ), SW_HIDE);
	EnableWindow( GetDlgItem( hWnd, ID_LIST_TAGS_AVAIL ), FALSE);

	// retrieve input pattern
	int len = SendDlgItemMessage(hWnd, ID_TAGNAME, WM_GETTEXTLENGTH, 0, 0);
	LPWSTR pattern = (LPWSTR) LocalAlloc(LPTR, sizeof(WCHAR)*(len+1));
	GetDlgItemText(hWnd, ID_TAGNAME, (LPWSTR) pattern, len+1);

	// retrieve global list count
	LPWSTR str = (LPWSTR) LocalAlloc(LPTR, sizeof(WCHAR)*FILE_NAME_MAX);
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


void closeDialog(HWND hWnd, WPARAM wParam, LPARAM lParam) {
	if(taggerCommandLinePath != NULL) LocalFree(taggerCommandLinePath);
	if(installDirectory != NULL) LocalFree(installDirectory);
	DestroyWindow(hWnd);
	PostQuitMessage(0);
}

/* Export methods
*/
void showSaveAs(HWND hWnd, WPARAM, LPARAM) {
	WCHAR filename[FILE_NAME_MAX] = L"export.txt";
	WCHAR filter[1024] = L"XML Shareable Playlist Format (*.xspf)\0*.xspf\0Windows Media Playlist (*.wpl)\0*.wpl\0Advanced Stream Redirector (*.asx)\0*.asx\0M3U Playlist (*.m3u)\0*.m3u\0M3U8 unicode Playlist (*.m3u8)\0*.m3u8\0PLS Playlist (*.pls)\0*.pls\0";
	OPENFILENAME ofn;
	memset(&ofn, 0, sizeof(ofn));
	ofn.lStructSize   = sizeof(ofn);
	ofn.hwndOwner     = hWnd;
	ofn.lpstrFilter   = filter;
	ofn.lpstrFile     = filename;
	ofn.nMaxFile      = sizeof(filename);
	ofn.Flags         = OFN_OVERWRITEPROMPT;
	GetSaveFileName(&ofn);
	 if(!ofn.nFileExtension) {
		//ofn.nFilterIndex
		wcscat(filename, L".txt");
	}
	// write filename with current list content
	FILE *fd = _wfopen(filename, L"w+");
	int count = ListView_GetItemCount(GetDlgItem(hWnd, ID_LIST_FILES));
	for(int i = 0; i < count; ++i) {
		LVITEM lvItem;
		lvItem.mask = LVIF_PARAM;
		lvItem.iItem = i;
		lvItem.iSubItem = 0;
		ListView_GetItem(GetDlgItem( hWnd, ID_LIST_FILES ), &lvItem);
		fwprintf(fd, L"%s\r\n", (LPWSTR) lvItem.lParam);
	}
	fclose(fd);
}

/* Context menu methods
*/

void showContext(HWND hWnd, WPARAM wParam, LPARAM lParam) {
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

LPWSTR getSelectedFile() {
	HWND hWnd = GetActiveWindow();
//	int index = SendDlgItemMessage(hWnd, ID_LIST_FILES, LB_GETCURSEL, (WPARAM) 0, (LPARAM) 0);
//	int len = SendDlgItemMessage(hWnd, ID_LIST_FILES, LB_GETTEXTLEN, (WPARAM) index, (LPARAM) 0);
//	LPWSTR path = (LPWSTR) LocalAlloc(LPTR, sizeof(WCHAR)*(len+1));
//	SendDlgItemMessage(hWnd, ID_LIST_FILES, LB_GETTEXT, (WPARAM) index, (LPARAM) path);
//	return path;

	int index = ListView_GetNextItem(GetDlgItem( hWnd, ID_LIST_FILES ), -1, LVNI_SELECTED);
	LVITEM lvItem;
	lvItem.mask = LVIF_PARAM;
	lvItem.iItem = index;
	lvItem.iSubItem = 0;
	ListView_GetItem(GetDlgItem( hWnd, ID_LIST_FILES ), &lvItem);
	return (LPWSTR) lvItem.lParam;
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