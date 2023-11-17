/****************************************************************************
**
** Copyright (C) 2019 Ivan Komissarov (abbapoh@gmail.com)
** Contact: http://www.qt.io/licensing
**
** This file is part of Qbs.
**
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms and
** conditions see http://www.qt.io/terms-conditions. For further information
** use the contact form at http://www.qt.io/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 2.1 or version 3 as published by the Free
** Software Foundation and appearing in the file LICENSE.LGPLv21 and
** LICENSE.LGPLv3 included in the packaging of this file.  Please review the
** following information to ensure the GNU Lesser General Public License
** requirements will be met: https://www.gnu.org/licenses/lgpl.html and
** http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html.
**
** In addition, as a special exception, The Qt Company gives you certain additional
** rights.  These rights are described in The Qt Company LGPL Exception
** version 1.1, included in the file LGPL_EXCEPTION.txt in this package.
**
****************************************************************************/

import qbs.Host
import qbs.ModUtils
import qbs.Probes
import qbs.FileInfo
import 'windows-msvc-base.qbs' as MsvcBaseModule

MsvcBaseModule {
    condition: Host.os().includes('windows') &&
               qbs.targetOS.includes('windows') &&
               qbs.toolchain && qbs.toolchain.includes('clang-cl')
    priority: 100

    Probes.ClangClBinaryProbe {
        id: clangPathProbe
        condition: !toolchainInstallPath && !_skipAllChecks
        names: ["clang-cl"]
    }

    Probes.ClangClProbe {
        id: clangClProbe
        condition: !_skipAllChecks
        compilerFilePath: compilerPath
        vcvarsallFilePath: vcvarsallPath
        enableDefinesByLanguage: enableCompilerDefinesByLanguage
        preferredArchitecture: qbs.architecture
        winSdkVersion: windowsSdkVersion
    }

    qbs.architecture: clangClProbe.found ? clangClProbe.architecture : original

    compilerVersionMajor: clangClProbe.versionMajor
    compilerVersionMinor: clangClProbe.versionMinor
    compilerVersionPatch: clangClProbe.versionPatch
    compilerIncludePaths: clangClProbe.includePaths
    compilerDefinesByLanguage: clangClProbe.compilerDefinesByLanguage

    toolchainInstallPath: clangPathProbe.found ? clangPathProbe.path
                                               : undefined
    buildEnv: clangClProbe.buildEnv

    property string linkerVariant
    PropertyOptions {
        name: "linkerVariant"
        allowedValues: ["lld", "link"]
        description: "Allows to specify the linker variant. Maps to clang-cl's -fuse-ld option."
    }
    Properties {
        condition: linkerVariant
        driverLinkerFlags: "-fuse-ld=" + linkerVariant
    }

    property string vcvarsallPath : clangPathProbe.found ? clangPathProbe.vcvarsallPath : undefined

    compilerName: "clang-cl.exe"
    linkerName: "lld-link.exe"
    linkerPath: FileInfo.joinPaths(toolchainInstallPath, linkerName)

    systemIncludeFlag: "/imsvc"

    validateFunc: {
        var baseFunc = base;
        return function() {
            if (_skipAllChecks)
                return;
            var validator = new ModUtils.PropertyValidator("cpp");
            validator.setRequiredProperty("vcvarsallPath", vcvarsallPath);
            validator.validate();
            baseFunc();
        }
    }
}
