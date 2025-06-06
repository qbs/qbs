/****************************************************************************
**
** Copyright (C) 2015 The Qt Company Ltd.
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
import qbs.Probes
import "windows-msvc-base.qbs" as MsvcBaseModule
import 'msvc.js' as MSVC

MsvcBaseModule {
    condition: Host.os().includes('windows') &&
               qbs.targetOS.includes('windows') &&
               qbs.toolchain && qbs.toolchain.includes('msvc')
    priority: 50

    Probes.ClBinaryProbe {
        id: compilerPathProbe
        preferredArchitecture: qbs.architecture
        condition: !toolchainInstallPath && !_skipAllChecks
        names: ["cl"]
    }

    Probes.MsvcProbe {
        id: msvcProbe
        condition: !_skipAllChecks
        compilerFilePath: compilerPath
        enableDefinesByLanguage: enableCompilerDefinesByLanguage
        preferredArchitecture: qbs.architecture
        winSdkVersion: windowsSdkVersion
    }

    Properties {
        condition: msvcProbe.found
        qbs.architecture: msvcProbe.architecture
    }

    compilerVersionMajor: msvcProbe.versionMajor
    compilerVersionMinor: msvcProbe.versionMinor
    compilerVersionPatch: msvcProbe.versionPatch
    compilerIncludePaths: msvcProbe.includePaths
    compilerDefinesByLanguage: msvcProbe.compilerDefinesByLanguage

    toolchainInstallPath: compilerPathProbe.found ? compilerPathProbe.path
                                                  : undefined
    buildEnv: msvcProbe.buildEnv

    enableCxxLanguageMacro: true

    compiledModuleSuffix: ".ifc"
    moduleOutputFlag: "-ifcOutput "
    moduleFileFlag: "-reference %module%="

    stdModulesFiles: stdModulesProbe.found ? stdModulesProbe._stdModulesFiles : undefined
    Probe {
        id: stdModulesProbe
        condition: msvcProbe.found
            && !_skipAllChecks
            && stdModulesFiles === undefined
            && (forceUseImportStd || forceUseImportStdCompat)

        // input
        property string _modulesDirPath: msvcProbe.modulesPath
        property bool _forceUseImportStd : forceUseImportStd
        property bool _forceUseImportStdCompat : forceUseImportStdCompat

        // output
        property stringList _stdModulesFiles

        configure: MSVC.configureStdModules()
    }
}
