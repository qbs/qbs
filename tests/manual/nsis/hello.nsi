SetCompressor zlib

!ifdef Win64
    Name "Qbs Hello - ${qbs.architecture} (64-bit)"
!else
    Name "Qbs Hello - ${qbs.architecture} (32-bit)"
!endif

OutFile "you-should-not-see-a-file-with-this-name.exe"
InstallDir "$DESKTOP\Qbs Hello"
RequestExecutionLevel user

Page directory
Page instfiles

Section ""
    SetOutPath "$INSTDIR"
    File "${batchFile}"
SectionEnd
