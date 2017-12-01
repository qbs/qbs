[Setup]
AppName={#MyProgram}
AppVersion={#MyProgramVersion}
DefaultDirName={pf}\{#MyProgram}

[Files]
Source: "{#buildDirectory}\app.exe"; DestDir: "{app}"
Source: "{#buildDirectory}\lib.dll"; DestDir: "{app}"
