#include <windows.h>
#include <commctrl.h>

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
// Global variables
WCHAR* taggerCommandLinePath = NULL;

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
	return 1;
}



/* example to output a message 
WCHAR* buff = (WCHAR*) malloc(1024);
_swprintf(buff, L"%s", msg);
MessageBox(NULL, buff, L"info", MB_OK);
*/

// Forward declarations of functions included in this module

// functions that will be binded to the event listener
void initDialog(HWND, WPARAM, LPARAM);
void removeTags(HWND, WPARAM, LPARAM);
void addTags(HWND, WPARAM, LPARAM);
void updateTagName(HWND, WPARAM, LPARAM);
void closeDialog(HWND, WPARAM, LPARAM);

int WINAPI WinMain(HINSTANCE hInstance,
                   HINSTANCE hPrevInstance,
                   LPSTR lpCmdLine,
                   int nCmdShow) {
    	

	INITCOMMONCONTROLSEX InitCtrlEx;
	InitCtrlEx.dwSize = sizeof(INITCOMMONCONTROLSEX);
	InitCtrlEx.dwICC  = ICC_BAR_CLASSES;
	InitCommonControlsEx(&InitCtrlEx);

	EventListener* eventListener = EventListener::getInstance(HWND_DIALOG);

	HWND hSplash = CreateDialogParam(hInstance, MAKEINTRESOURCE(IDD_SPLASH), 0, (DLGPROC) NULL, 0);
    ShowWindow(hSplash, SW_SHOW);
    UpdateWindow(hSplash);


	HWND hWnd;
    if (!(hWnd = CreateDialogParam(hInstance, MAKEINTRESOURCE(IDD_DIALOG), 0, (DLGPROC) EventListener::dlgProc, 0))) {
        MessageBox(
			NULL,
            L"App creation failed!",
            L"Tagger",
            NULL
		);
        return 1;
    }


	// since WM_INITDIALOG has been sent before binding, we need to initialize dialog manually
	initDialog(hWnd, (WPARAM) 0, (LPARAM) 0);
	// bind events we're interested in with appropriate handling functions
	// global events
	eventListener->bind(hWnd, 0, WM_CLOSE, closeDialog);
	// controls events
	eventListener->bind(hWnd, ID_REMOVE, BN_CLICKED, removeTags);
	eventListener->bind(hWnd, ID_ADD, BN_CLICKED, addTags);
	eventListener->bind(hWnd, ID_TAGNAME, EN_CHANGE, updateTagName);
	eventListener->bind(hWnd, IDOK, BN_CLICKED, closeDialog);	

	ShowWindow(hSplash, SW_HIDE);
    ShowWindow(hWnd, SW_SHOWNORMAL);
    UpdateWindow(hWnd);

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


void initDialog(HWND hWnd, WPARAM, LPARAM) {

	// retrieve command line arguments
    int argc;
	LPWSTR *argv = CommandLineToArgvW(GetCommandLine(), &argc);

	if (argv == NULL || argc < 2) { 
		MessageBox(NULL, L"A filename with full path is expected as first argument of this app.", L"Notice", MB_OK | MB_ICONERROR);
		// closeDialog(hWnd);
	 }
	 else {
		// set filename, according to received argument
		SetDlgItemText(hWnd, ID_FILENAME, argv[1]);

		// enable '+' and '-' buttons
		EnableWindow( GetDlgItem( hWnd, ID_REMOVE ), TRUE);
		EnableWindow( GetDlgItem( hWnd, ID_ADD ), TRUE);

		// try to retrieve list of available tags
		if(!getEnv()) {
			MessageBox(NULL, L"Unable to locate installation directory.\nTo solve this, try re-installing the application.", L"Error", MB_OK|MB_ICONERROR);
		}
		else {
			// populate tags lists
			LPWSTR command, output;
			// 1) retrieve all existing tags 
			command = (LPWSTR) LocalAlloc(LPTR, sizeof(WCHAR) * (wcslen(taggerCommandLinePath)+wcslen(L" tags")+1) );
			swprintf(command, L"%s tags", taggerCommandLinePath);
			output = DosExec(command);
			for(LPWSTR line = wcstok(output, L"\n"); line; line = wcstok(NULL, L"\n")) {
				line[wcslen(line)-1] = '\0';
				SendDlgItemMessage(hWnd, ID_LIST_TAGS_AVAIL, LB_ADDSTRING, 0, (LPARAM) line);
				SendDlgItemMessage(hWnd, ID_LIST_TAGS_ALL, LB_ADDSTRING, 0, (LPARAM) line);
			}
			LocalFree(command);
			LocalFree(output);
			// 2) retrieve tags already applied on the given file
			command = (LPWSTR) LocalAlloc(LPTR, sizeof(WCHAR) * (wcslen(taggerCommandLinePath)+wcslen(L" tags ")+wcslen(L"\"\"")+wcslen(argv[1])+1) );
			swprintf(command, L"%s tags \"%s\"", taggerCommandLinePath, argv[1]);		
			output = DosExec(command);
			for(LPWSTR line = wcstok(output, L"\n"); line; line = wcstok(NULL, L"\n")) {
				line[wcslen(line)-1] = '\0';
				SendDlgItemMessage(hWnd, ID_LIST_TAGS_SET, LB_ADDSTRING, 0, (LPARAM) line);
				// remove tag from ID_LIST_TAGS_AVAIL and ID_LIST_TAGS_ALL
				int index = SendDlgItemMessage(hWnd, ID_LIST_TAGS_ALL, LB_FINDSTRING, (WPARAM) 0, (LPARAM) line);
				SendDlgItemMessage(hWnd, ID_LIST_TAGS_AVAIL, LB_DELETESTRING, (WPARAM) index, 0);
				SendDlgItemMessage(hWnd, ID_LIST_TAGS_ALL, LB_DELETESTRING, (WPARAM) index, 0);
			}
			LocalFree(command);
			LocalFree(output);
		}
	 }
	LocalFree(argv);
}

void removeTags(HWND hWnd, WPARAM, LPARAM){
	// retrieve filename
	int filename_len = SendDlgItemMessage(hWnd, ID_FILENAME, WM_GETTEXTLENGTH, 0, 0);
	LPWSTR filename = (LPWSTR) LocalAlloc(LPTR, sizeof(WCHAR)*(filename_len+1));
	GetDlgItemText(hWnd, ID_FILENAME, (LPWSTR) filename, filename_len+1);

	int sel_count = SendDlgItemMessage(hWnd, ID_LIST_TAGS_SET, LB_GETSELCOUNT, 0, 0);
	INT* indexes = (INT*) LocalAlloc(LPTR, sizeof(INT) * sel_count);
	SendDlgItemMessage(hWnd, ID_LIST_TAGS_SET, LB_GETSELITEMS, (WPARAM) sel_count, (LPARAM) indexes);

	// first pass : update database
	for(int i = 0; i < sel_count; ++i) {
		int tagname_len = SendDlgItemMessage(hWnd, ID_LIST_TAGS_SET, LB_GETTEXTLEN, (WPARAM) indexes[i], 0);
		LPWSTR tagname = (LPWSTR) LocalAlloc(LPTR, sizeof(WCHAR)*(tagname_len+1));
		SendDlgItemMessage(hWnd, ID_LIST_TAGS_SET, LB_GETTEXT, (WPARAM) indexes[i], (LPARAM) tagname);
// todo : we should check the current pattern in ID_TAG_NAME
		// add selected tags to ID_LIST_TAGS_AVAIL and ID_LIST_TAGS_ALL
		SendDlgItemMessage(hWnd, ID_LIST_TAGS_AVAIL, LB_ADDSTRING, 0, (LPARAM) tagname);
		SendDlgItemMessage(hWnd, ID_LIST_TAGS_ALL, LB_ADDSTRING, 0, (LPARAM) tagname);
		// remove tag from file
		// call DOS command (tagger tag -{tagname} file}
		LPWSTR command = (LPWSTR) LocalAlloc(LPTR, sizeof(WCHAR) * (wcslen(taggerCommandLinePath)+wcslen(L" tag -")+tagname_len+wcslen(L" \"\"\"\"")+filename_len+1) );
		swprintf(command, L"%s tag -\"%s\" \"%s\"", taggerCommandLinePath, tagname, filename);
		DosExec(command);
		free(command);
		free(tagname);
	}
	// second pass : remove selected items from ID_LIST_TAGS_SET
	int count = SendDlgItemMessage(hWnd, ID_LIST_TAGS_SET, LB_GETCOUNT, 0, 0);
	for(int i = count-1; i >= 0; --i) {
		if(SendDlgItemMessage(hWnd, ID_LIST_TAGS_SET, LB_GETSEL, (WPARAM) i, 0)) {
			SendDlgItemMessage(hWnd, ID_LIST_TAGS_SET, LB_DELETESTRING, (WPARAM) i, 0);
		}
	}

	free(indexes);
	free(filename);
}

/* Add selected tag to given file.
 Update available tags and set tags lists.
*/
void addTags(HWND hWnd, WPARAM wParam, LPARAM lParam) {
	// retrieve selected tags
	int sel_count = SendDlgItemMessage(hWnd, ID_LIST_TAGS_AVAIL, LB_GETSELCOUNT, 0, 0);
	if(sel_count == 0){
		// no tag is selected
		int len = SendDlgItemMessage(hWnd, ID_TAGNAME, WM_GETTEXTLENGTH, 0, 0);
		if(len == 0) {
			// no input pattern is given : do nothing
			return;
		}
		else {
			// count number of partial match tags
			int count = SendDlgItemMessage(hWnd, ID_LIST_TAGS_AVAIL, LB_GETCOUNT, 0, 0);
			if(count == 0) {
				int tagname_len = SendDlgItemMessage(hWnd, ID_TAGNAME, WM_GETTEXTLENGTH, 0, 0);
				LPWSTR tagname = (LPWSTR) malloc(sizeof(WCHAR)*(tagname_len+1));
				GetDlgItemText(hWnd, ID_TAGNAME, (LPWSTR) tagname, tagname_len+1);
				// check if tag is already in file's tag list								
				if(SendDlgItemMessage(hWnd, ID_LIST_TAGS_SET, LB_FINDSTRING, 0, (LPARAM) tagname) == LB_ERR) {
					// create a new tag
					LPWSTR command = (LPWSTR) LocalAlloc(LPTR, sizeof(WCHAR) * (wcslen(taggerCommandLinePath)+wcslen(L" create +\"\"")+tagname_len+1) );
					swprintf(command, L"%s create \"%s\"", taggerCommandLinePath, tagname);
					DosExec(command);
					LocalFree(command);
					// add item to global and available tags lists
					SendDlgItemMessage(hWnd, ID_LIST_TAGS_ALL, LB_ADDSTRING, 0, (LPARAM) tagname);
					int index = SendDlgItemMessage(hWnd, ID_LIST_TAGS_AVAIL, LB_ADDSTRING, 0, (LPARAM) tagname);
					SendDlgItemMessage(hWnd, ID_LIST_TAGS_AVAIL, LB_SELITEMRANGE, TRUE, (LPARAM) MAKELPARAM(index, index));
					addTags(hWnd, wParam, lParam);
				}
			}
			else {
				// input pattern is not among remaining items
				// this should never occur since exact match item is automaticaly selected at input update
			}
		}
	}
	else {
		// at least one tag is selected		
		int* indexes = (int*) LocalAlloc(LPTR, sizeof(int)*sel_count);
		SendDlgItemMessage(hWnd, ID_LIST_TAGS_AVAIL, LB_GETSELITEMS, (WPARAM) sel_count, (LPARAM) indexes);
		// first pass : update database
		for(int i = 0; i < sel_count; ++i) {
			// get string for current index
			int tagname_len = SendDlgItemMessage(hWnd, ID_LIST_TAGS_AVAIL, LB_GETTEXTLEN, (WPARAM) indexes[i], 0);
			LPWSTR tagname = (LPWSTR) LocalAlloc(LPTR, sizeof(WCHAR)*(tagname_len+1));
			SendDlgItemMessage(hWnd, ID_LIST_TAGS_AVAIL, LB_GETTEXT, (WPARAM) indexes[i], (LPARAM) tagname);
			// add selected tags to ID_LIST_TAGS_SET
			SendDlgItemMessage(hWnd, ID_LIST_TAGS_SET, LB_ADDSTRING, 0, (LPARAM) tagname);

			// call DOS command (tagger tag +{new_tag} file}
			// retrieve filename
			int filename_len = SendDlgItemMessage(hWnd, ID_FILENAME, WM_GETTEXTLENGTH, 0, 0);
			LPWSTR filename = (LPWSTR) LocalAlloc(LPTR, sizeof(WCHAR)*(filename_len+1));
			GetDlgItemText(hWnd, ID_FILENAME, (LPWSTR) filename, filename_len+1);

			LPWSTR command = (LPWSTR) LocalAlloc(LPTR, sizeof(WCHAR) * (wcslen(taggerCommandLinePath)+wcslen(L" tag +")+tagname_len+wcslen(L" \"\"\"\"")+filename_len+1) );
			swprintf(command, L"%s tag +\"%s\" \"%s\"", taggerCommandLinePath, tagname, filename);
			DosExec(command);

			LocalFree(command);
			LocalFree(filename);
			LocalFree(tagname);
		}
		// second pass : remove selected items from ID_LIST_TAGS_AVAIL and ID_LIST_TAGS_ALL
		int count = SendDlgItemMessage(hWnd, ID_LIST_TAGS_AVAIL, LB_GETCOUNT, 0, 0);
		for(int i = count-1; i >= 0; --i) {
			if(SendDlgItemMessage(hWnd, ID_LIST_TAGS_AVAIL, LB_GETSEL, (WPARAM) i, 0)) {
				SendDlgItemMessage(hWnd, ID_LIST_TAGS_AVAIL, LB_DELETESTRING, (WPARAM) i, 0);
				SendDlgItemMessage(hWnd, ID_LIST_TAGS_ALL, LB_DELETESTRING, (WPARAM) i, 0);
			}
		}
		LocalFree(indexes);
	}
}

/* Limit the list of available tags to those which name match the current pattern
*/
void updateTagName(HWND hWnd, WPARAM, LPARAM) {
	// reset available list
	SendDlgItemMessage(hWnd, ID_LIST_TAGS_AVAIL, LB_RESETCONTENT, 0, 0);

	// retrieve input pattern
	int len = SendDlgItemMessage(hWnd, ID_TAGNAME, WM_GETTEXTLENGTH, 0, 0);
	LPWSTR pattern = (LPWSTR) malloc(sizeof(WCHAR)*(len+1));
	GetDlgItemText(hWnd, ID_TAGNAME, (LPWSTR) pattern, len+1);

	// retrieve global list count
	LPWSTR str = (LPWSTR) malloc(sizeof(WCHAR)*1024);
	int count = SendDlgItemMessage(hWnd, ID_LIST_TAGS_ALL, LB_GETCOUNT, 0, 0);

	// add only tags matching input pattern
	for(int i = 0; i < count; ++i) {
		SendDlgItemMessage(hWnd, ID_LIST_TAGS_ALL, LB_GETTEXT, (WPARAM) i, (LPARAM) str);
		if(wcsstr(str, pattern) == str || wcslen(pattern) == 0) {
			int index = SendDlgItemMessage(hWnd, ID_LIST_TAGS_AVAIL, LB_ADDSTRING, 0, (LPARAM) str);
			if(wcscmp(str, pattern) == 0) {
				SendDlgItemMessage(hWnd, ID_LIST_TAGS_AVAIL, LB_SELITEMRANGE, TRUE, (LPARAM) MAKELPARAM(index, index));
			}
		}
	}
}


void closeDialog(HWND hWnd, WPARAM, LPARAM) {
	if(taggerCommandLinePath != NULL) LocalFree(taggerCommandLinePath);
	DestroyWindow(hWnd);
	PostQuitMessage(0);
}