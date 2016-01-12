#include <windows.h>
#include "DosCommand.h"

/*
	'ANSI' refers to windows-125x, used for win32 applications
	while 'OEM' refers to the code page used by console/MS-DOS applications.
	Note: current active code-pages can be retrieved with functions GetOEMCP() and GetACP()
*/

/* Convert a single/multi-byte string to a UTF-16 string (16-bit).
 We take advantage of the MultiByteToWideChar function that allows to specify the charset of the input string.
*/
LPWSTR CHARtoWCHAR(LPSTR str, UINT codePage) {
	size_t len = strlen(str) + 1;
	int size_needed = MultiByteToWideChar(codePage, 0, str, len, NULL, 0);
	LPWSTR wstr = (LPWSTR) LocalAlloc(LPTR, sizeof(WCHAR) * size_needed);
	MultiByteToWideChar(codePage, 0, str, len, wstr, size_needed);
	return wstr;
}


/* Convert a UTF-16 string (16-bit) to a single/multi-byte string.
 We take advantage of the WideCharToMultiByte function that allows to specify the charset of the output string.
*/
LPSTR WCHARtoCHAR(LPWSTR wstr, UINT codePage) {
	size_t len = wcslen(wstr) + 1;
	int size_needed = WideCharToMultiByte(codePage, 0, wstr, len, NULL, 0, NULL, NULL);
	LPSTR str = (LPSTR) LocalAlloc(LPTR, sizeof(CHAR) * size_needed );
	WideCharToMultiByte(codePage, 0, wstr, len, str, size_needed, NULL, NULL);
	return str;
}


/* Execute a DOS command.

 If the function succeeds, the return value is a non-NULL pointer to the output of the invoked command. 
 Command will produce a 8-bit characters stream using OEM code-page.
 
 As charset depends on OS config (ex: CP437 [OEM-US/latin-US], CP850 [OEM 850/latin-1]),
 before being returned, output is converted to a wide-char string with function OEMtoUNICODE.

 Resulting buffer is allocated with LocalAlloc.
 It is the caller's responsibility to free the memory used by the argument list when it is no longer needed. 
 To free the memory, use a single call to LocalFree function.
*/
LPWSTR DosExec(LPWSTR command){
	// Allocate 1Mo to store the output (final buffer will be sized to actual output)
    // If output exceeds that size, it will be truncated
	const SIZE_T RESULT_SIZE = sizeof(char)*1024*1024;
	char* output = (char*) LocalAlloc(LPTR, RESULT_SIZE);

	HANDLE readPipe, writePipe;
    SECURITY_ATTRIBUTES	security;
    STARTUPINFOA		start;
    PROCESS_INFORMATION	processInfo;
    
    security.nLength = sizeof(SECURITY_ATTRIBUTES);
    security.bInheritHandle = true;
    security.lpSecurityDescriptor = NULL;

    if ( CreatePipe(
                    &readPipe,  // address of variable for read handle
                    &writePipe, // address of variable for write handle
                    &security,  // pointer to security attributes
                    0 	        // number of bytes reserved for pipe
					) ){

        
        GetStartupInfoA(&start);
        start.hStdOutput  = writePipe;
        start.hStdError   = writePipe;
        start.hStdInput   = readPipe;
        start.dwFlags     = STARTF_USESTDHANDLES + STARTF_USESHOWWINDOW;
        start.wShowWindow = SW_HIDE;

// We have to start the DOS app the same way cmd.exe does (using the current Win32 ANSI code-page).
// So, we use the "ANSI" version of createProcess, to be able to pass a LPSTR (single/multi-byte character string) 
// instead of a LPWSTR (wide-character string) and we use the UNICODEtoANSI function to convert the given command 
        if (CreateProcessA(NULL,                    // pointer to name of executable module
                           UNICODEtoANSI(command),	// pointer to command line string
                           &security,               // pointer to process security attributes
                           &security,               // pointer to thread security attributes
                           TRUE,                    // handle inheritance flag
                           NORMAL_PRIORITY_CLASS,   // creation flags
                           NULL,                    // pointer to new environment block
                           NULL,                    // pointer to current directory name
	                       &start,                  // pointer to STARTUPINFO
                           &processInfo             // pointer to PROCESS_INFORMATION
                      )){

			// wait for the child process to start
			for(UINT state = WAIT_TIMEOUT; state == WAIT_TIMEOUT; state = WaitForSingleObject(processInfo.hProcess, 100) );

			DWORD bytesRead = 0, count = 0;
			const int BUFF_SIZE = 1024;
			char* buffer = (char*) malloc(sizeof(char)*BUFF_SIZE+1);
			strcpy(output, "");
			do {				
				DWORD dwAvail = 0;
				if (!PeekNamedPipe(readPipe, NULL, 0, NULL, &dwAvail, NULL)) {
					// error, the child process might have ended
					break;
				}
				if (!dwAvail) {
					// no data available in the pipe
					break;
				}
				ReadFile(readPipe, buffer, BUFF_SIZE, &bytesRead, NULL);
				buffer[bytesRead] = '\0';
				if((count+bytesRead) > RESULT_SIZE) break;
				strcat(output, buffer);
				count += bytesRead;
			} while (bytesRead >= BUFF_SIZE);
			free(buffer);
        }

    }

	CloseHandle(processInfo.hThread);
    CloseHandle(processInfo.hProcess);
    CloseHandle(writePipe);
	CloseHandle(readPipe);

	// convert buffer to a wide-character string
	LPWSTR result = OEMtoUNICODE(output);

	LocalFree(output);
	return result;
}