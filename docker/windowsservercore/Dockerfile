
FROM mcr.microsoft.com/windows/servercore:ltsc2022
LABEL Description="Windows Server Core 2022 development environment for Qbs with Qt, Chocolatey and various dependencies for testing Qbs modules and functionality"

# Disable crash dialog for release-mode runtimes
RUN reg add "HKLM\SOFTWARE\Microsoft\Windows\Windows Error Reporting" /v Disabled /t REG_DWORD /d 1 /f
RUN reg add "HKLM\SOFTWARE\Microsoft\Windows\Windows Error Reporting" /v DontShowUI /t REG_DWORD /d 1 /f

# Install VS 2019 from the website since chocolatey has broken .NET 4.8 (dotnetfx package) which is a
# dependency for the visualstudio2019buildtools package
RUN powershell -NoProfile -ExecutionPolicy Bypass -Command \
    Invoke-WebRequest "https://aka.ms/vs/17/release/vs_community.exe" \
    -OutFile "%TEMP%\vs_community.exe" -UseBasicParsing

RUN "%TEMP%\vs_community.exe"  --quiet --wait --norestart --noUpdateInstaller \
    --add Microsoft.VisualStudio.Component.VC.Tools.x86.x64 \
    --add Microsoft.VisualStudio.Component.Windows10SDK.19041

RUN powershell -NoProfile -ExecutionPolicy Bypass -Command \
    $Env:chocolateyVersion = '0.10.15' ; \
    $Env:chocolateyUseWindowsCompression = 'false' ; \
    "[Net.ServicePointManager]::SecurityProtocol = \"tls12, tls11, tls\"; iex ((New-Object System.Net.WebClient).DownloadString('http://chocolatey.org/install.ps1'))" && SET "PATH=%PATH%;%ALLUSERSPROFILE%\chocolatey\bin"

RUN choco install -y python --version 3.13.6 && \
    choco install -y 7zip --version 25.1.0 && \
    choco install -y git --version 2.50.1 --params "/GitAndUnixToolsOnPath" && \
    choco install -y vswhere

RUN python.exe -m pip install --upgrade pip

# for building the documentation
RUN python.exe -m pip install beautifulsoup4 lxml

########### Install Qt #############
ARG QT_VERSION
COPY scripts/install-qt.sh install-qt.sh

RUN bash -c "./install-qt.sh -d /c/Qt --version ${QT_VERSION} --toolchain win64_msvc2022_64 qtbase qtdeclarative qttools qt5compat"
ENV QTDIR64=C:\\Qt\\${QT_VERSION}\\msvc2022_64

########### Install Qbs #############
ARG QBS_VERSION
RUN choco install -y qbs --version %QBS_VERSION%

RUN qbs setup-toolchains --detect && \
    qbs setup-qt %QTDIR64%/bin/qmake.exe qt64 && \
    qbs config defaultProfile qt64
