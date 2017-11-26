Page directory
Page instfiles

Section ""
    SetOutPath "$INSTDIR"
    File "${buildDirectory}\app.exe"
    File "${buildDirectory}\lib.dll"
SectionEnd
