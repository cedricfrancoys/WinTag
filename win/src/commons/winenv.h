/* winenv.h - interface for retrieving Windows environment properies

    This file is part of the tagger-ui suite <http://www.github.com/cedricfrancoys/tagger-ui>
    Copyright (C) Cedric Francoys, 2016, Yegen
    Some Right Reserved, GNU GPL 3 license <http://www.gnu.org/licenses/>
*/


#ifndef __WINENV_H
#define __WINENV_H 1


typedef struct {
	WCHAR szDrive[32];
	WCHAR szVolumeName[64];
	WCHAR szFileSystem[6];		// HPFS, CDFS, FAT, FAT32, NTFS, CsvFS, ReFS
	WCHAR szRecycleBinPath[64];
	DWORD dwSerialNumber;
	DWORD dwFileSystemFlags;
	DWORD dwMaxFileNameLength;
} DRIVE_INFO, DRIVEINFO;

typedef DRIVEINFO* LPDRIVEINFO;


typedef struct {
	WCHAR szVersion[5];
	WCHAR szName[64];
	WCHAR szBuild[5];
	WCHAR szDirectory[128];
	WCHAR szServicePack[16];
} WINDOWS_INFO, WINDOWSINFO;

typedef WINDOWSINFO *LPWINDOWSINFO;


typedef struct {
	WCHAR szName[64];
	WCHAR szSid[64];
	WCHAR szHomeDirectory[256];

} CURRENTUSER_INFO, CURRENTUSERINFO;

typedef CURRENTUSERINFO *LPCURRENTUSERINFO;

// adding cutom constants to CSIDL_* defined in shlobj.h
#define CSIDL_TEMP                 0x000c // (X):\Windows\Temp
#define CSIDL_MYTEMP               0x000f // (X):\<user profile>\Local Settings\Temp

LPWINDOWSINFO WinEnv_GetWindowsInfo();

LPCURRENTUSERINFO WinEnv_GetCurrentUserInfo();

LPDRIVEINFO WinEnv_GetDriveInfo(LPWSTR szDriveLetter);

/* Retrieve the physical directory related to given folder identifier (as defined in shlobj.h)
*/
LPWSTR WinEnv_GetFolderPath(UINT nFolderID);

/* Return an array of installed drives matching given type.
*/
LPWSTR* WinEnv_GetDrives(UINT nType);



#endif