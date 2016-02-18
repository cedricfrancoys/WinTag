#pragma once

#include "FileActionInfo.h"

#include <string>
#include <cwchar>
#include <vector>
#include <map>

using std::wstring;
using std::vector;
using std::map;


/* Retrieve file name from a file path. 
If string does not contains backslashes (given string is not a path), 
then function returns NULL.
*/
/*
LPWSTR GetFileName(LPWSTR filePath) {
	LPWSTR result = wcsrchr(filePath, '\\');
	if(result) result += sizeof(WCHAR);
	return result;
}
*/

/*
There are two structures storing pointers to FileActionInfo items : a vector and a map. 
This is because we need to be able to:
1) quickly retrieve the latest added item
2) quickly search among all queued items (in which case we use fileName as hashcode)
*/
class FileActionQueue {
private:
	LPCRITICAL_SECTION criticalSection; 
	vector<FileActionInfo*> *qActionQueue;
	map<wstring, vector<FileActionInfo*>> *mActionMap;

	void Queue(vector<FileActionInfo*> *v, FileActionInfo* lpAction) {
		v->push_back(lpAction);
	}

	void Dequeue(vector<FileActionInfo*> *v, FileActionInfo* lpAction) {
		for(int i = 0, nCount = v->size(); i < nCount; ++i){
			if(lpAction == v->at(i)) {
				v->erase(v->begin() + i);
				break;
			}
		}
	}

public:
    FileActionQueue() {
        this->qActionQueue = new vector<FileActionInfo*>;
        this->mActionMap = new map<wstring, vector<FileActionInfo*>>;
		this->criticalSection = new CRITICAL_SECTION;
		InitializeCriticalSection(this->criticalSection);
    }
    
    ~FileActionQueue() {
        delete this->qActionQueue;
        delete this->mActionMap;
		DeleteCriticalSection(this->criticalSection);
		delete this->criticalSection;
    }

	void Add(FileActionInfo* lpAction) {
		EnterCriticalSection(this->criticalSection);
		this->Queue(&((*this->mActionMap)[lpAction->GetFileName()]), lpAction);
		this->Queue(this->qActionQueue, lpAction);
		LeaveCriticalSection(this->criticalSection);
	}

	void Remove(FileActionInfo* lpAction) {
		if(lpAction) {
			EnterCriticalSection(this->criticalSection);
			this->Dequeue(&((*this->mActionMap)[lpAction->GetFileName()]), lpAction);
			this->Dequeue(this->qActionQueue, lpAction);
			delete lpAction;
			LeaveCriticalSection(this->criticalSection);
		}
	}

	UINT Size() {
		return this->qActionQueue->size();
	}

	FileActionInfo* Last() {
		FileActionInfo* result = NULL;
		vector<FileActionInfo*> *v = this->qActionQueue;
		EnterCriticalSection(this->criticalSection);
		if(v->size() > 0)  result = v->at(v->size()-1);
		LeaveCriticalSection(this->criticalSection);
		return result;
	}

	FileActionInfo* Search(LPWSTR fileName, DWORD action, PCHAR drives) {
		FileActionInfo* result = NULL;
		vector<FileActionInfo*> *v;
		EnterCriticalSection(this->criticalSection);
		if( v = &((*this->mActionMap)[fileName])) {
			for(int i = 0, nCount = v->size(); i < nCount && !result; ++i){
				FileActionInfo* lpAction = v->at(i);
				if(lpAction->GetAction() == action) {
					int j = 0;
					while(!result && drives[j]) {
						if(lpAction->GetDrive() == drives[j]) result = lpAction;
						++j;
					}			
				}
			}
		}
		LeaveCriticalSection(this->criticalSection);
		return result;
	}

	BOOL Search(FileActionInfo* lpInfo) {
		BOOL result = FALSE;
		vector<FileActionInfo*> *v = this->qActionQueue;
		EnterCriticalSection(this->criticalSection);
		for(int i = 0, nCount = v->size(); i < nCount && !result; ++i){
			if(v->at(i) == lpInfo) result = TRUE;
		}
		LeaveCriticalSection(this->criticalSection);
		return result;
	}

};