/* FileSysMon.h - interface for monitoring File System changes using ReadDirectoryChangesW
	
	Code originally written by H. Seldon (hseldon@veridium.net), December 2010 <http://veridium.net>
 	Modified by Cedric Francoys, February 2016

    This file is part of the tagger-ui suite <http://www.github.com/cedricfrancoys/tagger-ui>
    Copyright (C) Cedric Francoys, 2016, Yegen
    Some Right Reserved, GNU GPL 3 license <http://www.gnu.org/licenses/>

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.
    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#pragma once
#include <Windows.h>
#include "FileActionInfo.h"
#include "FileActionQueue.h"

#include <vector>
using std::vector;

#ifndef FILE_NAME_MAX
	#define FILE_NAME_MAX 1024
#endif

#define MAX_BUFF_SIZE  100

// messages defined in FSChangeNotifier.cpp
extern DWORD WM_FSNOTIFY_ADDED;
extern DWORD WM_FSNOTIFY_MOVED;
extern DWORD WM_FSNOTIFY_REMOVED;
extern DWORD WM_FSNOTIFY_RESTORED;


class DirInfo {
public:
	LPWSTR						dirPath;
	OVERLAPPED					ol;
	HANDLE						hFile;
	BOOL						bSubTree;
	FILE_NOTIFY_INFORMATION*	pBuff;

	DirInfo(LPCWSTR dirPath, BOOL bSubTree = FALSE) {
		this->dirPath	= (LPWSTR) GlobalAlloc(GPTR, sizeof(WCHAR)*(wcslen(dirPath)+2));
		wcscpy(this->dirPath, dirPath);
		// add separator at the end of the path, if not present
		if(dirPath[wcslen(dirPath)-1] != '\\') wcscat(this->dirPath, L"\\");
		this->bSubTree	= bSubTree;
		this->pBuff		= NULL;
	}

	~DirInfo() {
		if (this->pBuff) delete[] this->pBuff;
		GlobalFree(this->dirPath);
	}
};




enum {
	E_FILESYSMON_SUCCESS,
	E_FILESYSMON_ERRORUNKNOWN,
	E_FILESYSMON_ERRORNOTINIT,
	E_FILESYSMON_ERROROUTOFMEM,
	E_FILESYSMON_ERROROPENFILE,
	E_FILESYSMON_ERRORADDTOIOCP,
	E_FILESYSMON_ERRORREADDIR,
	E_FILESYSMON_NOCHANGE,
	E_FILESYSMON_ERRORDEQUE
};

/* 
This class uses the Singleton pattern.
*/
class FSChangeNotifier {
private:
	FSChangeNotifier();

	HANDLE					hIOCP;
	HANDLE					hThread;
	vector<DirInfo*>		vecDirs;
	vector<wstring>			vecExclusions;
	vector<HWND>			vechWndDest;


	FileActionQueue			changesQueue;
	CHAR					drives[27];
	INT						nLastError;

	vector<FileActionInfo*>*FetchChanges();
	void					Notify(DWORD action, LPWSTR oldFileName, LPWSTR newFileName);
	static DWORD WINAPI		DelayedRemoval(LPVOID lpvd);
	static DWORD WINAPI		ThreadWatch(LPVOID lpvd);

public:
	static FSChangeNotifier* GetInstance();
	~FSChangeNotifier();
	
	/* 
	Bind a window so that it will receive messages when a change occurs.
	Message to be expected are: WM_FSNOTIFY_ADDED, WM_FSNOTIFY_MOVED, WM_FSNOTIFY_REMOVED, WM_FSNOTIFY_RESTORED
	*/
	void bind(HWND);

	BOOL Init();

	BOOL Start();
	BOOL Stop();

	INT AddPath(LPCWSTR pPath, BOOL bSubTree = true);
	INT AddExclusion(LPCWSTR pPath);

	void RemovePath(UINT nIndex); //zero based index
	void RemoveAllPaths();
};