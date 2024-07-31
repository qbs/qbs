/****************************************************************************
**
** Copyright (C) 2017 The Qt Company Ltd.
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

import qbs.File
import qbs.FileInfo
import qbs.Utilities
import 'quick.js' as QC

QtModule {
    qtModuleName: @name@
    Depends { name: "Qt"; submodules: @dependencies@ }

    hasLibrary: @has_library@
    architectures: @archs@
    targetPlatform: @targetPlatform@
    staticLibsDebug: @staticLibsDebug@
    staticLibsRelease: @staticLibsRelease@
    dynamicLibsDebug: @dynamicLibsDebug@
    dynamicLibsRelease: @dynamicLibsRelease@
    linkerFlagsDebug: @linkerFlagsDebug@
    linkerFlagsRelease: @linkerFlagsRelease@
    frameworksDebug: @frameworksDebug@
    frameworksRelease: @frameworksRelease@
    frameworkPathsDebug: @frameworkPathsDebug@
    frameworkPathsRelease: @frameworkPathsRelease@
    libNameForLinkerDebug: @libNameForLinkerDebug@
    libNameForLinkerRelease: @libNameForLinkerRelease@
    libFilePathDebug: @libFilePathDebug@
    libFilePathRelease: @libFilePathRelease@
    pluginTypes: @pluginTypes@
    moduleConfig: @moduleConfig@
    cpp.defines: @defines@
    cpp.systemIncludePaths: @includes@
    cpp.libraryPaths: @libraryPaths@
    @additionalContent@

    readonly property bool _compilerIsQmlCacheGen: Utilities.versionCompare(Qt.core.version,
                                                                            "5.11") >= 0
    readonly property bool _supportsQmlJsFiltering: Utilities.versionCompare(Qt.core.version,
                                                                             "5.15") < 0
    readonly property string _generatedLoaderFileName: _compilerIsQmlCacheGen
                                                       ? "qmlcache_loader.cpp"
                                                       : "qtquickcompiler_loader.cpp"
    property string _compilerBaseDir: _compilerIsQmlCacheGen ? Qt.core.qmlLibExecPath
                                                             : Qt.core.binPath
    property string compilerBaseName: (_compilerIsQmlCacheGen ? "qmlcachegen" : "qtquickcompiler")
    property string compilerFilePath: FileInfo.joinPaths(_compilerBaseDir,
                                        compilerBaseName + FileInfo.executableSuffix())

    property bool compilerAvailable: File.exists(compilerFilePath);
    property bool useCompiler: compilerAvailable && !_compilerIsQmlCacheGen

    Group {
        condition: useCompiler

        FileTagger {
            patterns: "*.qrc"
            fileTags: ["qt.quick.qrc"]
            priority: 100
        }

        Scanner {
            inputs: 'qt.quick.qrc'
            searchPaths: [FileInfo.path(input.filePath)]
            scan: QC.scanQrc(product, input.filePath)
        }

        Rule {
            inputs: ["qt.quick.qrc"]
            Artifact {
                filePath: input.fileName + ".json"
                fileTags: ["qt.quick.qrcinfo"]
            }
            prepare: QC.generateCompilerInputCommands(product, input, output)
        }

        Rule {
            inputs: ["qt.quick.qrcinfo"]
            outputFileTags: ["cpp", "qrc"]
            multiplex: true
            outputArtifacts: QC.compilerOutputArtifacts(product, inputs)
            prepare: QC.compilerCommands.apply(QC, arguments)
        }
    }
}
