!include "MUI2.nsh"
!include "LogicLib.nsh"
!include "nsDialogs.nsh"

!ifndef VERSION
    !error "VERSION must be defined"
!endif
!ifndef PAYLOAD_DIR
    !error "PAYLOAD_DIR must be defined"
!endif
!ifndef LICENSE_FILE
    !error "LICENSE_FILE must be defined"
!endif
!ifndef OUTPUT_FILE
    !error "OUTPUT_FILE must be defined"
!endif
!ifndef ICON_FILE
    !error "ICON_FILE must be defined"
!endif

Name "Lymalink"
OutFile "${OUTPUT_FILE}"
InstallDir "$LOCALAPPDATA\Programs\Lymalink"
RequestExecutionLevel user
Unicode true

!define APP_NAME "Lymalink"
!define APP_PUBLISHER "Morsomus"
!define UNINSTALL_KEY "Software\Microsoft\Windows\CurrentVersion\Uninstall\Lymalink"
!define VULKAN_KEY "Software\Khronos\Vulkan\ImplicitLayers"

!define MUI_ABORTWARNING
!define MUI_ICON "${ICON_FILE}"
!define MUI_UNICON "${ICON_FILE}"
!define MUI_FINISHPAGE_RUN "$INSTDIR\Lymalink.exe"

!insertmacro MUI_PAGE_LICENSE "${LICENSE_FILE}"
Page custom InstallInfoPageCreate
!insertmacro MUI_PAGE_INSTFILES
!insertmacro MUI_PAGE_FINISH

!insertmacro MUI_UNPAGE_CONFIRM
!insertmacro MUI_UNPAGE_INSTFILES

!insertmacro MUI_LANGUAGE "English"

Var InfoDialog
Var InfoLabel

Function InstallInfoPageCreate
    nsDialogs::Create 1018
    Pop $InfoDialog
    ${If} $InfoDialog == error
        Abort
    ${EndIf}

    ${NSD_CreateLabel} 0 0 100% 100% "Lymalink will be installed for the current Windows user only.$\r$\n$\r$\nInstall location:$\r$\n$LOCALAPPDATA\Programs\Lymalink$\r$\n$\r$\nStart Menu shortcut:$\r$\n$SMPROGRAMS\Lymalink\Lymalink.lnk$\r$\n$\r$\nRunning the installer again updates installed program files and preserves existing user configuration and database files.$\r$\n$\r$\nUninstall preserves only user configuration and database files."
    Pop $InfoLabel

    nsDialogs::Show
FunctionEnd

Section "Install"
    SetShellVarContext current

    Call StopLymalinkProcesses

    RMDir /r "$INSTDIR"
    SetOutPath "$INSTDIR"

    File /r "${PAYLOAD_DIR}\*.*"

    Call WriteOverlayManifestX64
    Call WriteOverlayManifestX86
    Call RegisterOverlayManifestX64
    Call RegisterOverlayManifestX86

    CreateDirectory "$SMPROGRAMS\Lymalink"
    CreateShortCut "$SMPROGRAMS\Lymalink\Lymalink.lnk" "$INSTDIR\Lymalink.exe" "" "$INSTDIR\Lymalink.exe" 0

    WriteUninstaller "$INSTDIR\uninstall-lymalink.exe"
    Call WriteUninstallRegistry
SectionEnd

Section "Uninstall"
    SetShellVarContext current

    Call un.StopLymalinkProcesses
    Call un.UnregisterOverlayManifestX64
    Call un.UnregisterOverlayManifestX86

    Delete "$SMPROGRAMS\Lymalink\Lymalink.lnk"
    RMDir "$SMPROGRAMS\Lymalink"

    Delete "$INSTDIR\Lymalink.exe"
    Delete "$INSTDIR\lymalinkd.exe"
    Delete "$INSTDIR\sqlite3.dll"
    Delete "$INSTDIR\64x64-lymalink-test-icon.png"
    Delete "$INSTDIR\lymalinkd-tray-icon.png"
    Delete "$INSTDIR\LICENSE"
    Delete "$INSTDIR\uninstall-lymalink.exe"
    Delete "$INSTDIR\*.dll"
    Delete "$INSTDIR\*.pdb"
    Delete "$INSTDIR\*.qml"
    Delete "$INSTDIR\*.qm"
    Delete "$INSTDIR\*.pak"
    Delete "$INSTDIR\*.dat"
    Delete "$INSTDIR\*.conf"
    Delete "$INSTDIR\*.json"
    RMDir /r "$INSTDIR\bearer"
    RMDir /r "$INSTDIR\generic"
    RMDir /r "$INSTDIR\iconengines"
    RMDir /r "$INSTDIR\imageformats"
    RMDir /r "$INSTDIR\networkinformation"
    RMDir /r "$INSTDIR\platforms"
    RMDir /r "$INSTDIR\platformthemes"
    RMDir /r "$INSTDIR\qml"
    RMDir /r "$INSTDIR\qmltooling"
    RMDir /r "$INSTDIR\sqldrivers"
    RMDir /r "$INSTDIR\styles"
    RMDir /r "$INSTDIR\tls"
    RMDir /r "$INSTDIR\translations"
    RMDir /r "$INSTDIR\xcbglintegrations"
    RMDir /r "$INSTDIR\sounds"
    RMDir /r "$INSTDIR\overlay"
    RMDir "$INSTDIR"

    RMDir /r "$LOCALAPPDATA\Lymalink"
    Call un.CleanUserDataPreservingConfigAndDatabase

    SetRegView 64
    DeleteRegKey HKCU "${UNINSTALL_KEY}"
    SetRegView lastused
SectionEnd

Function StopLymalinkProcesses
    ExecWait '"$SYSDIR\taskkill.exe" /IM Lymalink.exe /F'
    ExecWait '"$SYSDIR\taskkill.exe" /IM lymalinkd.exe /F'
FunctionEnd

Function un.StopLymalinkProcesses
    ExecWait '"$SYSDIR\taskkill.exe" /IM Lymalink.exe /F'
    ExecWait '"$SYSDIR\taskkill.exe" /IM lymalinkd.exe /F'
FunctionEnd

Function un.CleanUserDataPreservingConfigAndDatabase
    RMDir /r "$TEMP\lymalink-uninstall-preserve"
    CreateDirectory "$TEMP\lymalink-uninstall-preserve"

    IfFileExists "$APPDATA\Lymalink\config.ini" 0 +2
        CopyFiles /SILENT "$APPDATA\Lymalink\config.ini" "$TEMP\lymalink-uninstall-preserve\config.ini"
    IfFileExists "$APPDATA\Lymalink\lymalink_database" 0 +2
        CopyFiles /SILENT "$APPDATA\Lymalink\lymalink_database" "$TEMP\lymalink-uninstall-preserve\lymalink_database"
    IfFileExists "$APPDATA\Lymalink\lymalink_database-wal" 0 +2
        CopyFiles /SILENT "$APPDATA\Lymalink\lymalink_database-wal" "$TEMP\lymalink-uninstall-preserve\lymalink_database-wal"
    IfFileExists "$APPDATA\Lymalink\lymalink_database-shm" 0 +2
        CopyFiles /SILENT "$APPDATA\Lymalink\lymalink_database-shm" "$TEMP\lymalink-uninstall-preserve\lymalink_database-shm"

    RMDir /r "$APPDATA\Lymalink"

    IfFileExists "$TEMP\lymalink-uninstall-preserve\config.ini" 0 +3
        CreateDirectory "$APPDATA\Lymalink"
        CopyFiles /SILENT "$TEMP\lymalink-uninstall-preserve\config.ini" "$APPDATA\Lymalink\config.ini"
    IfFileExists "$TEMP\lymalink-uninstall-preserve\lymalink_database" 0 +3
        CreateDirectory "$APPDATA\Lymalink"
        CopyFiles /SILENT "$TEMP\lymalink-uninstall-preserve\lymalink_database" "$APPDATA\Lymalink\lymalink_database"
    IfFileExists "$TEMP\lymalink-uninstall-preserve\lymalink_database-wal" 0 +3
        CreateDirectory "$APPDATA\Lymalink"
        CopyFiles /SILENT "$TEMP\lymalink-uninstall-preserve\lymalink_database-wal" "$APPDATA\Lymalink\lymalink_database-wal"
    IfFileExists "$TEMP\lymalink-uninstall-preserve\lymalink_database-shm" 0 +3
        CreateDirectory "$APPDATA\Lymalink"
        CopyFiles /SILENT "$TEMP\lymalink-uninstall-preserve\lymalink_database-shm" "$APPDATA\Lymalink\lymalink_database-shm"

    RMDir /r "$TEMP\lymalink-uninstall-preserve"
FunctionEnd

Function WriteOverlayManifestX64
    FileOpen $0 "$INSTDIR\overlay\lymalink-overlay-vulkan-x64.json" w
    FileWrite $0 '{$\r$\n'
    FileWrite $0 '  "file_format_version": "1.0.0",$\r$\n'
    FileWrite $0 '  "layer": {$\r$\n'
    FileWrite $0 '    "name": "VK_LAYER_LYMALINK_overlay",$\r$\n'
    FileWrite $0 '    "type": "GLOBAL",$\r$\n'
    FileWrite $0 '    "library_path": "lymalink-overlay-vulkan-x64.dll",$\r$\n'
    FileWrite $0 '    "api_version": "1.4.312",$\r$\n'
    FileWrite $0 '    "implementation_version": "1",$\r$\n'
    FileWrite $0 '    "description": "Lymalink achievement overlay",$\r$\n'
    FileWrite $0 '    "disable_environment": {$\r$\n'
    FileWrite $0 '      "DISABLE_VK_LAYER_LYMALINK_overlay": "1"$\r$\n'
    FileWrite $0 '    },$\r$\n'
    FileWrite $0 '    "functions": {$\r$\n'
    FileWrite $0 '      "vkNegotiateLoaderLayerInterfaceVersion": "LymalinkLayer_vkNegotiateLoaderLayerInterfaceVersion"$\r$\n'
    FileWrite $0 '    }$\r$\n'
    FileWrite $0 '  }$\r$\n'
    FileWrite $0 '}$\r$\n'
    FileClose $0
FunctionEnd

Function WriteOverlayManifestX86
    FileOpen $0 "$INSTDIR\overlay\lymalink-overlay-vulkan-x86.json" w
    FileWrite $0 '{$\r$\n'
    FileWrite $0 '  "file_format_version": "1.0.0",$\r$\n'
    FileWrite $0 '  "layer": {$\r$\n'
    FileWrite $0 '    "name": "VK_LAYER_LYMALINK_overlay",$\r$\n'
    FileWrite $0 '    "type": "GLOBAL",$\r$\n'
    FileWrite $0 '    "library_path": "lymalink-overlay-vulkan-x86.dll",$\r$\n'
    FileWrite $0 '    "api_version": "1.4.312",$\r$\n'
    FileWrite $0 '    "implementation_version": "1",$\r$\n'
    FileWrite $0 '    "description": "Lymalink achievement overlay",$\r$\n'
    FileWrite $0 '    "disable_environment": {$\r$\n'
    FileWrite $0 '      "DISABLE_VK_LAYER_LYMALINK_overlay": "1"$\r$\n'
    FileWrite $0 '    },$\r$\n'
    FileWrite $0 '    "functions": {$\r$\n'
    FileWrite $0 '      "vkNegotiateLoaderLayerInterfaceVersion": "LymalinkLayer_vkNegotiateLoaderLayerInterfaceVersion"$\r$\n'
    FileWrite $0 '    }$\r$\n'
    FileWrite $0 '  }$\r$\n'
    FileWrite $0 '}$\r$\n'
    FileClose $0
FunctionEnd

Function RegisterOverlayManifestX64
    SetRegView 64
    WriteRegDWORD HKCU "${VULKAN_KEY}" "$INSTDIR\overlay\lymalink-overlay-vulkan-x64.json" 1
    SetRegView lastused
FunctionEnd

Function RegisterOverlayManifestX86
    SetRegView 32
    WriteRegDWORD HKCU "${VULKAN_KEY}" "$INSTDIR\overlay\lymalink-overlay-vulkan-x86.json" 1
    SetRegView lastused
FunctionEnd

Function un.UnregisterOverlayManifestX64
    SetRegView 64
    DeleteRegValue HKCU "${VULKAN_KEY}" "$INSTDIR\overlay\lymalink-overlay-vulkan-x64.json"
    SetRegView lastused
FunctionEnd

Function un.UnregisterOverlayManifestX86
    SetRegView 32
    DeleteRegValue HKCU "${VULKAN_KEY}" "$INSTDIR\overlay\lymalink-overlay-vulkan-x86.json"
    SetRegView lastused
FunctionEnd

Function WriteUninstallRegistry
    SetRegView 64
    WriteRegStr HKCU "${UNINSTALL_KEY}" "DisplayName" "${APP_NAME}"
    WriteRegStr HKCU "${UNINSTALL_KEY}" "DisplayVersion" "${VERSION}"
    WriteRegStr HKCU "${UNINSTALL_KEY}" "Publisher" "${APP_PUBLISHER}"
    WriteRegStr HKCU "${UNINSTALL_KEY}" "InstallLocation" "$INSTDIR"
    WriteRegStr HKCU "${UNINSTALL_KEY}" "DisplayIcon" "$INSTDIR\Lymalink.exe"
    WriteRegStr HKCU "${UNINSTALL_KEY}" "UninstallString" '"$INSTDIR\uninstall-lymalink.exe"'
    WriteRegStr HKCU "${UNINSTALL_KEY}" "QuietUninstallString" '"$INSTDIR\uninstall-lymalink.exe" /S'
    WriteRegDWORD HKCU "${UNINSTALL_KEY}" "NoModify" 1
    WriteRegDWORD HKCU "${UNINSTALL_KEY}" "NoRepair" 1
    SetRegView lastused
FunctionEnd
