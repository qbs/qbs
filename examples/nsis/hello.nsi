# Based on https://nsis.sourceforge.io/Simple_tutorials
# ("Install a file and create an uninstaller to remove it").
#
# Qbs overrides any OutFile command; the installer name comes from
# product.targetName instead.

Name "Qbs Hello"
InstallDir "$DESKTOP\Qbs Hello"
RequestExecutionLevel user

Page directory
Page instfiles

Section ""
    SetOutPath $INSTDIR

    # Built application (path provided via nsis.defines).
    File "${buildDirectory}\hello.exe"

    # Data file from the source tree (next to this script).
    File "readme.txt"

    WriteUninstaller $INSTDIR\uninstall.exe
SectionEnd

Section "Uninstall"
    Delete $INSTDIR\hello.exe
    Delete $INSTDIR\readme.txt
    Delete $INSTDIR\uninstall.exe
    RMDir $INSTDIR
SectionEnd
