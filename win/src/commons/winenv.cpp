/* winenv.cpp - interface for retrieving Windows environment properies

    This file is part of the tagger-ui suite <http://www.github.com/cedricfrancoys/tagger-ui>
    Copyright (C) Cedric Francoys, 2016, Yegen
    Some Right Reserved, GNU GPL 3 license <http://www.gnu.org/licenses/>
*/

#include <windows.h>
#include <shlobj.h>
#include <sddl.h>

#include <stdio.h>

#include "registry.h"
#include "winenv.h"

/* Retrieve information about current Windows installation

Returned value is a pointer to a WINDOWSINFO struct

typedef struct {
	WCHAR szVersion[5];
	WCHAR szName[64];
	WCHAR szBuild[5];
	WCHAR szDirectory[128];
	WCHAR szServicePack[16];
} WINDOWS_INFO, WINDOWSINFO;

Windows versions memo:
    Windows 10                      10.0
    Windows Server 2016             10.0
    Windows 8.1                     6.3
    Windows Server 2012 R2          6.3
    Windows 8                       6.2
    Windows Server 2012             6.2
    Windows 7                       6.1
    Windows Server 2008 R2          6.1
    Windows Server 2008             6.0
    Windows Vista                   6.0
    Windows Server 2003 R2          5.2
    Windows Server 2003             5.2
    Windows XP 64-Bit Edition       5.2
    Windows XP                      5.1
    Windows 2000                    5.0
*/
LPWINDOWSINFO WinEnv_GetWindowsInfo() {
	static WINDOWSINFO winInf = {0};

	if(!winInf.szVersion[0]) {
		LPWSTR szBuff;

		if(szBuff = (LPWSTR) Registry_Read(HKEY_LOCAL_MACHINE, L"SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion", L"CurrentVersion")) {
			wcscpy(winInf.szVersion, szBuff);
			LocalFree(szBuff);
		}

		if(szBuff = (LPWSTR) Registry_Read(HKEY_LOCAL_MACHINE, L"SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion", L"ProductName")) {
			wcscpy(winInf.szName, szBuff);
			LocalFree(szBuff);
		}
    
		if(szBuff = (LPWSTR) Registry_Read(HKEY_LOCAL_MACHINE, L"SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion", L"CurrentBuildNumber")) {
			wcscpy(winInf.szBuild, szBuff);
			LocalFree(szBuff);
		}	
	
		if(szBuff = (LPWSTR) Registry_Read(HKEY_LOCAL_MACHINE, L"SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion", L"SystemRoot")) {
			wcscpy(winInf.szDirectory, szBuff);
			LocalFree(szBuff);    
		}

		if(szBuff = (LPWSTR) Registry_Read(HKEY_LOCAL_MACHINE, L"SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion", L"CSDVersion")) {
			wcscpy(winInf.szServicePack, szBuff);
			LocalFree(szBuff);
		}
	}
	return &winInf;
}


/* Retrieve the physical directory related to given folder identifier (as defined in shlobj.h)
 Note: Most of these directories can be found in HKEY_CURRENT_USER\Software\Microsoft\Windows\CurrentVersion\Explorer\Shell Folders

// sytem directories
CSIDL_FONTS	                    // (X):\Windows\Fonts
CSIDL_WINDOWS                   // (X):\Windows
CSIDL_SYSTEM                    // (X):\Windows\System
CSIDL_PROGRAM_FILES             // (X):\Program Files
CSIDL_SYSTEMX86                 // (X):\Windows\System32
CSIDL_PROGRAM_FILESX86          // (X):\Program Files (x86)

// all users
CSIDL_COMMON_DOCUMENTS          // All Users\Documents
CSIDL_COMMON_TEMPLATES          // All Users\Templates
CSIDL_COMMON_APPDATA            // All Users\Application Data
CSIDL_COMMON_STARTMENU          // All Users\Start Menu
CSIDL_COMMON_PROGRAMS           // All Users\Start Menu\Programs
CSIDL_COMMON_STARTUP            // All Users\Startup
CSIDL_COMMON_DESKTOPDIRECTORY   // All Users\Desktop
CSIDL_COMMON_MUSIC              // All Users\My Music
CSIDL_COMMON_PICTURES           // All Users\My Pictures
CSIDL_COMMON_VIDEO              // All Users\My Video

// current user profile
CSIDL_FAVORITES                 // <user profile>\Favorites
CSIDL_NETHOOD	                // <user profile>\Nethood
CSIDL_PRINTHOOD                 // <user profile>\Printhood
CSIDL_RECENT	                // <user profile>\Recent
CSIDL_SENDTO	                // <user profile>\SendTo
CSIDL_STARTMENU	                // <user profile>\Start Menu
CSIDL_PROGRAMS	                // <user profile>\Start Menu\Programs
CSIDL_STARTUP	                // <user profile>\Start Menu\Programs\Startup
CSIDL_DESKTOPDIRECTORY          // <user profile>\Desktop
CSIDL_MYDOCUMENTS               // <user profile>\My Documents
CSIDL_MYMUSIC                   // <user profile>\My Music
CSIDL_MYVIDEO                   // <user profile>\My Video
CSIDL_MYPICTURES                // <user profile>\My Pictures
CSIDL_APPDATA                   // <user profile>\Application Data
CSIDL_LOCAL_APPDATA             // <user profile>\Local Settings\Applicaiton Data
*/
#define CSIDL_TEMP                 0x000c // (X):\Windows\Temp
#define CSIDL_MYTEMP               0x000f // (X):\<user profile>\Local Settings\Temp

LPWSTR WinEnv_GetFolderPath(UINT nFolderID) {
    LPWSTR result = NULL;
	
	if(nFolderID == CSIDL_MYTEMP) {
		WCHAR buff[1024];
		LPWSTR result = (LPWSTR) LocalAlloc(LPTR, sizeof(WCHAR)*1024);
		GetTempPath(1024, buff);
		GetLongPathName(buff, result, 1024);
		return result;
	}
	if(nFolderID == CSIDL_TEMP) {
		LPWSTR result = (LPWSTR) LocalAlloc(LPTR, sizeof(WCHAR)*1024);
		LPWINDOWSINFO lpWinInf = WinEnv_GetWindowsInfo();
		wsprintf(result, L"%s\\Temp", lpWinInf->szDirectory);
		return result;
	}

	LPITEMIDLIST pidl;
	LPMALLOC g_pMalloc;
	SHGetMalloc(&g_pMalloc);
	if(SHGetSpecialFolderLocation(NULL, nFolderID, &pidl) == NOERROR) {
		LPWSTR pszPath = (LPWSTR) g_pMalloc->Alloc(MAX_PATH);
		if(SHGetPathFromIDListW(pidl, pszPath)) {
			result = (LPWSTR) LocalAlloc(LPTR, sizeof(WCHAR)*(wcslen(pszPath)+1));
            wcscpy(result, pszPath);
		}
		g_pMalloc->Free(pidl); 
		g_pMalloc->Free(pszPath);
	}
    return result;
}

/*  Retrieve information about current user

Returned value is a pointer to a CURRENTUSERINFO struct

typedef struct {
	WCHAR szName[64];
	WCHAR szSid[64];
	WCHAR szHomeDirectory[256];

} CURRENTUSER_INFO, CURRENTUSERINFO;
*/
LPCURRENTUSERINFO WinEnv_GetCurrentUserInfo() {
	static CURRENTUSERINFO currentUserInfo = {0};

	if(!currentUserInfo.szName[0]) {
		// retrieve user name
		DWORD nSize = 1024;
		LPWSTR szBuff = (LPWSTR) LocalAlloc(LPTR, sizeof(WCHAR)*nSize);
		if(GetUserName(szBuff, &nSize)) {
			wcscpy(currentUserInfo.szName, szBuff);
		}
		LocalFree(szBuff);

		// retrieve home directory
		LPWSTR szReg = (LPWSTR) Registry_Read(HKEY_CURRENT_USER, L"Environment", L"HOME");
		if(szReg) {
			wcscpy(currentUserInfo.szHomeDirectory, szReg);
			LocalFree(szReg);
		}

		// retrieve user SID string (ex. S-1-5-xx-xxxxxxxxx-xxxxxxxxxx-xxxxxxxxxx-xxxx)
		HANDLE hToken;
		if(OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &hToken)) {
			DWORD dwSize = 0;
			GetTokenInformation(hToken, TOKEN_INFORMATION_CLASS::TokenUser, NULL, 0, &dwSize);
			if(dwSize > 0) {
				PTOKEN_USER ptu = (PTOKEN_USER) LocalAlloc(LPTR, dwSize);
				if(GetTokenInformation(hToken, TOKEN_INFORMATION_CLASS::TokenUser, ptu, dwSize, &dwSize)) {
					LPWSTR result = NULL;
					ConvertSidToStringSidW(ptu->User.Sid, &result);
					wcscpy((LPWSTR) currentUserInfo.szSid, result);
					LocalFree(result);
				}
				LocalFree((HLOCAL) ptu);
			}
			CloseHandle(hToken);
		}
	}
	return &currentUserInfo;
}


/* Return an array of installed drives matching givn type.
Note: names include double-point and trailing backslash
nType must be a DRIVE_* type as defined in winbase.h :
 DRIVE_REMOVABLE, DRIVE_FIXED, DRIVE_REMOTE, DRIVE_CDROM, DRIVE_RAMDISK
*/
LPWSTR* WinEnv_GetDrives(UINT nType) {
	LPWSTR* result = (LPWSTR*) LocalAlloc(LPTR, sizeof(LPWSTR)*26);
	DWORD dwLogicalDrives = GetLogicalDrives();
	UINT nPos = 0;
	for(CHAR cDrive = 'A'; cDrive <= 'Z'; ++cDrive) {
		if(dwLogicalDrives & 1) {
			result[nPos] = (LPWSTR) LocalAlloc(LPTR, sizeof(WCHAR)*4);
			wsprintf(result[nPos], L"%c:\\", cDrive);
			if(GetDriveType(result[nPos]) != nType) {
				LocalFree(result[nPos]);
				result[nPos] = NULL;
			}
			else {
				++nPos;
			}
		}
		dwLogicalDrives >>= 1;
	}
	result[nPos] = NULL;
	return result;
}

LPDRIVEINFO WinEnv_GetDriveInfo(LPWSTR szDrive) {
	LPDRIVEINFO lpdi = (LPDRIVEINFO) LocalAlloc(LPTR, sizeof(DRIVEINFO));
	if(!GetVolumeInformation(szDrive,
                            lpdi->szVolumeName,
                            sizeof(lpdi->szVolumeName),
                            &lpdi->dwSerialNumber,
                            &lpdi->dwMaxFileNameLength,
                            &lpdi->dwFileSystemFlags,
                            lpdi->szFileSystem,
                            sizeof(lpdi->szFileSystem))) {
		LocalFree(lpdi);
		lpdi = NULL;
	}
	else {
		LPWINDOWSINFO lpWinInf = WinEnv_GetWindowsInfo();
		FLOAT winVersion;
		if(swscanf(lpWinInf->szVersion, L"%f", &winVersion)) {
			if(winVersion < 6.0) {
				if(wcscmp(lpdi->szFileSystem, L"NTFS") == 0) {
					wcscpy(lpdi->szRecycleBinPath, szDrive);
					wcscat(lpdi->szRecycleBinPath, L"RECYCLER\\");
				}
				else {
					wcscpy(lpdi->szRecycleBinPath, szDrive);
					wcscat(lpdi->szRecycleBinPath, L"RECYCLED\\");
				}
			}
			else {
				wcscpy(lpdi->szRecycleBinPath, szDrive);
				wcscat(lpdi->szRecycleBinPath, L"$RECYCLE.BIN\\");            
			}
			if(wcscmp(lpdi->szFileSystem, L"NTFS") == 0) {
				LPCURRENTUSERINFO lpCurrentUserInfo = WinEnv_GetCurrentUserInfo();
				wcscat(lpdi->szRecycleBinPath, lpCurrentUserInfo->szSid);
				wcscat(lpdi->szRecycleBinPath, L"\\");
			}            
		}
	}
	return lpdi;
}



/*
LPWSTR WinEnv_GetCurrentUserName() {
    DWORD nSize = 1024;
    LPWSTR result = NULL;
    LPWSTR temp = (LPWSTR) LocalAlloc(LPTR, sizeof(WCHAR)*nSize);
    if(GetUserName(temp, &nSize)) {
        result = (LPWSTR) LocalAlloc(LPTR, sizeof(WCHAR)*(nSize+1));
        wcscpy(result, temp);
    }
    LocalFree(temp);
    return result;
}
*/

/*
LPWSTR WinEnv_GetCurrentUserHome() {
   return (LPWSTR) Registry_Read(HKEY_CURRENT_USER, L"Environement", L"HOME"); 
}
*/

/*
LPWSTR WinEnv_GetCurrentUserSid() {
	HANDLE hToken;
	LPWSTR result = NULL;
	if(OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &hToken)) {
		DWORD dwSize = 0;
		GetTokenInformation(hToken, TOKEN_INFORMATION_CLASS::TokenUser, NULL, 0, &dwSize);
		if(dwSize > 0) {
			PTOKEN_USER ptu = (PTOKEN_USER) LocalAlloc(LPTR, dwSize);
			if(GetTokenInformation(hToken, TOKEN_INFORMATION_CLASS::TokenUser, ptu, dwSize, &dwSize)) {
				ConvertSidToStringSidW(ptu->User.Sid, &result);
			}
			LocalFree((HLOCAL) ptu);
		}
        CloseHandle(hToken);
	}
    return result;
}


LPWSTR WinEnv_GetRecycleBinPath(LPWSTR szDrive) {
    LPWSTR result = NULL;
	LPWINDOWSINFO lpWinInf = WinEnv_GetWindowsInfo();
    FLOAT winVersion;
    if(swscanf(lpWinInf->szVersion, L"%f", &winVersion)) {
        LPDRIVEINFO lpdi = WinEnv_GetDriveInfo(szDrive);
        result = (LPWSTR) LocalAlloc(LPTR, sizeof(WCHAR)*64);
        if(winVersion < 6.0) {
            if(wcscmp(lpdi->szFileSystem, L"NTFS") == 0) {
                wcscpy(result, szDrive);
                wcscat(result, L"RECYCLER\\");
            }
            else {
                wcscpy(result, szDrive);
                wcscat(result, L"RECYCLED\\");
            }
        }
        else {
            wcscpy(result, szDrive);
            wcscat(result, L"$RECYCLE.BIN\\");            
        }
        if(wcscmp(lpdi->szFileSystem, L"NTFS") == 0) {
			LPWSTR szUserSid = WinEnv_GetCurrentUserSid();
			if(szUserSid) {
				wcscat(result, szUserSid);
				wcscat(result, L"\\");
				LocalFree(szUserSid);
			}
        }            
    }
    return result;
}
*/