/* tfmon.cpp - win32 application for watching filesystem changes and maintaining tagger database consistency.

    This file is part of the tagger-ui suite <http://www.github.com/cedricfrancoys/tagger-ui>
    Copyright (C) Cedric Francoys, 2016, Yegen
    Some Right Reserved, GNU GPL 3 license <http://www.gnu.org/licenses/>
*/

#include <windows.h>
#include <commctrl.h>
#include <shlobj.h>
#include <shellapi.h>
#include <sddl.h>
#include <uxtheme.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "tfmon.h" 
#include "FSChangeNotifier.h"


#include "../commons/eventlistener.h" 
#include "../commons/dosexec.h" 
#include "../commons/dlgctrl.h" 
#include "../commons/registry.h" 
#include "../commons/winenv.h" 

#pragma comment(linker, \
  "\"/manifestdependency:type='Win32' "\
  "name='Microsoft.Windows.Common-Controls' "\
  "version='6.0.0.0' "\
  "processorArchitecture='*' "\
  "publicKeyToken='6595b64144ccf1df' "\
  "language='*'\"")

#pragma comment(lib, "ComCtl32.lib")
#pragma comment(lib, "uxtheme.lib")



// Global variables

// main application window (hidden)
HWND hWnd;
// dialogs
HWND hWndActivity;
HWND hWndSettings;
HWND hWndAbout;

DWORD WM_NOTIFYICON = RegisterWindowMessage(L"TaggerNotifyIcon");


// custom structure for holding settings data used during initialization
struct {
	LPWSTR			taggerCommandLinePath;
	LPWSTR			taggerVersion;
	LPDRIVEINFO*	lpDrivesInfos;
	UINT			nDrives;
} Settings;

// forward declarations of functions included in this module
BOOL initApp();
BOOL initDialogActivity();
BOOL initDialogSettings();
BOOL initDialogAbout();


BOOL StartMonitoring();

void appendLog(UINT type, LPCWSTR str, BOOL isCommand=false);

// functions to be bound to the event listener
void closeApp(HWND, WPARAM, LPARAM);
void notifyIcon(HWND, WPARAM, LPARAM);
// filesystem changes callbacks
void fileMove(HWND, WPARAM, LPARAM);
void fileRemove(HWND, WPARAM, LPARAM);
void fileRestore(HWND, WPARAM, LPARAM);
void watcherStopped(HWND, WPARAM, LPARAM);
// dialogs callbacks
void closeDialog(HWND, WPARAM, LPARAM);
// context menu handlers
void menuActivityLog(HWND,WPARAM,LPARAM);
void menuSettings(HWND,WPARAM,LPARAM);
void menuAbout(HWND,WPARAM,LPARAM);
void menuRestart(HWND,WPARAM,LPARAM);




int WINAPI WinMain(HINSTANCE hInstance,	HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
	// ensure there is only one instance
	HANDLE hMutex;
	if ( (hMutex = CreateMutex(NULL, TRUE, L"TUIFSM")) == NULL) { 
		MessageBox(NULL, L"Could not create mutex object.", L"error", MB_OK | MB_ICONERROR); 
	}
	else {
		if(GetLastError() == ERROR_ALREADY_EXISTS) {
			// main window already exists : exit current instance
			return 0;
		}
	}

	// init main window
	if(!initApp()) {
        MessageBox(NULL, L"App creation failed @ initApp", L"TaggerUI", MB_OK | MB_ICONERROR);
		return 1;
	}
	// init dialogs
	if(!initDialogActivity()) {
        MessageBox(NULL, L"App creation failed @ initDialogActivity", L"TaggerUI", MB_OK | MB_ICONERROR);
		return 1;
	}
	if(!initDialogSettings()) {
        MessageBox(NULL, L"App creation failed @ initDialogSettings", L"TaggerUI", MB_OK | MB_ICONERROR);
		return 1;
	}
	if(!initDialogAbout()) {
        MessageBox(NULL, L"App creation failed @ initDialogAbout", L"TaggerUI", MB_OK | MB_ICONERROR);
		return 1;
	}

	// bind main window events 
	EventListener* wndEventListener = EventListener::getInstance(HWND_WINDOW);
	// global events
	wndEventListener->bind(hWnd, 0, WM_NOTIFYICON, notifyIcon);
	wndEventListener->bind(hWnd, 0, WM_FSNOTIFY_MOVED, fileMove);
	wndEventListener->bind(hWnd, 0, WM_FSNOTIFY_REMOVED, fileRemove);
	wndEventListener->bind(hWnd, 0, WM_FSNOTIFY_RESTORED, fileRestore);
	wndEventListener->bind(hWnd, 0, WM_FSNOTIFY_STOP, watcherStopped);
	
	// menu events
	wndEventListener->bind(hWnd, IDD_DIALOG_ACTIVITY, 0, menuActivityLog);
	wndEventListener->bind(hWnd, IDD_DIALOG_SETTINGS, 0, menuSettings);
	wndEventListener->bind(hWnd, IDD_DIALOG_ABOUT, 0, menuAbout);
	wndEventListener->bind(hWnd, IDM_RESTART, 0, menuRestart);
	wndEventListener->bind(hWnd, IDM_QUIT, 0, closeApp);

	// bind dialogs-related events
	EventListener* dlgEventListener = EventListener::getInstance(HWND_DIALOG);
	// global events
	dlgEventListener->bind(hWndActivity, 0, WM_CLOSE, closeDialog);	
	dlgEventListener->bind(hWndAbout, 0, WM_CLOSE, closeDialog);		
	// control-specific events
	dlgEventListener->bind(hWndActivity, IDOK, BN_CLICKED, closeDialog);	
	dlgEventListener->bind(hWndAbout, IDOK, BN_CLICKED, closeDialog);	
	dlgEventListener->bind(hWndActivity, IDC_TAB, TCN_SELCHANGE, DlgCtrl_EvChangeTab);


	// hide all windows at startup
	ShowWindow(hWnd, SW_HIDE);
    ShowWindow(hWndActivity, SW_HIDE);
    //ShowWindow(hWndSettings, SW_HIDE);
    ShowWindow(hWndAbout, SW_HIDE);

	// start monitoring 
	StartMonitoring();


    // main message loop
	MSG msg;
    while (GetMessage(&msg, NULL, 0, 0) > 0) {
		TranslateMessage(&msg);
		DispatchMessage(&msg);
    }

    // return (int) msg.wParam;
	return 0;
}
  
BOOL StartMonitoring() {

// todo : we should do the extraction of required data somewhere else and store them into the Settings structure
// what about InitDialogActivity ?

	// add some logs
	WCHAR outputBuff[1024];

	appendLog(ID_LOG_APP, L"Detected environment:", true);	
	LPWINDOWSINFO lpWinInfo = WinEnv_GetWindowsInfo();
	wsprintf(outputBuff, L"Windows version: %s (%s)", lpWinInfo->szVersion, lpWinInfo->szName);
	appendLog(ID_LOG_APP, outputBuff);

	wsprintf(outputBuff, L"tagger.exe command line: %s", Settings.taggerCommandLinePath);
	appendLog(ID_LOG_APP, outputBuff);

	wsprintf(outputBuff, L"%s --version", Settings.taggerCommandLinePath);
	Settings.taggerVersion = wcstok(DosExec(outputBuff), L"\r\n");

	wsprintf(outputBuff, L"tagger.exe version: %s", Settings.taggerVersion);
	appendLog(ID_LOG_APP, outputBuff);

	appendLog(ID_LOG_APP, L"Retrieved drives and recycle bins:", true);
// todo : check settings to know which kind of drives user wants to be watched


	// retrieve environment data
	Settings.lpDrivesInfos = (LPDRIVEINFO*) LocalAlloc(LPTR, sizeof(LPDRIVEINFO)*26);
	Settings.nDrives = 0;

	LPDRIVEINFO lpdi;

	// load local fixed drives
// todo : improve this (reduce redundant code)
	LPWSTR* lpFixedDrives = WinEnv_GetDrives(DRIVE_FIXED);
	for(UINT i = 0; lpFixedDrives[i]; ++i) {
		if(lpdi = WinEnv_GetDriveInfo(lpFixedDrives[i])) {
			Settings.lpDrivesInfos[Settings.nDrives++] = lpdi;
			wsprintf(outputBuff, L"%s (%s) - %s", lpdi->szDrive, lpdi->szFileSystem, lpdi->szRecycleBinPath);
			appendLog(ID_LOG_APP, outputBuff);
		}
		else {
			wsprintf(outputBuff, L"Unable to get info for drive %s", lpFixedDrives[i]);
			appendLog(ID_LOG_APP, outputBuff);
		}
	}

	// load remote drives 
	LPWSTR* lpRemovableDrives = WinEnv_GetDrives(DRIVE_REMOVABLE);
	for(UINT i = 0; lpRemovableDrives[i]; ++i) {
		if(lpdi = WinEnv_GetDriveInfo(lpRemovableDrives[i])) {
			Settings.lpDrivesInfos[Settings.nDrives++] = lpdi;
			wsprintf(outputBuff, L"%s (%s) - %s", lpdi->szDrive, lpdi->szFileSystem, lpdi->szRecycleBinPath);
			appendLog(ID_LOG_APP, outputBuff);
		}
		else {
			wsprintf(outputBuff, L"Unable to get info for drive %s", lpRemovableDrives[i]);
			appendLog(ID_LOG_APP, outputBuff);
		}
	}

	// load remote drives 
	LPWSTR* lpRemoteDrives = WinEnv_GetDrives(DRIVE_REMOTE);
	for(UINT i = 0; lpRemoteDrives[i]; ++i) {
		if(lpdi = WinEnv_GetDriveInfo(lpRemoteDrives[i])) {
			Settings.lpDrivesInfos[Settings.nDrives++] = lpdi;
			wsprintf(outputBuff, L"%s (%s) - %s", lpdi->szDrive, lpdi->szFileSystem, lpdi->szRecycleBinPath);
			appendLog(ID_LOG_APP, outputBuff);
		}
		else {
			wsprintf(outputBuff, L"Unable to get info for drive %s", lpRemoteDrives[i]);
			appendLog(ID_LOG_APP, outputBuff);
		}
	}

	FSChangeNotifier* lpNotifier = FSChangeNotifier::GetInstance();

	// initialize change watcher
	if (!lpNotifier->Init()) {
		MessageBox(0, L"Initialization Error", NULL, MB_ICONERROR);
		return FALSE;
	}

	// add drives to watch list
	appendLog(ID_LOG_APP, L"Drive(s) supported for monitoring:", true);
	for(UINT i = 0; i < Settings.nDrives; ++i) {
// todo : improve check to accept all theorically supported filesystems (NTFS, SMB 3+, CsvFS, ReFS)
		if(wcsicmp(Settings.lpDrivesInfos[i]->szFileSystem, L"NTFS") == 0) {
			if (lpNotifier->AddPath(Settings.lpDrivesInfos[i]->szDrive) == E_FILESYSMON_SUCCESS) {
				wsprintf(outputBuff, L"%s", Settings.lpDrivesInfos[i]->szDrive);
				appendLog(ID_LOG_APP, outputBuff);
				continue;
			}
		}		
		// wsprintf(outputBuff, L"Error adding drive %s to monitoring list", Settings.lpDrivesInfos[i]->szDrive);
		// appendLog(ID_LOG_APP, outputBuff);
	}

	appendLog(ID_LOG_APP, L"Path(s) excluded from monitoring:", true);
	// exclude windows\Temp 
	lpNotifier->AddExclusion(WinEnv_GetFolderPath(CSIDL_TEMP));
	// exclude <user profile>\Local Settings\Temp
	lpNotifier->AddExclusion(WinEnv_GetFolderPath(CSIDL_MYTEMP));
	// exclude <user profile>\Local Settings\Applicaiton Data
	lpNotifier->AddExclusion(WinEnv_GetFolderPath(CSIDL_LOCAL_APPDATA));
	// exclude .tagger directory (tagger database)
	LPCURRENTUSERINFO lpInfo = WinEnv_GetCurrentUserInfo();
	wsprintf(outputBuff, L"%s\\.tagger", lpInfo->szHomeDirectory);

	lpNotifier->AddExclusion(outputBuff);
	appendLog(ID_LOG_APP, WinEnv_GetFolderPath(CSIDL_TEMP));
	appendLog(ID_LOG_APP, WinEnv_GetFolderPath(CSIDL_MYTEMP));
	appendLog(ID_LOG_APP, WinEnv_GetFolderPath(CSIDL_LOCAL_APPDATA));
	appendLog(ID_LOG_APP, outputBuff);

	
	// bind main window with notifier
	lpNotifier->bind(hWnd);

	appendLog(ID_LOG_APP, L"Starting monitoring...", true);
	// start watching thread
	if (!lpNotifier->Start()) {
		MessageBox(0, L"Initialization Error", NULL, MB_ICONERROR);
		return FALSE;
	}

	return TRUE;
}


BOOL initApp() {
	Settings.taggerCommandLinePath = NULL;
	// Read registry to fetch path of installation directory.
	LPWSTR data = (LPWSTR) Registry_Read(HKEY_LOCAL_MACHINE, L"SOFTWARE\\TaggerUI", L"Tagger_Dir");
	if(!data) {
		MessageBox(NULL, L"Unrecoverable error : unable to retrieve tagger.exe location from registry.", L"TaggerUI", MB_OK | MB_ICONERROR);
		return FALSE;
	}
	else {
		// Set taggerCommandLinePath according to HKLM/SOFTWARE/TaggerUI/Tagger_Dir.
		Settings.taggerCommandLinePath = (LPWSTR) LocalAlloc(LPTR, sizeof(WCHAR) * (wcslen(data) + wcslen(L"\\tagger.exe")+1) );
		wsprintf(Settings.taggerCommandLinePath, L"%s\\tagger.exe", data);
	}			  
	
	INITCOMMONCONTROLSEX InitCtrlEx;
	InitCtrlEx.dwSize = sizeof(INITCOMMONCONTROLSEX);
	InitCtrlEx.dwICC  = ICC_BAR_CLASSES | ICC_TAB_CLASSES;
	InitCommonControlsEx(&InitCtrlEx);

	WNDCLASSEX wcex;
	wcex.cbSize			= sizeof(WNDCLASSEX);
	wcex.style			= CS_CLASSDC;
	wcex.lpfnWndProc	= EventListener::wndProc;
	wcex.cbClsExtra		= 0;
	wcex.cbWndExtra		= 0;
	wcex.hInstance		= GetModuleHandle(NULL);
	wcex.hIcon			= LoadIcon(wcex.hInstance, MAKEINTRESOURCE(IDD_ICON));
	wcex.hCursor		= LoadCursor(NULL, IDC_ARROW);
	wcex.hbrBackground	= (HBRUSH)(COLOR_WINDOW+1);
	wcex.lpszMenuName	= 0;
	wcex.lpszClassName	= L"myClass";
	wcex.hIconSm		= LoadIcon(wcex.hInstance, MAKEINTRESOURCE(IDD_ICON));

	// register custom class
	if(!RegisterClassEx(&wcex)) {
		return FALSE;
	}

	// create main window
	if(!(hWnd = CreateWindowEx(WS_EX_TOOLWINDOW, L"myClass", L"", WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, 0, CW_USEDEFAULT, 0, NULL, NULL, GetModuleHandle(NULL), NULL))) {
        return FALSE;
	}

	// create status bar notify icon 
    NOTIFYICONDATA tnid;  
    tnid.cbSize = sizeof(NOTIFYICONDATA); 
    tnid.hWnd = hWnd; 
    tnid.uID = 0; 
    tnid.uFlags = NIF_MESSAGE | NIF_ICON | NIF_TIP; 
    tnid.uCallbackMessage = WM_NOTIFYICON; 
    tnid.hIcon = (HICON) LoadIcon(GetModuleHandle(NULL), MAKEINTRESOURCE(IDD_ICON)); 
    wcscpy(tnid.szTip, L"TaggerUI FileSystem Monitor");
    Shell_NotifyIcon(NIM_ADD, &tnid); 

	return TRUE;
}


BOOL initDialogActivity() {
    if (!(hWndActivity = CreateDialogParam(GetModuleHandle(NULL), MAKEINTRESOURCE(IDD_DIALOG_ACTIVITY), hWnd, (DLGPROC) EventListener::dlgProc, 0))) {
        return FALSE;
    }
	// init tabControl
	DlgCtrl_InitTabs(hWndActivity, IDC_TAB, 3, IDD_ACTIVITY_PANE_ONE, (DLGPROC) EventListener::dlgProc);

	// set log editControl font
	LOGFONT logfont;
	memset(&logfont, 0, sizeof(logfont));
	logfont.lfCharSet = ANSI_CHARSET;
	logfont.lfWeight = FW_NORMAL;
	logfont.lfHeight = -MulDiv(9.5, GetDeviceCaps(GetDC(hWnd), LOGPIXELSY), 72);
	logfont.lfOutPrecision = OUT_RASTER_PRECIS;
	//lstrcpy(logfont.lfFaceName, L"Lucida Console");
	lstrcpy(logfont.lfFaceName, L"Consolas");
	HFONT hFont = CreateFontIndirect(&logfont); 
	DlgCtrl_SendMessage(hWndActivity, ID_LOG_TAGGER, WM_SETFONT, (WPARAM) hFont, TRUE);

	// set listboxes horizontal extent
	DlgCtrl_SendMessage(hWndActivity, ID_LOG_FS, LB_SETHORIZONTALEXTENT, (WPARAM) 1024, 0);
	DlgCtrl_SendMessage(hWndActivity, ID_LOG_APP, LB_SETHORIZONTALEXTENT, (WPARAM) 1024, 0);

	return TRUE;
}


BOOL initDialogSettings() {

	return TRUE;
}

BOOL initDialogAbout() {
    if (!(hWndAbout = CreateDialogParam(GetModuleHandle(NULL), MAKEINTRESOURCE(IDD_DIALOG_ABOUT), hWnd, (DLGPROC) EventListener::dlgProc, 0))) {
        return FALSE;
    }
	return TRUE;
}

void fileMove(HWND hWnd, WPARAM wParam, LPARAM lParam) {
	static WCHAR buff[4192];
	LPWSTR output;
	LPWSTR oldFileName = (LPWSTR) wParam;
	LPWSTR newFileName = (LPWSTR) lParam;

	if(oldFileName == NULL || newFileName == NULL) return;
	if(wcscmp(oldFileName, newFileName) == 0) return;	  // no change

	appendLog(ID_LOG_FS, L"File moved:");
	wsprintf(buff, L"    Src: %s", oldFileName);
	appendLog(ID_LOG_FS, buff);
	wsprintf(buff, L"    Dst: %s", newFileName);
	appendLog(ID_LOG_FS, buff);
	appendLog(ID_LOG_FS, L"");

	if(GetFileAttributes(newFileName) & FILE_ATTRIBUTE_DIRECTORY) {
		// search for sub items (files or directories)
		wsprintf(buff, L"%s --quiet --files list \"%s\\*\"", Settings.taggerCommandLinePath, oldFileName);
		output = DosExec(buff);
		appendLog(ID_LOG_TAGGER, buff, true);
		appendLog(ID_LOG_TAGGER, output);
		for(LPWSTR line = wcstok(output, L"\n"); line; line = wcstok(NULL, L"\n")) {
			// for each line, replace oldFileName by newFileName
			line[wcslen(line)-1] = '\0';			
			wsprintf(buff, L"%s --files rename \"%s\" \"%s\\%s\"", Settings.taggerCommandLinePath, line, newFileName, line+wcslen(oldFileName)+1);
			output = DosExec(buff);
			appendLog(ID_LOG_TAGGER, buff, true);
			appendLog(ID_LOG_TAGGER, output);
		}
	}

	// check file's presence in database (retrieve tags already applied on the given file)
	wsprintf(buff, L"%s query \"%s\"", Settings.taggerCommandLinePath, oldFileName);		
	output = DosExec(buff);
	appendLog(ID_LOG_TAGGER, buff, true);
	appendLog(ID_LOG_TAGGER, output);

	if(wcscmp(output, L"No tag currently applied on given file(s).\r\n") != 0) {
		LocalFree(output);
		// given file is in the tagger DB
		wsprintf(buff, L"%s --files rename \"%s\" \"%s\"", Settings.taggerCommandLinePath, oldFileName, newFileName);
		output = DosExec(buff);
		appendLog(ID_LOG_TAGGER, buff, true);
		appendLog(ID_LOG_TAGGER, output);
	}

	LocalFree(output);
}

void fileRemove(HWND hWnd, WPARAM wParam, LPARAM lParam) {
	static WCHAR buff[4192];
	LPWSTR output;
	LPWSTR oldFileName = (LPWSTR) wParam;
	LPWSTR newFileName = (LPWSTR) lParam;

	if(oldFileName == NULL) return;

	appendLog(ID_LOG_FS, L"File deleted:");
	wsprintf(buff, L"    %s", oldFileName);
	appendLog(ID_LOG_FS, buff);
	appendLog(ID_LOG_FS, L"");

	// note : as file is now deleted, we cannot ask windows file's attributes !!


// todo : try to remove all files starting with given path
/*
		wsprintf(buff, L"%s --quiet --files list \"%s\\*\"", Settings.taggerCommandLinePath, oldFileName);
		output = DosExec(buff);
		appendLog(ID_LOG_TAGGER, buff, true);
		appendLog(ID_LOG_TAGGER, output);
		for(LPWSTR line = wcstok(output, L"\n"); line; line = wcstok(NULL, L"\n")) {
			// for each line, replace oldFileName by newFileName
			line[wcslen(line)-1] = '\0';			
			wsprintf(buff, L"%s --files rename \"%s\" \"%s\\%s\"", Settings.taggerCommandLinePath, line, newFileName, line+wcslen(oldFileName)+1);
			output = DosExec(buff);
			appendLog(ID_LOG_TAGGER, buff, true);
			appendLog(ID_LOG_TAGGER, output);
		}
*/
	// check file's presence in database (retrieve tags already applied on the given file)
	wsprintf(buff, L"%s query \"%s\"", Settings.taggerCommandLinePath, oldFileName);		
	output = DosExec(buff);
	appendLog(ID_LOG_TAGGER, buff, true);
	appendLog(ID_LOG_TAGGER, output);

	if(wcscmp(output, L"No tag currently applied on given file(s).\r\n") != 0) {
		LocalFree(output);
		// given file is in the tagger DB
		wsprintf(buff, L"%s --files delete \"%s\"", Settings.taggerCommandLinePath, oldFileName);
		output = DosExec(buff);
		appendLog(ID_LOG_TAGGER, buff, true);
		appendLog(ID_LOG_TAGGER, output);
	}
	LocalFree(output);
}

void fileRestore(HWND hWnd, WPARAM wParam, LPARAM lParam) {
	static WCHAR buff[4192];
	LPWSTR output;
	LPWSTR oldFileName = (LPWSTR) wParam;
	LPWSTR newFileName = (LPWSTR) lParam;

	if(oldFileName == NULL) return;

	appendLog(ID_LOG_FS, L"File restored:");
	wsprintf(buff, L"    %s", oldFileName);
	appendLog(ID_LOG_FS, buff);
	appendLog(ID_LOG_FS, L"");

	// check if given file is actually non-existent
// todo : most files will be non-existent in the DB, and we shouldn't try to recover them
// add a feature to tagger.exe to check inside trash only
	wsprintf(buff, L"%s query \"%s\"", Settings.taggerCommandLinePath, oldFileName);
	output = DosExec(buff);
	appendLog(ID_LOG_TAGGER, buff, true);
	appendLog(ID_LOG_TAGGER, output);

	if(wcscmp(output, L"No tag currently applied on given file(s).\r\n") == 0) {
		LocalFree(output);
		// given file is in the tagger DB
		wsprintf(buff, L"%s --files recover \"%s\"", Settings.taggerCommandLinePath, oldFileName);
		output = DosExec(buff);
		appendLog(ID_LOG_TAGGER, buff, true);
		appendLog(ID_LOG_TAGGER, output);
	}
	LocalFree(output);
}

void watcherStopped(HWND hWnd, WPARAM wParam, LPARAM lParam) {
	MessageBox(NULL, L"Watcher thread stopped unexpectedly\r\nPlease, try to restart the application.", L"Error", MB_OK);
}

void notifyIcon(HWND hWnd, WPARAM wParam, LPARAM lParam) {	
	HMENU hMenu = GetSubMenu( LoadMenu(GetModuleHandle(NULL), MAKEINTRESOURCE(ID_POPUP_MENU)), 0);
	if ((UINT) lParam == WM_LBUTTONDOWN) {
		SendMessage(hWnd, WM_COMMAND, IDD_DIALOG_ACTIVITY, 0);
	}
	else if ((UINT) lParam == WM_RBUTTONDOWN) {
		POINT mPos;
		GetCursorPos(&mPos);
		TrackPopupMenuEx(hMenu, TPM_TOPALIGN | TPM_LEFTALIGN, mPos.x, mPos.y, hWnd, NULL); 
	}
}

void menuActivityLog(HWND hWnd, WPARAM wParam, LPARAM lParam) {	
	ShowWindow(hWndActivity, SW_SHOWNORMAL);
	BringWindowToTop(hWndActivity);
}

void menuSettings(HWND hWnd, WPARAM wParam, LPARAM lParam) {	
	// ShowWindow(hWndSettings, SW_SHOWNORMAL);
	// BringWindowToTop(hWndSettings);
}

void menuAbout(HWND hWnd, WPARAM wParam, LPARAM lParam) {	
	ShowWindow(hWndAbout, SW_SHOWNORMAL);
	BringWindowToTop(hWndAbout);
}

void menuRestart(HWND hWnd, WPARAM wParam, LPARAM lParam) {	

	FSChangeNotifier* lpNotifier = FSChangeNotifier::GetInstance();
	// stop watching thread
	lpNotifier->Stop();

	// empty all logs
	DlgCtrl_SendMessage(hWndActivity, ID_LOG_APP, LB_RESETCONTENT, 0, 0 );
	DlgCtrl_SendMessage(hWndActivity, ID_LOG_FS, LB_RESETCONTENT, 0, 0 );	
	DlgCtrl_SendMessage(hWndActivity, ID_LOG_TAGGER, WM_SETTEXT, 0, (LPARAM) (LPWSTR) L"");		

	// restart monitoring
	StartMonitoring();
}

void closeDialog(HWND hWnd, WPARAM, LPARAM) {
	ShowWindow(hWnd, SW_HIDE);
}


void closeApp(HWND hWnd, WPARAM, LPARAM) {
	if(MessageBox(hWnd, L"Terminating this program means that filesystem changes will no longer be monitored.\r\n This might result in Tagger database inconsistency (if tagged files are moved, deleted or restored).\r\n\r\nAre you sure you want to end monitoring ?", L"TaggerUI", MB_YESNO | MB_ICONWARNING | MB_DEFBUTTON2) == IDYES) {

		// free allocated memory
		if(Settings.taggerCommandLinePath) LocalFree(Settings.taggerCommandLinePath);
				
		for(UINT i = 0; i < Settings.nDrives; ++i) {
			LocalFree(Settings.lpDrivesInfos[i]);
		}
		LocalFree(Settings.lpDrivesInfos);

		NOTIFYICONDATA tnid;  
		tnid.cbSize = sizeof(NOTIFYICONDATA); 
		tnid.hWnd = hWnd; 
		tnid.uID = 0;         
		Shell_NotifyIcon(NIM_DELETE, &tnid); 

		DestroyWindow(hWndActivity);
		//DestroyWindow(hWndSettings);
		DestroyWindow(hWndAbout);
		DestroyWindow(hWnd);

		PostQuitMessage(0);
	}
}

void appendLog(UINT type, LPCWSTR str, BOOL isCommand) {
	if(!str) return;
	switch(type) {
	case ID_LOG_FS:
	case ID_LOG_APP:
		if(isCommand) DlgCtrl_SendMessage(hWndActivity, type, LB_ADDSTRING, 0, (LPARAM) L"");
		DlgCtrl_SendMessage(hWndActivity, type, LB_ADDSTRING, 0, (LPARAM) str);
		break;
	case ID_LOG_TAGGER:
		if(isCommand) DlgCtrl_SendMessage(hWndActivity, type, EM_REPLACESEL, 0, (LPARAM) (LPWSTR) L"$>");
		DlgCtrl_SendMessage(hWndActivity, type, EM_REPLACESEL, 0, (LPARAM) (LPWSTR) str);
		DlgCtrl_SendMessage(hWndActivity, type, EM_REPLACESEL, 0, (LPARAM) (LPWSTR) L"\r\n");
		break;
	}
}