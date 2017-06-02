FROM microsoft/windowsservercore:10.0.14393.1198
LABEL Description="Windows Server Core development environment for Qbs with Qt 5.8, Chocolatey and various dependencies for testing Qbs modules and functionality"

COPY qtifwsilent.qs C:\qtifwsilent.qs
RUN powershell -NoProfile -ExecutionPolicy Bypass -Command \
    $ErrorActionPreference = 'Stop'; \
    wget 'https://download.qt.io/official_releases/qt/5.8/5.8.0/qt-opensource-windows-x86-msvc2015-5.8.0.exe' -OutFile C:\qt32.exe ; \
    wget 'https://download.qt.io/official_releases/qt/5.8/5.8.0/qt-opensource-windows-x86-msvc2015_64-5.8.0.exe' -OutFile C:\qt64.exe ; \
    $Env:QT_INSTALL_DIR = 'C:\\Qt_5.8.0_msvc2015'    ; Start-Process C:\qt32.exe -ArgumentList '--script C:/qtifwsilent.qs' -Wait ; \
    $Env:QT_INSTALL_DIR = 'C:\\Qt_5.8.0_msvc2015_64' ; Start-Process C:\qt64.exe -ArgumentList '--script C:/qtifwsilent.qs' -Wait ; \
    Remove-Item C:\qt32.exe -Force ; \
    Remove-Item C:\qt64.exe -Force ; \
    Remove-Item C:\qtifwsilent.qs -Force
ENV QTDIR C:\\Qt_5.8.0_msvc2015\\5.8\\msvc2015
ENV QTDIR64 C:\\Qt_5.8.0_msvc2015_64\\5.8\\msvc2015_64

RUN @powershell -NoProfile -ExecutionPolicy Bypass -Command \
    $Env:chocolateyVersion = '0.10.5' ; \
    $Env:chocolateyUseWindowsCompression = 'false' ; \
    "iex ((New-Object System.Net.WebClient).DownloadString('https://chocolatey.org/install.ps1'))" && SET "PATH=%PATH%;%ALLUSERSPROFILE%\chocolatey\bin"
RUN choco install -y qbs --version 1.8.0
RUN choco install -y unzip --version 6.0
RUN choco install -y visualcpp-build-tools --version 14.0.25420.1
RUN choco install -y zip --version 3.0

# Disable crash dialog for release-mode runtimes
RUN reg add "HKLM\SOFTWARE\Microsoft\Windows\Windows Error Reporting" /v DontShowUI /t REG_DWORD /d 1 /f
