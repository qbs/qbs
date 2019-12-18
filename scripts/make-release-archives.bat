@echo off

REM Copyright (C) 2017 The Qt Company Ltd.
REM Contact: https://www.qt.io/licensing/
REM
REM This file is part of Qbs.
REM
REM $QT_BEGIN_LICENSE:LGPL$
REM Commercial License Usage
REM Licensees holding valid commercial Qt licenses may use this file in
REM accordance with the commercial license agreement provided with the
REM Software or, alternatively, in accordance with the terms contained in
REM a written agreement between you and The Qt Company. For licensing terms
REM and conditions see https://www.qt.io/terms-conditions. For further
REM information use the contact form at https://www.qt.io/contact-us.
REM
REM GNU Lesser General Public License Usage
REM Alternatively, this file may be used under the terms of the GNU Lesser
REM General Public License version 3 as published by the Free Software
REM Foundation and appearing in the file LICENSE.LGPL3 included in the
REM packaging of this file. Please review the following information to
REM ensure the GNU Lesser General Public License version 3 requirements
REM will be met: https://www.gnu.org/licenses/lgpl-3.0.html.
REM
REM GNU General Public License Usage
REM Alternatively, this file may be used under the terms of the GNU
REM General Public License version 2.0 or (at your option) the GNU General
REM Public license version 3 or any later version approved by the KDE Free
REM Qt Foundation. The licenses are as published by the Free Software
REM Foundation and appearing in the file LICENSE.GPL2 and LICENSE.GPL3
REM included in the packaging of this file. Please review the following
REM information to ensure the GNU General Public License requirements will
REM be met: https://www.gnu.org/licenses/gpl-2.0.html and
REM https://www.gnu.org/licenses/gpl-3.0.html.
REM
REM $QT_END_LICENSE$

setlocal enabledelayedexpansion || exit /b
if not exist VERSION ( echo This script must be run from the qbs source directory 1>&2 && exit /b 1 )
for /f %%j in (VERSION) do ( set "version=!version!%%j," )
set "version=%version:~0,-1%"

set builddir=%TEMP%\qbs-release-%version%
if exist "%builddir%" ( del /s /q "%builddir%" || exit /b )

qbs setup-toolchains --settings-dir "%builddir%\.settings" --detect || exit /b

if exist "%QTDIR%" (
    qbs setup-qt --settings-dir "%builddir%\.settings"^
     "%QTDIR%\bin\qmake.exe" qt || exit /b
) else (
    echo QTDIR environment variable not set or does not exist: %QTDIR%
    exit /b 1
)
if exist "%QTDIR64%" (
    qbs setup-qt --settings-dir "%builddir%\.settings"^
     "%QTDIR64%\bin\qmake.exe" qt64 || exit /b
) else (
    echo QTDIR64 environment variable not set or does not exist: %QTDIR64%
    exit /b 1
)

REM Work around QBS-1142, where symlinks to UNC named paths aren't resolved
REM properly, for example if this command is being run in a Docker container
REM where the current directory is a symlink
subst Q: "%CD%" && Q:

qbs build --settings-dir "%builddir%\.settings"^
 -f qbs.qbs -d "%builddir%\build"^
 -p dist qbs.buildVariant:release project.withDocumentation:false "products.qbs archive.includeTopLevelDir:true"^
 modules.qbsbuildconfig.enableBundledQt:true^
 config:release "qbs.installRoot:%builddir%\qbs-windows-x86-%version%" profile:qt^
 config:release-64 "qbs.installRoot:%builddir%\qbs-windows-x86_64-%version%" profile:qt64 || exit /b

copy /y "%builddir%\build\release\qbs.%version%.nupkg" dist || exit /b
copy /y "%builddir%\build\release\qbs-windows-x86-%version%.zip" dist || exit /b
copy /y "%builddir%\build\release-64\qbs-windows-x86_64-%version%.zip" dist || exit /b
