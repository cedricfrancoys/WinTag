#pragma once

#include <time.h>

/* constants defined in winnt.h :
#define FILE_ACTION_ADDED                   0x00000001   
#define FILE_ACTION_REMOVED                 0x00000002   
#define FILE_ACTION_MODIFIED                0x00000003   
#define FILE_ACTION_RENAMED_OLD_NAME        0x00000004   
#define FILE_ACTION_RENAMED_NEW_NAME        0x00000005
*/
#define FILE_ACTION_MOVED			        0x00000008
#define FILE_ACTION_RESTORED			    0x00000010


/* Structure holding info about a file modification.
*/
class FileActionInfo {
private:
	LPWSTR	filePath;	
	DWORD	action;
	time_t	timestamp;
	// virtual members accessible through Getters
	// CHAR	drive;
	// LPWSTR fileName;
public:

	FileActionInfo(LPCWSTR filePath, DWORD action) {		
		this->filePath = (LPWSTR) GlobalAlloc(GPTR, sizeof(WCHAR)*(wcslen(filePath)+1));
		wcscpy(this->filePath, filePath);
		this->action = action;
		this->timestamp = time(NULL);
	}
    
	~FileActionInfo() { GlobalFree(this->filePath);	}

	LPWSTR GetFilePath() { return this->filePath; }
	DWORD GetAction() { return this->action; }
	time_t GetTimeStamp() { return this->timestamp; }

	CHAR GetDrive() {
		CHAR result = 0;
		if(wcslen(this->filePath) > 0) result = (CHAR) this->filePath[0];		
		return result;
	}
    
	LPWSTR GetFileName() {
		LPWSTR result = wcsrchr(this->filePath, '\\');
		if(result) result += sizeof(WCHAR);
		return result;
	}
};