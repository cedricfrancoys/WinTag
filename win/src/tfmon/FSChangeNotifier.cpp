/* FSChangeNotifier.cpp - interface for monitoring File System changes using ReadDirectoryChangesW
				  
    This file is part of the tagger-ui suite <http://www.github.com/cedricfrancoys/tagger-ui>
    Copyright (C) Cedric Francoys, 2016, Yegen
    Some Right Reserved, GNU GPL 3 license <http://www.gnu.org/licenses/>
	
	Part of this code is based on the class CFileSysMon, defined in FileSysMon.cpp originally written by H. Seldon (hseldon@veridium.net), December 2010 <http://veridium.net>
*/


#include <windows.h>
#include <stdlib.h>
#include "FSChangeNotifier.h"

DWORD WM_FSNOTIFY_ADDED = RegisterWindowMessage(L"FSChangeNotifierAdd");
DWORD WM_FSNOTIFY_MOVED = RegisterWindowMessage(L"FSChangeNotifierMove");
DWORD WM_FSNOTIFY_REMOVED = RegisterWindowMessage(L"FSChangeNotifierRemove");
DWORD WM_FSNOTIFY_RESTORED = RegisterWindowMessage(L"FSChangeNotifierRestore");

FSChangeNotifier::FSChangeNotifier() {	
	this->hIOCP = NULL;
	memset(this->drives, 0, 27);
}

FSChangeNotifier* FSChangeNotifier::GetInstance() {
	static FSChangeNotifier fsInst;
	return &fsInst;
}

FSChangeNotifier::~FSChangeNotifier() {
	RemoveAllPaths();
	if (this->hIOCP) CloseHandle(this->hIOCP);
}

/*
BOOL FSChangeNotifier::AddPrivilege(LPCTSTR pszPrivName, BOOL bEnable) {
	BOOL result = FALSE;
	HANDLE hToken = NULL;

	if (OpenProcessToken(GetCurrentProcess(), TOKEN_ADJUST_PRIVILEGES, &hToken)) {
		TOKEN_PRIVILEGES tp = { 1 };        
		if (LookupPrivilegeValue(NULL, pszPrivName,  &tp.Privileges[0].Luid)) {
			tp.Privileges[0].Attributes = bEnable ?  SE_PRIVILEGE_ENABLED : 0;
			AdjustTokenPrivileges(hToken, FALSE, &tp, sizeof(tp), NULL, NULL);
			result = (GetLastError() == ERROR_SUCCESS);		
		}
		CloseHandle(hToken);
	} 
	return result;
}
*/

BOOL FSChangeNotifier::Init() {
	HANDLE hToken = NULL;
	// adjust privileges for current process
	if (OpenProcessToken(GetCurrentProcess(), TOKEN_ADJUST_PRIVILEGES, &hToken)) {
		TOKEN_PRIVILEGES tp = { 1 }; 
// do we need this ?
		if (LookupPrivilegeValue(NULL, SE_BACKUP_NAME,  &tp.Privileges[0].Luid)) {
			tp.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;
			AdjustTokenPrivileges(hToken, FALSE, &tp, sizeof(tp), NULL, NULL);
			if(GetLastError() != ERROR_SUCCESS) {
				this->nLastError = GetLastError();
				CloseHandle(hToken);
				return FALSE;
			}
		}
// do we need this ?
		if (LookupPrivilegeValue(NULL, SE_RESTORE_NAME,  &tp.Privileges[0].Luid)) {
			tp.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;
			AdjustTokenPrivileges(hToken, FALSE, &tp, sizeof(tp), NULL, NULL);
			if(GetLastError() != ERROR_SUCCESS) {
				this->nLastError = GetLastError();
				CloseHandle(hToken);
				return FALSE;
			}
		}
		// we need this one enabled  (enabled by default for all user since win NT)
		if (LookupPrivilegeValue(NULL, SE_CHANGE_NOTIFY_NAME,  &tp.Privileges[0].Luid)) {
			tp.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;
			AdjustTokenPrivileges(hToken, FALSE, &tp, sizeof(tp), NULL, NULL);
			if(GetLastError() != ERROR_SUCCESS) {
				this->nLastError = GetLastError();
				CloseHandle(hToken);
				return FALSE;
			}
		}
		CloseHandle(hToken);
	} 


	// Get a handle to the I/O completion port (limit threading to one instance)
	if (!(this->hIOCP = CreateIoCompletionPort(	(HANDLE) INVALID_HANDLE_VALUE, NULL, 0, 1))) {
		this->nLastError = GetLastError();
		return FALSE;
	}

	return TRUE;
}

void FSChangeNotifier::bind(HWND hWnd) {
	vechWndDest.push_back(hWnd);
}

BOOL FSChangeNotifier::Start() {
	// start monitoring thread
	this->hThread = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE) FSChangeNotifier::ThreadWatch, NULL, 0, NULL);
	return TRUE;
}

BOOL FSChangeNotifier::Stop() {
	CloseHandle(this->hThread);
	return TRUE;
}

INT FSChangeNotifier::AddPath(LPCWSTR pPath, BOOL bSubTree) {
	// check IO completion port handler
	if (!this->hIOCP) {
			// must call Init() method first !
			return E_FILESYSMON_ERRORNOTINIT;
	}

	// update drives list
	BOOL bPresent = FALSE;
	UINT i, uiCount;
	for(i = 0, uiCount = strlen(this->drives); !bPresent && i < uiCount; ++i) {
		if(this->drives[i] == pPath[0]) bPresent = TRUE;
	} if(!bPresent) this->drives[i] = pPath[0];


	// Open handle to the directory to be monitored
	// flag FILE_FLAG_OVERLAPPED allows asynchronous calls of ReadDirectoryChangesW
	DirInfo* pDir = new DirInfo(pPath, bSubTree);
	if ((pDir->hFile = CreateFile(pPath, FILE_LIST_DIRECTORY, FILE_SHARE_READ | FILE_SHARE_DELETE | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OVERLAPPED, NULL)) == INVALID_HANDLE_VALUE) {
		this->nLastError = GetLastError();
		delete pDir;
		return E_FILESYSMON_ERROROPENFILE;
	}

	// Allocate notification buffers (will be filled by the system when a notification occurs)
	memset(&pDir->ol,  0, sizeof(pDir->ol));
	if((pDir->pBuff = new FILE_NOTIFY_INFORMATION[MAX_BUFF_SIZE]) == NULL) {
		CloseHandle(pDir->hFile);
		delete pDir;
		return E_FILESYSMON_ERROROUTOFMEM;
	}

	// Associate directory handle with the IO completion port
	if (CreateIoCompletionPort(pDir->hFile, this->hIOCP, (ULONG_PTR) pDir->hFile, 0) == NULL) {
		this->nLastError = GetLastError();
		CloseHandle(pDir->hFile);
		delete pDir;
		return E_FILESYSMON_ERRORADDTOIOCP;
	}

	// Start monitoring for changes
	DWORD dwBytesReturned;
	if (!ReadDirectoryChangesW(pDir->hFile, pDir->pBuff, MAX_BUFF_SIZE * sizeof(FILE_NOTIFY_INFORMATION), bSubTree, FILE_NOTIFY_CHANGE_DIR_NAME | FILE_NOTIFY_CHANGE_FILE_NAME, &dwBytesReturned, &pDir->ol, NULL)) {
		this->nLastError = GetLastError();
		CloseHandle(pDir->hFile);
		delete pDir;	
		return E_FILESYSMON_ERRORREADDIR;
	}

	// add direcory to watched directories queue	
	this->vecDirs.push_back(pDir);

	return E_FILESYSMON_SUCCESS;
}

INT FSChangeNotifier::AddExclusion(LPCWSTR pPath) {
	WCHAR temp[FILE_NAME_MAX];
	GetShortPathName(pPath, temp, FILE_NAME_MAX);

	this->vecExclusions.push_back(pPath);
	this->vecExclusions.push_back(temp);

	return E_FILESYSMON_SUCCESS;
}

void FSChangeNotifier::RemovePath(UINT nIndex) {
	//sanity check
	if (nIndex >= (int) this->vecDirs.size() || this->vecDirs.empty()) {
		return;
	}

	if(this->vecDirs[nIndex]) {
		if(this->vecDirs[nIndex]->hFile) {
			CancelIo(this->vecDirs[nIndex]->hFile);
//			CloseHandle(this->vecDirs[nIndex]->hFile);
		}
		delete this->vecDirs[nIndex];
	}

	this->vecDirs.erase(this->vecDirs.begin() + nIndex);
}


void FSChangeNotifier::RemoveAllPaths() {
	for (int i = 0, uiCount = this->vecDirs.size(); i < uiCount; ++i) {
		this->RemovePath(0);
	}
}


vector<FileActionInfo*>* FSChangeNotifier::FetchChanges() {
	static vector<FileActionInfo*> vecChanges;
	DWORD		dwBytesXFered = 0;
	ULONG_PTR	ulKey = 0;
	OVERLAPPED*	pOl;
	
	// reset queue
	vecChanges.clear();

	// get new completion key (ulKey) or wait for timeout 
	if (!GetQueuedCompletionStatus(this->hIOCP, &dwBytesXFered, &ulKey, &pOl, INFINITE)) {
		if (GetLastError() == WAIT_TIMEOUT)
			this->nLastError = E_FILESYSMON_NOCHANGE;
		else this->nLastError = E_FILESYSMON_ERRORDEQUE;
		return NULL;
	}

	// identify which watched directory has been changed (ulKey should be a DirInfo handler)
	DirInfo* pDir;	
	int nIndex, uiCount = this->vecDirs.size();
	for (nIndex = 0; nIndex < uiCount; ++nIndex) {
		pDir = this->vecDirs.at(nIndex);
		if (ulKey == (ULONG_PTR) pDir->hFile)
			break;
	}
	// directory not found in queue: this->vecDirs and directories submitted to IOCompletionPort are no longer synched !
	if (nIndex == uiCount) {
		this->nLastError = E_FILESYSMON_ERRORUNKNOWN;
		return NULL;
	}
	
	// pDir->pBuff holds latest IO operations 
	FILE_NOTIFY_INFORMATION* pIter = pDir->pBuff;
	while (pIter) {
		// retrieve file full-path
		WCHAR tempPath[FILE_NAME_MAX];
		memset(tempPath, 0, sizeof(WCHAR)*FILE_NAME_MAX);
		wcscpy(tempPath, pDir->dirPath);
		memcpy(tempPath+wcslen(tempPath), pIter->FileName, pIter->FileNameLength);

		// queue new change
		vecChanges.push_back(new FileActionInfo(tempPath, pIter->Action));

		if(pIter->NextEntryOffset == 0UL) break;	

//		if ((DWORD)((BYTE*)pIter - (BYTE*)pDir->pBuff) > (MAX_BUFF_SIZE * sizeof(FILE_NOTIFY_INFORMATION)))
//			pIter = pDir->pBuff;

		pIter = (PFILE_NOTIFY_INFORMATION) ((LPBYTE)pIter + pIter->NextEntryOffset);
	 }

	// re-register current directory for receiving further changes
	DWORD dwBytesReturned = 0;
	if (!ReadDirectoryChangesW(pDir->hFile, pDir->pBuff, MAX_BUFF_SIZE * sizeof(FILE_NOTIFY_INFORMATION), pDir->bSubTree, FILE_NOTIFY_CHANGE_DIR_NAME | FILE_NOTIFY_CHANGE_FILE_NAME, &dwBytesReturned, &pDir->ol, NULL)) {
		this->nLastError = E_FILESYSMON_ERRORREADDIR;
		RemovePath(nIndex);
		return NULL;
	}

	return &vecChanges;
}


void FSChangeNotifier::Notify(DWORD action, LPWSTR oldFileName, LPWSTR newFileName) {
	for(int i = 0, j = vechWndDest.size(); i < j; ++i) {
		SendMessage(vechWndDest[i], action, (WPARAM) oldFileName, (LPARAM) newFileName);
	}
}

DWORD WINAPI FSChangeNotifier::DelayedRemoval(LPVOID lpvd) {
	FileActionInfo* lpAction = (FileActionInfo*) lpvd;
	FSChangeNotifier* fsChangeNotifier = FSChangeNotifier::GetInstance();
		
	Sleep(2000);	 

	if(fsChangeNotifier->changesQueue.Search(lpAction)) {
		// if 'removed' event is still in the queue, handle it as an actual removal
		fsChangeNotifier->Notify(WM_FSNOTIFY_REMOVED, lpAction->GetFilePath(), NULL);
		// remove event from the queue		
		fsChangeNotifier->changesQueue.Remove(lpAction);
	}

	return 0;
}

/*
This function uses FSChangeNotifier::FetchChanges to detect changes and calls FSChangeNotifier::Callback (pointer to user's custom function) passing action, file_old_name and file_new_name as parameters.
It is meant to be invoked as a thread routine.
Possible invoked actions are: 
- FILE_ACTION_ADDED		a file was created
- FILE_ACTION_REMOVED	a file was deleted
- FILE_ACTION_MOVED		a file was renamed or moved
- FILE_ACTION_RESTORED	a file was brought back from recylce bin

Things that could be improved:
- if two files with same filename are created during same session on different volumes and afteward one of them is deleted, this will erroneously be handled as a 'moved' event
- a 'moved' event between two distinct volumes will also generate an 'added' event
- with time, FILE_ACTION_INFO queue might grow big (because 'added' events are never deleted since they might be the beginning of a 'moved' event) : we could add a max delay for 'moved' events
*/
DWORD WINAPI FSChangeNotifier::ThreadWatch(LPVOID lpvd) {
	FSChangeNotifier* fsChangeNotifier = FSChangeNotifier::GetInstance();
	vector<FileActionInfo*>* vecChanges;
	FileActionInfo* lpAction;

	// main loop
	while ( (vecChanges = fsChangeNotifier->FetchChanges()) ) {
		for (UINT i = 0, uiCount = vecChanges->size(); i < uiCount; ++i)	{
			FileActionInfo* lpNewAction = vecChanges->at(i);
			FileActionInfo* lpLastAction = fsChangeNotifier->changesQueue.Last();

			// check for exclusion list
			BOOL exclusion = FALSE;
			for(UINT j = 0, uiSize = fsChangeNotifier->vecExclusions.size(); j < uiSize; ++j) {
				if(wcsstr(lpNewAction->GetFilePath(), fsChangeNotifier->vecExclusions.at(j).c_str())) {
					exclusion = TRUE;
					break;
				}				
			} if(exclusion) continue;

			switch(lpNewAction->GetAction()) {
			case FILE_ACTION_ADDED:					
					if(lpLastAction && lpLastAction->GetAction() == FILE_ACTION_REMOVED && lpLastAction->GetDrive() == lpNewAction->GetDrive() ) {
// todo : use previously retrieved recycle bin(s) exact path
						if(wcsstr(lpNewAction->GetFilePath(), L"RECYCLE")) {
							// deletion toward recycle bin: delayed removal will handle this
						}
						else if(wcsstr(lpLastAction->GetFilePath(), L"RECYCLE")) {
							// file restored
							fsChangeNotifier->Notify(WM_FSNOTIFY_RESTORED, lpNewAction->GetFilePath(), NULL);
							// remove 'added' event from queue
							fsChangeNotifier->changesQueue.Remove(lpLastAction);
						}
						else {
							if(wcscmp(lpLastAction->GetFileName(), lpNewAction->GetFileName()) == 0 ) {
								// file moved
								fsChangeNotifier->Notify(WM_FSNOTIFY_MOVED, lpLastAction->GetFilePath(), lpNewAction->GetFilePath());
								// remove 'added' event from queue
								fsChangeNotifier->changesQueue.Remove(lpLastAction);
							}
							else {
								// file added
								fsChangeNotifier->Notify(WM_FSNOTIFY_ADDED, NULL, lpNewAction->GetFilePath());
								// push 'added' event to queue
								fsChangeNotifier->changesQueue.Add(lpNewAction);
							}							
						}
					}
					else {
						// file added
						fsChangeNotifier->Notify(WM_FSNOTIFY_ADDED, NULL, lpNewAction->GetFilePath());
						// push 'added' event to queue
						fsChangeNotifier->changesQueue.Add(lpNewAction);
					}
				break;
			case FILE_ACTION_REMOVED:
				CHAR otherDrives[27];
				for(UINT j = 0, k = 0, uiSize = strlen(fsChangeNotifier->drives); j < uiSize; ++j) {
					if(fsChangeNotifier->drives[j] != lpNewAction->GetDrive()) {
						otherDrives[k] = fsChangeNotifier->drives[j];
						otherDrives[++k] = 0;
					}					
				}
				// search the queue for an 'added' event for the same filename on a different volume				
				if(lpAction = fsChangeNotifier->changesQueue.Search(lpNewAction->GetFilePath(), FILE_ACTION_ADDED, otherDrives)) {
					// file moved
					fsChangeNotifier->Notify(WM_FSNOTIFY_MOVED, lpNewAction->GetFilePath(), lpAction->GetFilePath());
					// remove 'removed' event from queue
					fsChangeNotifier->changesQueue.Remove(lpAction);
				}
				else {
					fsChangeNotifier->changesQueue.Add(lpNewAction);					
					CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE) DelayedRemoval, (LPVOID) lpNewAction, 0, NULL);
				}
				break;
			case FILE_ACTION_RENAMED_OLD_NAME:
				// push 'renamed' event to queue
				fsChangeNotifier->changesQueue.Add(lpNewAction);
				break;
			case FILE_ACTION_RENAMED_NEW_NAME:
				if(lpLastAction && lpLastAction->GetAction() == FILE_ACTION_RENAMED_OLD_NAME) {					
					fsChangeNotifier->Notify(WM_FSNOTIFY_MOVED, lpLastAction->GetFilePath(), lpNewAction->GetFilePath());
					fsChangeNotifier->changesQueue.Remove(lpLastAction);
				}				
				break;
			default:
				delete lpNewAction;
				break;
			}

		}
	}

	return 0;
}