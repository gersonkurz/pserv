SetCompressor /SOLID LZMA 

!define CURRENT_VERSION "5.0.0"

!include "MUI2.nsh"

XPStyle on 


Name "pserv5 ${CURRENT_VERSION}" 
OutFile "pserv5-${CURRENT_VERSION}-setup-arm64.exe"
InstallDir "$PROGRAMFILES64\pserv5"
InstallDirRegKey HKLM SOFTWARE\p-nand-q.com\pserv5 "Install_Dir"

!define MUI_ICON "${NSISDIR}\Contrib\Graphics\Icons\modern-install.ico"
!define MUI_UNICON "${NSISDIR}\Contrib\Graphics\Icons\modern-uninstall.ico"

!define MUI_WELCOMEPAGE_TITLE "pserv5 ${CURRENT_VERSION}"
!define MUI_ABORTWARNING

!define MUI_FINISHPAGE_RUN "$INSTDIR\pserv5.exe"

!insertmacro MUI_PAGE_WELCOME
!insertmacro MUI_PAGE_LICENSE "license.txt"
!insertmacro MUI_PAGE_DIRECTORY
!insertmacro MUI_PAGE_COMPONENTS
!insertmacro MUI_PAGE_INSTFILES

!insertmacro MUI_PAGE_FINISH

!insertmacro MUI_UNPAGE_WELCOME
!insertmacro MUI_UNPAGE_CONFIRM
!insertmacro MUI_UNPAGE_INSTFILES
!insertmacro MUI_UNPAGE_FINISH

!insertmacro MUI_LANGUAGE "English"

BrandingText "p-nand-q.com"

ShowInstDetails show

RequestExecutionLevel admin

  VIProductVersion "${CURRENT_VERSION}.0"
  VIAddVersionKey /LANG=${LANG_ENGLISH} "ProductName" "pserv5"
  VIAddVersionKey /LANG=${LANG_ENGLISH} "Comments" "MIT Licensed"
  VIAddVersionKey /LANG=${LANG_ENGLISH} "CompanyName" "Gerson Kurz"
  VIAddVersionKey /LANG=${LANG_ENGLISH} "LegalTrademarks" ""
  VIAddVersionKey /LANG=${LANG_ENGLISH} "LegalCopyright" "(C) Gerson Kurz"
  VIAddVersionKey /LANG=${LANG_ENGLISH} "FileDescription" "pserv5"
  VIAddVersionKey /LANG=${LANG_ENGLISH} "FileVersion" ${CURRENT_VERSION}

Section  "-pserv5 (required)"
    SetRegView 64
    SetOutPath $INSTDIR
    File /R ..\pserv5\bin\arm64\Release\*

    WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\pserv5" "DisplayName" "pserv5 (Remove only)"
    WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\pserv5" "UninstallString" '"$INSTDIR\uninstall.exe"'
    WriteUninstaller "$INSTDIR\uninstall.exe"
    DetailPrint "Done."
SectionEnd

Section "Create Desktop Shortcut"
    CreateShortCut "$DESKTOP\pserv5.lnk" "$INSTDIR\pserv5.exe" ""
SectionEnd

Section "Create Start Menu Shortcuts"
    CreateDirectory "$SMPROGRAMS\pserv5"
    CreateShortCut "$SMPROGRAMS\pserv5\Uninstall.lnk" "$INSTDIR\Uninstall.exe" "" "$INSTDIR\Uninstall.exe" 0
    CreateShortCut "$SMPROGRAMS\pserv5\pserv5.lnk" "$INSTDIR\pserv5.exe" "" "$INSTDIR\pserv5.exe" 0
SectionEnd

Section "Register Shell Extension"
    WriteRegStr HKCR "*\shell\pserv5" "Icon" "$INSTDIR\pserv5.exe"
    WriteRegStr HKCR "*\shell\pserv5" "Position" "Middle"
    WriteRegStr HKCR "*\shell\pserv5\Command" "" '$INSTDIR\pserv5.exe %0'
SectionEnd

Section "Uninstall"
    SetRegView 64
	Delete $INSTDIR\uninstall.exe
    Delete "$DESKTOP\pserv5.lnk"
    RMDir /r "$SMPROGRAMS\pserv5"
	ReadRegStr $INSTDIR HKLM SOFTWARE\p-nand-q.com\pserv5 "Install_Dir"
    RMDir /r "$INSTDIR\"
SectionEnd
