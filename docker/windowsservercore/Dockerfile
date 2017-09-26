FROM microsoft/windowsservercore:10.0.14393.1715
LABEL Description="Windows Server Core development environment for Qbs with Qt 5.9, Chocolatey and various dependencies for testing Qbs modules and functionality"

# Disable crash dialog for release-mode runtimes
RUN reg add "HKLM\SOFTWARE\Microsoft\Windows\Windows Error Reporting" /v Disabled /t REG_DWORD /d 1 /f
RUN reg add "HKLM\SOFTWARE\Microsoft\Windows\Windows Error Reporting" /v DontShowUI /t REG_DWORD /d 1 /f

COPY qtifwsilent.qs C:\qtifwsilent.qs
RUN powershell -NoProfile -ExecutionPolicy Bypass -Command \
    $ErrorActionPreference = 'Stop'; \
    $Wc = New-Object System.Net.WebClient ; \
    $Wc.DownloadFile('https://download.qt.io/official_releases/qt/5.9/5.9.1/qt-opensource-windows-x86-5.9.1.exe', 'C:\qt.exe') ; \
    Echo 'Downloaded qt-opensource-windows-x86-5.9.1.exe' ; \
    $Env:QT_INSTALL_DIR = 'C:\\Qt' ; \
    Start-Process C:\qt.exe -ArgumentList '--verbose --script C:/qtifwsilent.qs' -NoNewWindow -Wait ; \
    Remove-Item C:\qt.exe -Force ; \
    Remove-Item C:\qtifwsilent.qs -Force
ENV QTDIR C:\\Qt\\5.9.1\\msvc2015
ENV QTDIR64 C:\\Qt\\5.9.1\\msvc2015_64

RUN @powershell -NoProfile -ExecutionPolicy Bypass -Command \
    $Env:chocolateyVersion = '0.10.5' ; \
    $Env:chocolateyUseWindowsCompression = 'false' ; \
    "iex ((New-Object System.Net.WebClient).DownloadString('https://chocolatey.org/install.ps1'))" && SET "PATH=%PATH%;%ALLUSERSPROFILE%\chocolatey\bin"
RUN choco install -y qbs --version 1.9.0 && qbs --version
RUN choco install -y unzip --version 6.0 && unzip -v
RUN choco install -y visualcpp-build-tools --version 14.0.25420.1 && dir "%PROGRAMFILES(X86)%\Microsoft Visual C++ Build Tools"
RUN choco install -y zip --version 3.0 && zip -v
