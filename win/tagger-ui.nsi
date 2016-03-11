; tagger.nsi
;
; This script is meant to create installation package for tagger-UI suite.
; It has uninstall support and (optionally) installs start menu shortcuts.
;
;--------------------------------
!include "MUI2.nsh"
!include 'LogicLib.nsh'

; The name of the installer
Name "Tagger-UI suite"

; The file to write
OutFile "tagger-ui-setup-win32.exe"

; The default installation directory
InstallDir "$PROGRAMFILES\TaggerUI"

; Registry key for keeping track of installation directory
InstallDirRegKey HKLM "SOFTWARE\TaggerUI" "Install_Dir"

; Request application privileges (if neceseary)
RequestExecutionLevel admin

;--------------------------------
;Interface Settings

  !define MUI_ABORTWARNING

;--------------------------------
;Pages
  
  !insertmacro MUI_PAGE_LICENSE "LICENSE.txt"

  !insertmacro MUI_PAGE_COMPONENTS

  !define MUI_DIRECTORYPAGE_VARIABLE $INSTDIR
  !insertmacro MUI_PAGE_DIRECTORY

  Var TAGGERDIR
  !define MUI_PAGE_HEADER_TEXT "Specify path of dependencies"
  !define MUI_PAGE_HEADER_SUBTEXT "Choose the folder in which is located required program"
  !define MUI_DIRECTORYPAGE_VARIABLE $TAGGERDIR
  !define MUI_DIRECTORYPAGE_TEXT_TOP "In order to run, this suite need the tagger.exe program.$\nPlease, select the folder in which this program is located.$\n$\nIf you haven't installed it yet, you can download the latest version at <http://www.github.com/cedricfrancoys/tagger>"
  !define MUI_DIRECTORYPAGE_TEXT_DESTINATION "tagger.exe installation directory"
  !define MUI_PAGE_CUSTOMFUNCTION_PRE TaggerDirectoryEnter
  !define MUI_PAGE_CUSTOMFUNCTION_LEAVE TaggerDirectoryLeave
  !insertmacro MUI_PAGE_DIRECTORY
  
  !insertmacro MUI_PAGE_INSTFILES

  !insertmacro MUI_UNPAGE_CONFIRM

  !insertmacro MUI_UNPAGE_INSTFILES

;--------------------------------
;Languages

  !insertmacro MUI_LANGUAGE "English"


;--------------------------------

; The stuff to install
Section "Tagger UI (required)" sectionTagger

  SectionIn RO

  ; Set output path to the installation directory.
  SetOutPath $INSTDIR

  ; attached binaries
  File "tfsearch.exe"
  File "tftag.exe"


  ; Write the installation path into the registry
  WriteRegStr HKLM "SOFTWARE\TaggerUI" "Tagger_Dir" "$TAGGERDIR"
  WriteRegStr HKLM "SOFTWARE\TaggerUI" "Install_Dir" "$INSTDIR"
  ; add shortcut for tagging in the generic file contextmenu
  WriteRegStr HKCR "*\shell\TaggerUI" "" "&tag file..."
  WriteRegStr HKCR "*\shell\TaggerUI\Command" "" '"$INSTDIR\tftag.exe" "%1"'

  ; Write the uninstall keys for Windows
  WriteRegStr HKLM "SOFTWARE\Microsoft\Windows\CurrentVersion\Uninstall\TaggerUI" "DisplayName" "Tagger UI suite"
  WriteRegStr HKLM "SOFTWARE\Microsoft\Windows\CurrentVersion\Uninstall\TaggerUI" "UninstallString" '"$INSTDIR\uninstall.exe"'
  WriteRegDWORD HKLM "SOFTWARE\Microsoft\Windows\CurrentVersion\Uninstall\TaggerUI" "NoModify" 1
  WriteRegDWORD HKLM "SOFTWARE\Microsoft\Windows\CurrentVersion\Uninstall\TaggerUI" "NoRepair" 1
  WriteUninstaller "uninstall.exe"

SectionEnd

Section "tagger command-line tool" sectionCLI
  ; attached binaries
  File "tagger.exe"

SectionEnd

Section "FS monitor" sectionFSMonitor
  ; attached binaries
  File "tfmon.exe"
  
  WriteRegStr HKCU "SOFTWARE\Microsoft\Windows\CurrentVersion\Run" "TaggerUI FS Monitor" '"$INSTDIR\tfmon.exe"'
SectionEnd

; Optional section (can be disabled by the user)
Section "Start Menu Shortcuts" sectionStartMenu
  CreateDirectory "$SMPROGRAMS\TaggerUI"

  CreateShortCut "$SMPROGRAMS\TaggerUI\File Search.lnk" "$INSTDIR\tfsearch.exe" "" "$INSTDIR\tfsearch.exe" 0
  ${If} ${SectionIsSelected} ${sectionFSMonitor}
    CreateShortCut "$SMPROGRAMS\TaggerUI\FileSystem monitor.lnk" "$INSTDIR\tfmon.exe" "" "$INSTDIR\tfmon.exe" 0  
  ${EndIf}
  
  CreateShortCut "$SMPROGRAMS\TaggerUI\Uninstall.lnk" "$INSTDIR\uninstall.exe" "" "$INSTDIR\uninstall.exe" 0  
SectionEnd

;--------------------------------

; Uninstaller

Section "Uninstall"

  ; Remove registry keys
  DeleteRegKey HKCU "SOFTWARE\Microsoft\Windows\CurrentVersion\Run\TaggerUI FS Monitor"
  DeleteRegKey HKLM "SOFTWARE\Microsoft\Windows\CurrentVersion\Uninstall\TaggerUI"
  DeleteRegKey HKLM "SOFTWARE\TaggerUI"
  DeleteRegKey HKCR "*\shell\TaggerUI"

  ; Remove files and uninstaller
  Delete "$INSTDIR\tagger.exe"
  Delete "$INSTDIR\tfsearch.exe"
  Delete "$INSTDIR\tftag.exe"
  Delete "$INSTDIR\tfmon.exe"  
  Delete "$INSTDIR\uninstall.exe"

  ; Remove shortcuts, if any
  Delete "$SMPROGRAMS\TaggerUI\*.*"

  ; Remove directories used
  RMDir "$SMPROGRAMS\TaggerUI"
  RMDir "$INSTDIR"

SectionEnd

;--------------------------------

; Custom functions
Function TaggerDirectoryEnter
	; set default value fot Tagger directory to previously set, if any
	ReadRegStr $TAGGERDIR HKLM "SOFTWARE\TaggerUI" "Tagger_Dir"

    ${If} ${SectionIsSelected} ${sectionCLI}     
        StrCpy $TAGGERDIR $INSTDIR
        Abort ; skip this page (user asked to install embedded tagger.exe)
    ${EndIf}

FunctionEnd

Function TaggerDirectoryLeave
  IfFileExists $TAGGERDIR\tagger.exe +3
  MessageBox MB_OK "Program tagger.exe was not found in the specified folder."
  Abort
FunctionEnd

;--------------------------------

; Custom descriptions

LangString DESC_SectionTagger ${LANG_ENGLISH} "Tagger UI main component consists of two executables: tftag.exe (for tagging files) and tfsearch.exe (for searching files).$\n$\nIn addition it updates files contextmenu."
LangString DESC_SectionStartMenu ${LANG_ENGLISH} "Add custom shortcuts to the start menu."
LangString DESC_sectionCLI ${LANG_ENGLISH} "tagger.exe is required (check this if it is not already on your system)."
LangString DESC_sectionFSMonitor ${LANG_ENGLISH} "File system changes notifier : required in order to maintain tag DB consistency in case of move, rename or delete."

!insertmacro MUI_FUNCTION_DESCRIPTION_BEGIN
  !insertmacro MUI_DESCRIPTION_TEXT ${SectionTagger} $(DESC_SectionTagger)
  !insertmacro MUI_DESCRIPTION_TEXT ${SectionStartMenu} $(DESC_SectionStartMenu)
!insertmacro MUI_FUNCTION_DESCRIPTION_END

