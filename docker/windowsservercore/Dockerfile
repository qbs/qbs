
FROM mcr.microsoft.com/windows/servercore:1809
LABEL Description="Windows Server Core development environment for Qbs with Qt, Chocolatey and various dependencies for testing Qbs modules and functionality"

# Disable crash dialog for release-mode runtimes
RUN reg add "HKLM\SOFTWARE\Microsoft\Windows\Windows Error Reporting" /v Disabled /t REG_DWORD /d 1 /f
RUN reg add "HKLM\SOFTWARE\Microsoft\Windows\Windows Error Reporting" /v DontShowUI /t REG_DWORD /d 1 /f

RUN powershell -NoProfile -ExecutionPolicy Bypass -Command \
    $Env:chocolateyVersion = '0.10.8' ; \
    $Env:chocolateyUseWindowsCompression = 'false' ; \
    "[Net.ServicePointManager]::SecurityProtocol = \"tls12, tls11, tls\"; iex ((New-Object System.Net.WebClient).DownloadString('http://chocolatey.org/install.ps1'))" && SET "PATH=%PATH%;%ALLUSERSPROFILE%\chocolatey\bin"

# Wait for vs_installer.exe, vs_installerservice.exe
# or vs_installershell.exe because choco doesn't
RUN powershell -NoProfile -InputFormat None -Command \
    choco install visualcpp-build-tools --version 15.0.26228.20170424 -y; \
    Write-Host 'Waiting for Visual C++ Build Tools to finish'; \
    Wait-Process -Name vs_installer

ARG QBS_VERSION
RUN choco install -y python && \
    choco install -y 7zip --version 19.0 && \
    choco install -y git --version 2.24.0 --params "/GitAndUnixToolsOnPath" && \
    choco install -y qbs --version %QBS_VERSION%

# for building the documentation
RUN pip install beautifulsoup4 lxml

# clcache for speeding up MSVC builds
ENV CLCACHE_DIR="C:/.ccache"
RUN certutil -generateSSTFromWU roots.sst && \
    certutil -addstore -f root roots.sst && \
    del roots.sst && \
    pip install --trusted-host=pypi.org \
        git+https://github.com/frerich/clcache.git@cae73d8255d78db8ba11e23c51fd2c9a89e7475b

########### Install Qt #############
ARG QT_VERSION
COPY scripts/install-qt.sh install-qt.sh

RUN bash -c "./install-qt.sh -d /c/Qt --version ${QT_VERSION} --toolchain win64_msvc2017_64 qtbase qtdeclarative qttools qtscript"
ENV QTDIR64=C:\\Qt\\${QT_VERSION}\\msvc2017_64

RUN bash -c "./install-qt.sh -d /c/Qt --version ${QT_VERSION} --toolchain win32_msvc2017 qtbase qtdeclarative qttools qtscript"
ENV QTDIR=C:\\Qt\\${QT_VERSION}\\msvc2017

RUN qbs setup-toolchains --detect && \
    qbs setup-qt %QTDIR64%/bin/qmake.exe qt64 && \
    qbs setup-qt %QTDIR%/bin/qmake.exe qt && \
    qbs config defaultProfile qt64
