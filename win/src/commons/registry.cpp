/* registry.cpp - interface for accessing win registry

    This file is part of the tagger-ui suite <http://www.github.com/cedricfrancoys/tagger-ui>
    Copyright (C) Cedric Francoys, 2016, Yegen
    Some Right Reserved, GNU GPL 3 license <http://www.gnu.org/licenses/>
*/

#include <windows.h>
#include "registry.h"

LPVOID Registry_Read(HKEY hKey, LPCWSTR lpSubKey, LPCWSTR lpValueName) {    
	HKEY hKeyResult;
    if (RegOpenKeyEx(hKey, lpSubKey, 0, KEY_READ, &hKeyResult) != ERROR_SUCCESS){
		return NULL;
    }
	DWORD type;
	// We allocate a fixed size buffer of 1024 bytes and don't deal with ERROR_MORE_DATA,
	// for a data field longer than 1ko is very unlikely
	DWORD size = 1024;
	LPBYTE data = (LPBYTE) LocalAlloc(LPTR, sizeof(BYTE) * size);
	if( RegQueryValueEx(hKeyResult, lpValueName, 0, &type, (LPBYTE) data, &size) != ERROR_SUCCESS ) {
		LocalFree(data);
		data = NULL;
	}
	RegCloseKey(hKeyResult);
	return (LPVOID) data;
}
		
BOOL Registry_Write(HKEY hKey, LPCWSTR lpSubKey, LPCWSTR lpValueName, DWORD dwType, LPCBYTE lpData) {
	BOOL result = TRUE;
	HKEY hKeyResult;
	DWORD dwDisposition;

	if (RegCreateKeyExW(hKey, lpSubKey, 0, NULL, REG_OPTION_NON_VOLATILE, KEY_WRITE, NULL, &hKeyResult, &dwDisposition) != ERROR_SUCCESS){
		return FALSE;
	}
	else{
		DWORD size;
		if ( dwType == REG_DWORD || dwType == REG_DWORD_LITTLE_ENDIAN || dwType == REG_DWORD_BIG_ENDIAN	 ) {
			size = sizeof(DWORD);
		}
		else if ( dwType == REG_SZ || dwType == REG_EXPAND_SZ){
			size = strlen((PCHAR) lpData);
		}
		if (RegSetValueExW(hKeyResult, lpValueName, 0, dwType, lpData, size) != ERROR_SUCCESS){
			result = FALSE;	
		}
	}
	RegCloseKey(hKeyResult);
	return result;
}