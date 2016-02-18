/* registry.h - interface for accessing win registry

    This file is part of the tagger-ui suite <http://www.github.com/cedricfrancoys/tagger-ui>
    Copyright (C) Cedric Francoys, 2016, Yegen
    Some Right Reserved, GNU GPL 3 license <http://www.gnu.org/licenses/>
*/


#ifndef __REGISTRY_H
#define __REGISTRY_H 1


LPVOID	Registry_Read(HKEY hKey, LPCWSTR lpSubKey, LPCWSTR lpValueName);
BOOL	Registry_Write(HKEY hKey, LPCWSTR lpSubKey, LPCWSTR lpValueName, DWORD dwType, LPCBYTE lpData);


#endif