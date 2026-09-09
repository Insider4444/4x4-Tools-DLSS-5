Unicode true
!include "MUI2.nsh"
!include "x64.nsh"
!include "FileFunc.nsh"
!include "LogicLib.nsh"
!ifndef PACKAGE
 !error "Provide /DPACKAGE=absolute-payload-directory"
!endif
!ifndef OUTPUT
 !error "Provide /DOUTPUT=absolute-output-exe"
!endif
!ifndef VERSION
 !error "Provide /DVERSION=major.minor.patch"
!endif
Name "4x4-Tools DLSS 5"
OutFile "${OUTPUT}"
InstallDir "$PROGRAMFILES64\4x4-Tools\DLSS-5"
RequestExecutionLevel admin
SetCompressor /SOLID lzma
SetCompressorDictSize 64
ShowInstDetails show
ShowUninstDetails show
ManifestDPIAware true
VIProductVersion "${VERSION}.0"
VIAddVersionKey /LANG=1033 "ProductName" "4x4-Tools DLSS 5"
VIAddVersionKey /LANG=1033 "CompanyName" "4x4-Tools"
VIAddVersionKey /LANG=1033 "FileDescription" "4x4-Tools DLSS 5 Setup"
VIAddVersionKey /LANG=1033 "FileVersion" "${VERSION}"
VIAddVersionKey /LANG=1033 "ProductVersion" "${VERSION}"
VIAddVersionKey /LANG=1033 "LegalCopyright" "Copyright 2026 4x4-Tools and contributors"
BrandingText "4x4Tools-DLSS5-win | v${VERSION}"
!define MUI_ICON "..\assets\app.ico"
!define MUI_UNICON "..\assets\app.ico"
!define MUI_WELCOMEFINISHPAGE_BITMAP "..\assets\installer-welcome.bmp"
!define MUI_HEADERIMAGE
!define MUI_HEADERIMAGE_BITMAP "..\assets\installer-header.bmp"
!define MUI_HEADERIMAGE_RIGHT
!define MUI_WELCOMEPAGE_TITLE "Bring your footage into focus."
!define MUI_WELCOMEPAGE_TEXT "Install 4x4-Tools DLSS 5 for After Effects and Premiere Pro.$\r$\n$\r$\nEight editable presets, three neural styles and precise finishing controls.$\r$\n$\r$\nSetup verifies the included runtime on your GPU before replacing any plug-in files. Save your work and close both Adobe apps before continuing.$\r$\n$\r$\nWindows x64 and a compatible NVIDIA RTX GPU are required."
!define MUI_LICENSEPAGE_TEXT_TOP "Review the plug-in license and separate third-party runtime terms."
!define MUI_FINISHPAGE_TITLE "4x4-Tools setup completed"
!define MUI_FINISHPAGE_TEXT "Reopen After Effects or Premiere Pro and search Effects for 4x4Tools-DLSS5.$\r$\n$\r$\nStart with the Natural balance footage preset and use the original / neural wipe to compare.$\r$\n$\r$\nSetup logs and previous-version backups are stored in ProgramData\4x4-Tools\DLSS-5."
!define MUI_FINISHPAGE_LINK "Read the controls and installation guide"
!define MUI_FINISHPAGE_LINK_LOCATION "https://github.com/Insider4444/4x4-Tools-DLSS-5#readme"
!define MUI_ABORTWARNING
!insertmacro MUI_PAGE_WELCOME
!insertmacro MUI_PAGE_LICENSE "${PACKAGE}\SETUP-LICENSE.txt"
!insertmacro MUI_PAGE_INSTFILES
!insertmacro MUI_PAGE_FINISH
!insertmacro MUI_UNPAGE_CONFIRM
!insertmacro MUI_UNPAGE_INSTFILES
!insertmacro MUI_UNPAGE_FINISH
!insertmacro MUI_LANGUAGE "English"
Var Arguments
Var TestRoot
Var TestArgument
Var ValidateOnly
Var PsExe

Function .onInit
 ${IfNot} ${RunningX64}
   MessageBox MB_ICONSTOP "4x4-Tools requires 64-bit Windows." /SD IDOK
   SetErrorLevel 1
   Abort
 ${EndIf}
 SetRegView 64
 StrCpy $PsExe "$WINDIR\Sysnative\WindowsPowerShell\v1.0\powershell.exe"
 ${GetParameters} $Arguments
 ${GetOptions} $Arguments "/TESTROOT=" $TestRoot
 StrCpy $TestArgument ""
 ${If} $TestRoot != ""
   StrCpy $TestArgument ' -SandboxRoot "$TestRoot"'
 ${EndIf}
 ClearErrors
 ${GetOptions} $Arguments "/VALIDATEONLY" $ValidateOnly
 ${IfNot} ${Errors}
   StrCpy $ValidateOnly "yes"
 ${EndIf}
FunctionEnd

Section "Install 4x4-Tools" SEC_MAIN
 InitPluginsDir
 SetOutPath "$PLUGINSDIR\payload"
 File /r "${PACKAGE}\payload\*.*"
 SetOutPath "$PLUGINSDIR"
 File "installer-engine.ps1"
 WriteUninstaller "$PLUGINSDIR\Uninstall.exe"
 ${If} $ValidateOnly == "yes"
   nsExec::ExecToLog '"$PsExe" -NoLogo -NoProfile -NonInteractive -ExecutionPolicy Bypass -File "$PLUGINSDIR\installer-engine.ps1" -Action Validate -PackageDir "$PLUGINSDIR\payload"$TestArgument'
 ${Else}
   nsExec::ExecToLog '"$PsExe" -NoLogo -NoProfile -NonInteractive -ExecutionPolicy Bypass -File "$PLUGINSDIR\installer-engine.ps1" -Action Install -PackageDir "$PLUGINSDIR\payload" -MaintenanceDir "$PLUGINSDIR"$TestArgument'
 ${EndIf}
 Pop $0
 ${If} $0 != 0
   MessageBox MB_ICONSTOP "Setup could not complete. Read the details above for the specific cause.$\r$\n$\r$\nSave and close Adobe apps, verify your NVIDIA driver and retry.$\r$\n$\r$\nLogs: ProgramData\4x4-Tools\DLSS-5\Logs (validation-only logs are in your temporary folder)." /SD IDOK
   SetErrorLevel 1
   Abort
 ${EndIf}
 SetErrorLevel 0
SectionEnd

Function un.onInit
 SetRegView 64
 StrCpy $PsExe "$WINDIR\Sysnative\WindowsPowerShell\v1.0\powershell.exe"
 ${GetParameters} $Arguments
 ${GetOptions} $Arguments "/TESTROOT=" $TestRoot
 StrCpy $TestArgument ""
 ${If} $TestRoot != ""
   StrCpy $TestArgument ' -SandboxRoot "$TestRoot"'
 ${EndIf}
FunctionEnd
Section "Uninstall"
 nsExec::ExecToLog '"$PsExe" -NoLogo -NoProfile -NonInteractive -ExecutionPolicy Bypass -File "$INSTDIR\installer-engine.ps1" -Action Uninstall$TestArgument'
 Pop $0
 ${If} $0 != 0
   MessageBox MB_ICONSTOP "Uninstall could not complete. Close Adobe apps and review the details and setup log, then retry." /SD IDOK
   SetErrorLevel 1
   Abort
 ${EndIf}
 Delete "$INSTDIR\installer-engine.ps1"
 Delete "$INSTDIR\Uninstall.exe"
 RMDir "$INSTDIR"
 SetErrorLevel 0
SectionEnd
