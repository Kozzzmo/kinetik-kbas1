; Windows installer (NSIS 3). Built in CI:
;   makensis -DPRODUCT="Kinetik KDSP-16" -DBIN=KinetikKDSP16 -DVERSION=0.6 -DOUTFILE=out.exe packaging/windows/installer.nsi
; Paths are relative to the repository root (makensis is run with /NOCD from the root, see the workflow).
Unicode true
SetCompressor /SOLID lzma
Name "${PRODUCT}"
OutFile "${OUTFILE}"
RequestExecutionLevel admin
!ifdef SIGN
  ; sign the uninstaller and the installer with the same certificate (scripts/sign-windows.sh)
  !uninstfinalize 'sh scripts/sign-windows.sh "%1"'
  !finalize 'sh scripts/sign-windows.sh "%1"'
!endif
InstallDir "$PROGRAMFILES64\Kinetik\${BIN}"
BrandingText "${PRODUCT} ${VERSION}"
VIProductVersion "${VERSION}.0.0"
VIAddVersionKey "ProductName" "${PRODUCT}"
VIAddVersionKey "FileVersion" "${VERSION}"
VIAddVersionKey "FileDescription" "${PRODUCT} installer"
VIAddVersionKey "LegalCopyright" "All rights reserved"

!include "MUI2.nsh"
!include "x64.nsh"
!define MUI_ABORTWARNING
!insertmacro MUI_PAGE_WELCOME
!insertmacro MUI_PAGE_LICENSE "EULA.txt"
!insertmacro MUI_PAGE_INSTFILES
!insertmacro MUI_PAGE_FINISH
!insertmacro MUI_UNPAGE_CONFIRM
!insertmacro MUI_UNPAGE_INSTFILES
!insertmacro MUI_LANGUAGE "English"
!insertmacro MUI_LANGUAGE "French"

; delete FILE, or rename it to FILE.<ticks>.old when a program has it loaded; $R8 = "1" if both fail
!macro MoveAside FILE
  IfFileExists "${FILE}" 0 +8
  ClearErrors
  Delete "${FILE}"
  IfErrors 0 +5
  ClearErrors
  Rename "${FILE}" "${FILE}.$R9.old"
  IfErrors 0 +2
  StrCpy $R8 "1"
!macroend

!define UNINSTKEY "Software\Microsoft\Windows\CurrentVersion\Uninstall\${BIN}"

Function .onInit
  ${IfNot} ${RunningX64}
    MessageBox MB_OK|MB_ICONSTOP "${PRODUCT} requires 64-bit Windows."
    Abort
  ${EndIf}
  SetRegView 64
FunctionEnd

Section "Install"
  SetRegView 64
  ; old plug-in files moved aside by an earlier update (deleted once no program has them open any more)
  Delete "$COMMONFILES64\VST3\${BIN}.vst3\Contents\x86_64-win\*.old"
  Delete "$COMMONFILES64\CLAP\${BIN}.clap.*.old"
  ; an open music program keeps the plug-in loaded: it cannot be deleted, but it can be renamed. The host keeps
  ; running the old code until it loads the plug-in again (next project / restart), then it gets the new one.
  System::Call 'kernel32::GetTickCount() i .R9'
  StrCpy $R8 "0"
  !insertmacro MoveAside "$COMMONFILES64\VST3\${BIN}.vst3\Contents\x86_64-win\${BIN}.vst3"
  !insertmacro MoveAside "$COMMONFILES64\CLAP\${BIN}.clap"
  StrCmp $R8 "1" 0 not_in_use
  MessageBox MB_OK|MB_ICONEXCLAMATION "${PRODUCT} is in use and could not be replaced. Close your music software and run the installer again." /SD IDOK
  SetErrorLevel 3
  Abort "${PRODUCT} is in use: close your music software and run the installer again."
  not_in_use:
  SetOutPath "$COMMONFILES64\VST3\${BIN}.vst3"
  File /r "bin\${BIN}.vst3\*.*"
  CreateDirectory "$COMMONFILES64\CLAP"
  SetOutPath "$COMMONFILES64\CLAP"
  File "bin\${BIN}.clap"
  SetOutPath "$INSTDIR"
  File "EULA.txt"
  File "THIRD_PARTY_NOTICES.txt"
!ifdef SAMPLES
  ; built-in sample library (folders of WAV / AIFF files), replaced on every install
  RMDir /r "$INSTDIR\Samples"
  SetOutPath "$INSTDIR\Samples"
  File /nonfatal /r "${SAMPLES}\*.wav"
  File /nonfatal /r "${SAMPLES}\*.aif"
  File /nonfatal /r "${SAMPLES}\*.aiff"
  SetOutPath "$INSTDIR"
!endif
  WriteUninstaller "$INSTDIR\Uninstall.exe"
  WriteRegStr HKLM "${UNINSTKEY}" "DisplayName" "${PRODUCT}"
  WriteRegStr HKLM "${UNINSTKEY}" "DisplayVersion" "${VERSION}"
  WriteRegStr HKLM "${UNINSTKEY}" "Publisher" "Kinetik"
  WriteRegStr HKLM "${UNINSTKEY}" "UninstallString" '"$INSTDIR\Uninstall.exe"'
  WriteRegStr HKLM "${UNINSTKEY}" "InstallLocation" "$INSTDIR"
  WriteRegDWORD HKLM "${UNINSTKEY}" "NoModify" 1
  WriteRegDWORD HKLM "${UNINSTKEY}" "NoRepair" 1
SectionEnd

Section "Uninstall"
  SetRegView 64
  Delete "$COMMONFILES64\CLAP\${BIN}.clap.*.old"
  RMDir /r "$COMMONFILES64\VST3\${BIN}.vst3"
  Delete "$COMMONFILES64\CLAP\${BIN}.clap"
  Delete "$INSTDIR\EULA.txt"
  Delete "$INSTDIR\THIRD_PARTY_NOTICES.txt"
  Delete "$INSTDIR\Uninstall.exe"
  RMDir /r "$INSTDIR\Samples"
  RMDir "$INSTDIR"
  RMDir "$PROGRAMFILES64\Kinetik"
  DeleteRegKey HKLM "${UNINSTKEY}"
SectionEnd
