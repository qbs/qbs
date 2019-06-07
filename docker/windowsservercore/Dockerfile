FROM microsoft/windowsservercore:10.0.14393.1884 as build-env
LABEL Description="Windows Server Core development environment for Qbs with Qt, Chocolatey and various dependencies for testing Qbs modules and functionality"

# Disable crash dialog for release-mode runtimes
RUN reg add "HKLM\SOFTWARE\Microsoft\Windows\Windows Error Reporting" /v Disabled /t REG_DWORD /d 1 /f
RUN reg add "HKLM\SOFTWARE\Microsoft\Windows\Windows Error Reporting" /v DontShowUI /t REG_DWORD /d 1 /f

RUN @powershell -NoProfile -ExecutionPolicy Bypass -Command \
    $Env:chocolateyVersion = '0.10.8' ; \
    $Env:chocolateyUseWindowsCompression = 'false' ; \
    "iex ((New-Object System.Net.WebClient).DownloadString('https://chocolatey.org/install.ps1'))" && SET "PATH=%PATH%;%ALLUSERSPROFILE%\chocolatey\bin"
RUN choco install -y python2 --version 2.7.14 && refreshenv && python --version && pip --version
RUN choco install -y vcredist140
RUN choco install -y qbs --version 1.12.1 && qbs --version
RUN choco install -y visualcpp-build-tools --version 14.0.25420.1 && dir "%PROGRAMFILES(X86)%\Microsoft Visual C++ Build Tools"
RUN choco install -y unzip --version 6.0
RUN choco install -y zip --version 3.0


FROM build-env as qt-build-env
LABEL Description="Environment for Building Qt"
RUN choco install -y jom --version 1.1.2 && jom /version

# Download and extract Qt source archives
WORKDIR C:/
RUN md src
WORKDIR src
COPY build-qt.bat .
ADD http://download.qt.io/official_releases/qt/5.12/5.12.5/submodules/qtbase-everywhere-src-5.12.5.zip qtbase.zip
ADD http://download.qt.io/official_releases/qt/5.12/5.12.5/submodules/qtscript-everywhere-src-5.12.5.zip qtscript.zip
ADD http://download.qt.io/official_releases/qt/5.12/5.12.5/submodules/qttools-everywhere-src-5.12.5.zip qttools.zip
RUN unzip qtbase.zip
RUN unzip qtscript.zip
RUN unzip qttools.zip
RUN rename qtbase-everywhere-src-5.12.5 qtbase
RUN rename qtscript-everywhere-src-5.12.5 qtscript
RUN rename qttools-everywhere-src-5.12.5 qttools
RUN build-qt.bat x86
RUN build-qt.bat x64


FROM build-env as qbs-build-env
WORKDIR C:/
COPY --from=qt-build-env C:/Qt C:/Qt/
ENV QTDIR C:\\Qt\\x86
ENV QTDIR64 C:\\Qt\\x64

# for building the documentation
RUN pip install beautifulsoup4 lxml
