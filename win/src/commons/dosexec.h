/* dosexec.h - interface for running specified command under win32 DOS console.

    This file is part of the tagger-ui suite <http://www.github.com/cedricfrancoys/tagger-ui>
    Copyright (C) Cedric Francoys, 2016, Yegen
    Some Right Reserved, GNU GPL 3 license <http://www.gnu.org/licenses/>
*/


#ifndef __DOSEXEC_H
#define __DOSEXEC_H 1

/* Convert a multi-byte-character string to a UNICODE wide-character string (16-bit).
*/
LPWSTR CHARtoWCHAR(LPSTR str, UINT code);

/* Convert a UNICODE wide-character string (16-bit) to a multi-byte-character string.
*/
LPSTR WCHARtoCHAR(LPWSTR str, UINT code);

/* Execute a DOS command.
*/
LPWSTR DosExec(LPWSTR command);

// define explicit names for charset conversion functions

// Convert a UTF-16 string (16-bit) to an OEM string (8-bit) 
#define UNICODEtoOEM(str)	WCHARtoCHAR(str, CP_OEMCP)

// Convert an OEM string (8-bit) to a UTF-16 string (16-bit) 
#define OEMtoUNICODE(str)	CHARtoWCHAR(str, CP_OEMCP)

// Convert an ANSI string (8-bit) to a UTF-16 string (16-bit) 
#define ANSItoUNICODE(str)	CHARtoWCHAR(str, CP_ACP)

// Convert a UTF-16 string (16-bit) to an ANSI string (8-bit)
#define UNICODEtoANSI(str)	WCHARtoCHAR(str, CP_ACP)

#endif