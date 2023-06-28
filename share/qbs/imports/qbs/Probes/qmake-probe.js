/****************************************************************************
**
** Copyright (C) 2019 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of Qbs.
**
** $QT_BEGIN_LICENSE:LGPL$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see https://www.qt.io/terms-conditions. For further
** information use the contact form at https://www.qt.io/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 3 as published by the Free Software
** Foundation and appearing in the file LICENSE.LGPL3 included in the
** packaging of this file. Please review the following information to
** ensure the GNU Lesser General Public License version 3 requirements
** will be met: https://www.gnu.org/licenses/lgpl-3.0.html.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 2.0 or (at your option) the GNU General
** Public license version 3 or any later version approved by the KDE Free
** Qt Foundation. The licenses are as published by the Free Software
** Foundation and appearing in the file LICENSE.GPL2 and LICENSE.GPL3
** included in the packaging of this file. Please review the following
** information to ensure the GNU General Public License requirements will
** be met: https://www.gnu.org/licenses/gpl-2.0.html and
** https://www.gnu.org/licenses/gpl-3.0.html.
**
** $QT_END_LICENSE$
**
****************************************************************************/

var Environment = require("qbs.Environment");
var File = require("qbs.File");
var FileInfo = require("qbs.FileInfo");
var Host = require("qbs.Host");
var Process = require("qbs.Process");
var ProviderUtils = require("qbs.ProviderUtils");
var TextFile = require("qbs.TextFile");
var Utilities = require("qbs.Utilities");

function splitNonEmpty(s, c) { return s.split(c).filter(function(e) { return e; }); }

function getQmakeFilePaths(qmakeFilePaths) {
    if (qmakeFilePaths && qmakeFilePaths.length > 0)
        return qmakeFilePaths;
    console.info("Detecting Qt installations...");
    var filePaths = [];
    var pathValue = Environment.getEnv("PATH");
    if (pathValue) {
        var dirs = splitNonEmpty(pathValue, FileInfo.pathListSeparator());
        for (var i = 0; i < dirs.length; ++i) {
            var candidate = FileInfo.joinPaths(dirs[i], "qmake" + FileInfo.executableSuffix());
            var canonicalCandidate = FileInfo.canonicalPath(candidate);
            if (!canonicalCandidate || !File.exists(canonicalCandidate))
                continue;
            if (FileInfo.completeBaseName(canonicalCandidate) !== "qtchooser")
                candidate = canonicalCandidate;
            if (!filePaths.contains(candidate)) {
                console.info("Found Qt at '" + FileInfo.toNativeSeparators(candidate) + "'.");
                filePaths.push(candidate);
            }
        }
    }
    if (filePaths.length === 0) {
        console.warn("Could not find any qmake executables in PATH. Either make sure a qmake "
        + "executable is present in PATH or set the moduleProviders.Qt.qmakeFilePaths property "
        + "to point a qmake executable.");
    }
    return filePaths;
}

function queryQmake(qmakeFilePath) {
    var qmakeProcess = new Process;
    qmakeProcess.exec(qmakeFilePath, ["-query"]);
    if (qmakeProcess.exitCode() !== 0) {
        throw "The qmake executable '" + FileInfo.toNativeSeparators(qmakeFilePath)
                + "' failed with exit code " + qmakeProcess.exitCode() + ".";
    }
    var queryResult = {};
    while (!qmakeProcess.atEnd()) {
        var line = qmakeProcess.readLine();
        var index = (line || "").indexOf(":");
        if (index !== -1)
            queryResult[line.slice(0, index)] = line.slice(index + 1).trim();
    }
    return queryResult;
}

function pathQueryValue(queryResult, key) {
    var p = queryResult[key];
    if (p)
        return FileInfo.fromNativeSeparators(p);
}

function readFileContent(filePath) {
    var f = new TextFile(filePath, TextFile.ReadOnly);
    var content = f.readAll();
    f.close();
    return content;
}

// TODO: Don't do the split every time...
function configVariable(configContent, key) {
    var configContentLines = configContent.split('\n');
    var regexp = new RegExp("^\\s*" + key + "\\s*\\+{0,1}=(.*)");
    for (var i = 0; i < configContentLines.length; ++i) {
        var line = configContentLines[i];
        var match = regexp.exec(line);
        if (match)
            return match[1].trim();
    }
}

function configVariableItems(configContent, key) {
    return splitNonEmpty(configVariable(configContent, key), ' ');
}

function msvcCompilerVersionForYear(year) {
    var mapping = {
        "2005": "14", "2008": "15", "2010": "16", "2012": "17", "2013": "18", "2015": "19",
        "2017": "19.1", "2019": "19.2"
    };
    return mapping[year];
}

function msvcCompilerVersionFromMkspecName(mkspecName) {
    return msvcCompilerVersionForYear(mkspecName.slice(msvcPrefix().length));
}

function addQtBuildVariant(qtProps, buildVariantName) {
    if (qtProps.qtConfigItems.contains(buildVariantName))
        qtProps.buildVariant.push(buildVariantName);
}

function checkForStaticBuild(qtProps) {
    if (qtProps.qtMajorVersion >= 5)
        return qtProps.qtConfigItems.contains("static");
    if (qtProps.frameworkBuild)
        return false; // there are no Qt4 static frameworks
    var isWin = qtProps.mkspecName.startsWith("win");
    var libDir = isWin ? qtProps.binaryPath : qtProps.libraryPath;
    var coreLibFiles = File.directoryEntries(libDir, File.Files)
        .filter(function(fp) { return fp.contains("Core"); });
    if (coreLibFiles.length === 0)
        throw "Could not determine whether Qt is a static build.";
    for (var i = 0; i < coreLibFiles.length; ++i) {
        if (Utilities.isSharedLibrary(coreLibFiles[i]))
            return false;
    }
    return true;
}

function guessMinimumWindowsVersion(qtProps) {
    if (qtProps.mkspecName.startsWith("winrt-"))
        return "10.0";
    if (!ProviderUtils.isDesktopWindowsQt(qtProps))
        return "";
    if (qtProps.qtMajorVersion >= 6)
        return "10.0";
    if (qtProps.architecture === "x86_64" || qtProps.architecture === "ia64")
        return "5.2"
    var match = qtProps.mkspecName.match(/^win32-msvc(\d+)$/);
    if (match) {
        var msvcVersion = match[1];
        if (msvcVersion < 2012)
            return "5.0";
        return "5.1";
    }
    return qtProps.qtMajorVersion < 5 ? "5.0" : "5.1";
}

function fillEntryPointLibs(qtProps, debug) {
    result = [];
    var isMinGW = ProviderUtils.isMinGwQt(qtProps);

    // Some Linux distributions rename the qtmain library.
    var qtMainCandidates = ["qtmain"];
    if (isMinGW && qtProps.qtMajorVersion === 5)
        qtMainCandidates.push("qt5main");
    if (qtProps.qtMajorVersion === 6)
        qtMainCandidates.push("Qt6EntryPoint");

    for (var i = 0; i < qtMainCandidates.length; ++i) {
        var baseNameCandidate = qtMainCandidates[i];
        var qtmain = qtProps.libraryPath + '/';
        if (isMinGW)
            qtmain += "lib";
        qtmain += baseNameCandidate + qtProps.qtLibInfix;
        if (debug && ProviderUtils.qtNeedsDSuffix(qtProps))
            qtmain += 'd';
        if (isMinGW) {
            qtmain += ".a";
        } else {
            qtmain += ".lib";
            if (Utilities.versionCompare(qtProps.qtVersion, "5.4.0") >= 0)
                result.push("Shell32.lib");
        }
        if (File.exists(qtmain)) {
            result.push(qtmain);
            break;
        }
    }
    if (result.length === 0) {
        console.warn("Could not find the qtmain library at '"
                     + FileInfo.toNativeSeparators(qtProps.libraryPath)
                     + "'. You will not be able to link Qt applications.");
    }
    return result;
}

function getQtProperties(qmakeFilePath) {
    var queryResult = queryQmake(qmakeFilePath);
    var qtProps = {};
    qtProps.installPrefixPath = pathQueryValue(queryResult, "QT_INSTALL_PREFIX");
    qtProps.documentationPath = pathQueryValue(queryResult, "QT_INSTALL_DOCS");
    qtProps.includePath = pathQueryValue(queryResult, "QT_INSTALL_HEADERS");
    qtProps.libraryPath = pathQueryValue(queryResult, "QT_INSTALL_LIBS");
    qtProps.hostLibraryPath = pathQueryValue(queryResult, "QT_HOST_LIBS");
    qtProps.binaryPath = pathQueryValue(queryResult, "QT_HOST_BINS")
            || pathQueryValue(queryResult, "QT_INSTALL_BINS");
    qtProps.installPath = pathQueryValue(queryResult, "QT_INSTALL_BINS");
    qtProps.documentationPath = pathQueryValue(queryResult, "QT_INSTALL_DOCS");
    qtProps.pluginPath = pathQueryValue(queryResult, "QT_INSTALL_PLUGINS");
    qtProps.qmlPath = pathQueryValue(queryResult, "QT_INSTALL_QML");
    qtProps.qmlImportPath = pathQueryValue(queryResult, "QT_INSTALL_IMPORTS");
    qtProps.qtVersion = queryResult.QT_VERSION;

    var mkspecsBaseSrcPath;
    if (Utilities.versionCompare(qtProps.qtVersion, "5") >= 0) {
        qtProps.mkspecBasePath = FileInfo.joinPaths(pathQueryValue(queryResult, "QT_HOST_DATA"),
                                                    "mkspecs");
        mkspecsBaseSrcPath = FileInfo.joinPaths(pathQueryValue(queryResult, "QT_HOST_DATA/src"),
                                                "mkspecs");
    } else {
        qtProps.mkspecBasePath = FileInfo.joinPaths
                (pathQueryValue(queryResult, "QT_INSTALL_DATA"), "mkspecs");
    }

    if (Utilities.versionCompare(qtProps.qtVersion, "6") >= 0) {
        qtProps.libExecPath = pathQueryValue(queryResult, "QT_HOST_LIBEXECS")
                || pathQueryValue(queryResult, "QT_INSTALL_LIBEXECS");
    }

    // QML tools were only moved in Qt 6.2.
    qtProps.qmlLibExecPath = Utilities.versionCompare(qtProps.qtVersion, "6.2") >= 0
        ? qtProps.libExecPath : qtProps.binaryPath;

    // qhelpgenerator was only moved in Qt 6.3.
    qtProps.helpGeneratorLibExecPath = Utilities.versionCompare(qtProps.qtVersion, "6.3") >= 0
        ? qtProps.libExecPath : qtProps.binaryPath;

    if (!File.exists(qtProps.mkspecBasePath))
        throw "Cannot extract the mkspecs directory.";

    var qconfigContent = readFileContent(FileInfo.joinPaths(qtProps.mkspecBasePath,
                                                            "qconfig.pri"));
    qtProps.qtMajorVersion = parseInt(configVariable(qconfigContent, "QT_MAJOR_VERSION"));
    qtProps.qtMinorVersion = parseInt(configVariable(qconfigContent, "QT_MINOR_VERSION"));
    qtProps.qtPatchVersion = parseInt(configVariable(qconfigContent, "QT_PATCH_VERSION"));
    qtProps.qtNameSpace = configVariable(qconfigContent, "QT_NAMESPACE");
    qtProps.qtLibInfix = configVariable(qconfigContent, "QT_LIBINFIX") || "";
    qtProps.architecture = configVariable(qconfigContent, "QT_TARGET_ARCH")
            || configVariable(qconfigContent, "QT_ARCH") || "x86";
    qtProps.configItems = configVariableItems(qconfigContent, "CONFIG");
    qtProps.qtConfigItems = configVariableItems(qconfigContent, "QT_CONFIG");

    // retrieve the mkspec
    if (qtProps.qtMajorVersion >= 5) {
        qtProps.mkspecName = queryResult.QMAKE_XSPEC;
        qtProps.mkspecPath = FileInfo.joinPaths(qtProps.mkspecBasePath, qtProps.mkspecName);
        if (mkspecsBaseSrcPath && !File.exists(qtProps.mkspecPath))
            qtProps.mkspecPath = FileInfo.joinPaths(mkspecsBaseSrcPath, qtProps.mkspecName);
    } else {
        if (Host.os().contains("windows")) {
            var baseDirPath = FileInfo.joinPaths(qtProps.mkspecBasePath, "default");
            var fileContent = readFileContent(FileInfo.joinPaths(baseDirPath, "qmake.conf"));
            qtProps.mkspecPath = configVariable(fileContent, "QMAKESPEC_ORIGINAL");
            if (!File.exists(qtProps.mkspecPath)) {
                // Work around QTBUG-28792.
                // The value of QMAKESPEC_ORIGINAL is wrong for MinGW packages. Y u h8 me?
                var match = fileContent.exec(/\binclude\(([^)]+)\/qmake\.conf\)/m);
                if (match) {
                    qtProps.mkspecPath = FileInfo.cleanPath(FileInfo.joinPaths(
                                                                   baseDirPath, match[1]));
                }
            }
        } else {
            qtProps.mkspecPath = FileInfo.canonicalPath(
                        FileInfo.joinPaths(qtProps.mkspecBasePath, "default"));
        }

        // E.g. in qmake.conf for Qt 4.8/mingw we find this gem:
        //    QMAKESPEC_ORIGINAL=C:\\Qt\\Qt\\4.8\\mingw482\\mkspecs\\win32-g++
        qtProps.mkspecPath = FileInfo.cleanPath(qtProps.mkspecPath);

        qtProps.mkspecName = qtProps.mkspecPath;
        var idx = qtProps.mkspecName.lastIndexOf('/');
        if (idx !== -1)
            qtProps.mkspecName = qtProps.mkspecName.slice(idx + 1);
    }
    if (!File.exists(qtProps.mkspecPath))
        throw "mkspec '" + FileInfo.toNativeSeparators(qtProps.mkspecPath) + "' does not exist";

    // Starting with qt 5.14, android sdk provides multi-abi
    if (Utilities.versionCompare(qtProps.qtVersion, "5.14.0") >= 0
            && qtProps.mkspecPath.contains("android")) {
        var qdeviceContent = readFileContent(FileInfo.joinPaths(qtProps.mkspecBasePath,
                                                                "qdevice.pri"));
        qtProps.androidAbis = configVariable(qdeviceContent, "DEFAULT_ANDROID_ABIS").split(' ');
    }

    // determine MSVC version
    if (ProviderUtils.isMsvcQt(qtProps)) {
        var msvcMajor = configVariable(qconfigContent, "QT_MSVC_MAJOR_VERSION");
        var msvcMinor = configVariable(qconfigContent, "QT_MSVC_MINOR_VERSION");
        var msvcPatch = configVariable(qconfigContent, "QT_MSVC_PATCH_VERSION");
        if (msvcMajor && msvcMinor && msvcPatch)
            qtProps.msvcVersion = msvcMajor + "." + msvcMinor + "." + msvcPatch;
        else
            qtProps.msvcVersion = msvcCompilerVersionFromMkspecName(qtProps.mkspecName);
    }

    // determine whether we have a framework build
    qtProps.frameworkBuild = qtProps.mkspecPath.contains("macx")
            && qtProps.configItems.contains("qt_framework");

    // determine whether Qt is built with debug, release or both
    qtProps.buildVariant = [];
    addQtBuildVariant(qtProps, "debug");
    addQtBuildVariant(qtProps, "release");

    qtProps.staticBuild = checkForStaticBuild(qtProps);

    // determine whether user apps require C++11
    if (qtProps.qtConfigItems.contains("c++11") && qtProps.staticBuild)
        qtProps.configItems.push("c++11");

    // Set the minimum operating system versions appropriate for this Qt version
    qtProps.windowsVersion = guessMinimumWindowsVersion(qtProps);
    if (qtProps.windowsVersion) {    // Is target OS Windows?
        if (qtProps.buildVariant.contains("debug"))
            qtProps.entryPointLibsDebug = fillEntryPointLibs(qtProps, true);
        if (qtProps.buildVariant.contains("release"))
            qtProps.entryPointLibsRelease = fillEntryPointLibs(qtProps, false);
    } else if (qtProps.mkspecPath.contains("macx")) {
        if (qtProps.qtMajorVersion >= 5) {
            var lines = getFileContentsRecursively(FileInfo.joinPaths(qtProps.mkspecPath,
                                                                      "qmake.conf"));
            for (var i = 0; i < lines.length; ++i) {
                var line = lines[i].trim();
                match = line.match
                        (/^QMAKE_(MACOSX|IOS|TVOS|WATCHOS)_DEPLOYMENT_TARGET\s*=\s*(.*)\s*$/);
                if (match) {
                    var platform = match[1];
                    var version = match[2];
                    if (platform === "MACOSX")
                        qtProps.macosVersion = version;
                    else if (platform === "IOS")
                        qtProps.iosVersion = version;
                    else if (platform === "TVOS")
                        qtProps.tvosVersion = version;
                    else if (platform === "WATCHOS")
                        qtProps.watchosVersion = version;
                }
            }
            var isMac = qtProps.mkspecName !== "macx-ios-clang"
                    && qtProps.mkspecName !== "macx-tvos-clang"
                    && qtProps.mkspecName !== "macx-watchos-clang";
            if (isMac) {
                // Qt 5.0.x placed the minimum version in a different file
                if (!qtProps.macosVersion)
                    qtProps.macosVersion = "10.6";

                // If we're using C++11 with libc++, make sure the deployment target is >= 10.7
                if (Utilities.versionCompare(qtProps.macosVersion, "10, 7") < 0
                        && qtProps.qtConfigItems.contains("c++11")) {
                    qtProps.macosVersion = "10.7";
                }
            }
        } else if (qtProps.qtMajorVersion === 4 && qtProps.qtMinorVersion >= 6) {
            var qconfigDir = qtProps.frameworkBuild
                    ? FileInfo.joinPaths(qtProps.libraryPath, "QtCore.framework", "Headers")
                    : FileInfo.joinPaths(qtProps.includePath, "Qt");
            try {
                var qconfig = new TextFile(FileInfo.joinPaths(qconfigDir, "qconfig.h"),
                                           TextFile.ReadOnly);
                var qtCocoaBuild = false;
                var ok = true;
                do {
                    line = qconfig.readLine();
                    if (line.match(/\s*#define\s+QT_MAC_USE_COCOA\s+1\s*/)) {
                        qtCocoaBuild = true;
                        break;
                    }
                } while (!qconfig.atEof());
                qtProps.macosVersion = qtCocoaBuild ? "10.5" : "10.4";
            }
            catch (e) {}
            finally {
                if (qconfig)
                    qconfig.close();
            }
            if (!qtProps.macosVersion) {
                throw "Could not determine whether Qt is using Cocoa or Carbon from '"
                        + FileInfo.toNativeSeparators(qconfig.filePath()) + "'.";
            }
        }
    } else if (qtProps.mkspecPath.contains("android")) {
        if (qtProps.qtMajorVersion >= 5)
            qtProps.androidVersion = "2.3";
        else if (qtProps.qtMajorVersion === 4 && qtProps.qtMinorVersion >= 8)
            qtProps.androidVersion = "1.6"; // Necessitas
    }
    return qtProps;
}

function makePluginData() {
    var pluginData = {};
    pluginData.type = undefined;
    pluginData.className = undefined;
    pluginData.autoLoad = true;
    pluginData["extends"] = [];
    return pluginData;
}

function makeQtModuleInfo(name, qbsName, deps) {
    var moduleInfo = {};
    moduleInfo.name = name; // As in the path to the headers and ".name" in the pri files.
    if (moduleInfo.name === undefined)
        moduleInfo.name = "";
    moduleInfo.qbsName = qbsName; // Lower-case version without "qt" prefix.
    moduleInfo.dependencies = deps || []; // qbs names.
    if (moduleInfo.qbsName && moduleInfo.qbsName !== "core"
            && !moduleInfo.dependencies.contains("core")) {
        moduleInfo.dependencies.unshift("core");
    }
    moduleInfo.isPrivate = qbsName && qbsName.endsWith("-private");
    moduleInfo.hasLibrary = !moduleInfo.isPrivate;
    moduleInfo.isStaticLibrary = false;
    moduleInfo.isPlugin = false;
    moduleInfo.mustExist = true;
    moduleInfo.modulePrefix = ""; // empty value means "Qt".
    moduleInfo.version = undefined;
    moduleInfo.includePaths = [];
    moduleInfo.compilerDefines = [];
    moduleInfo.staticLibrariesDebug = [];
    moduleInfo.staticLibrariesRelease = [];
    moduleInfo.dynamicLibrariesDebug = [];
    moduleInfo.dynamicLibrariesRelease = [];
    moduleInfo.linkerFlagsDebug = [];
    moduleInfo.linkerFlagsRelease = [];
    moduleInfo.libFilePathDebug = undefined;
    moduleInfo.libFilePathRelease = undefined;
    moduleInfo.frameworksDebug = [];
    moduleInfo.frameworksRelease = [];
    moduleInfo.frameworkPathsDebug = [];
    moduleInfo.frameworkPathsRelease = [];
    moduleInfo.libraryPaths = [];
    moduleInfo.libDir = "";
    moduleInfo.config = [];
    moduleInfo.supportedPluginTypes = [];
    moduleInfo.pluginData = makePluginData();
    return moduleInfo;
}

function frameworkHeadersPath(qtModuleInfo, qtProps) {
    return FileInfo.joinPaths(qtProps.libraryPath, qtModuleInfo.name + ".framework", "Headers");
}

function qt4ModuleIncludePaths(qtModuleInfo, qtProps) {
    var paths = [];
    if (ProviderUtils.qtIsFramework(qtModuleInfo, qtProps))
        paths.push(frameworkHeadersPath(qtModuleInfo, qtProps));
    else
        paths.push(qtProps.includePath, FileInfo.joinPaths(qtProps.includePath, qtModuleInfo.name));
    return paths;
}

// We erroneously called the "testlib" module "test" for quite a while. Let's not punish users
// for that.
function addTestModule(modules) {
    var testModule = makeQtModuleInfo("QtTest", "test", ["testlib"]);
    testModule.hasLibrary = false;
    modules.push(testModule);
}

// See above.
function addDesignerComponentsModule(modules) {
    var module = makeQtModuleInfo("QtDesignerComponents", "designercomponents",
                                  ["designercomponents-private"]);
    module.hasLibrary = false;
    modules.push(module);
}

function guessLibraryFilePath(prlFilePath, libDir, qtProps) {
    var baseName = FileInfo.baseName(prlFilePath);
    var prefixCandidates = ["", "lib"];
    var suffixCandidates = ["so." + qtProps.qtVersion, "so", "a", "lib", "dll.a"];
    for (var i = 0; i < prefixCandidates.length; ++i) {
        var prefix = prefixCandidates[i];
        for (var j = 0; j < suffixCandidates.length; ++j) {
            var suffix = suffixCandidates[j];
            var candidate = FileInfo.joinPaths(libDir, prefix + baseName + '.' + suffix);
            if (File.exists(candidate))
                return candidate;
        }
    }
}

function doReplaceQtLibNamesWithFilePath(namePathMap, libList) {
    for (var i = 0; i < libList.length; ++i) {
        var lib = libList[i];
        var path = namePathMap[lib];
        if (path)
            libList[i] = path;
    }
}

function replaceQtLibNamesWithFilePath(modules, qtProps) {
    // We don't want to add the libraries for Qt modules via "-l", because of the
    // danger that a wrong one will be picked up, e.g. from /usr/lib. Instead,
    // we pull them in using the full file path.
    var linkerNamesToFilePathsDebug = {};
    var linkerNamesToFilePathsRelease = {};
    for (var i = 0; i < modules.length; ++i) {
        var m = modules[i];
        linkerNamesToFilePathsDebug[
            ProviderUtils.qtLibNameForLinker(m, qtProps, true)] = m.libFilePathDebug;
        linkerNamesToFilePathsRelease[
            ProviderUtils.qtLibNameForLinker(m, qtProps, false)] = m.libFilePathRelease;
    }
    for (i = 0; i < modules.length; ++i) {
        var module = modules[i];
        doReplaceQtLibNamesWithFilePath(linkerNamesToFilePathsDebug, module.dynamicLibrariesDebug);
        doReplaceQtLibNamesWithFilePath(linkerNamesToFilePathsDebug, module.staticLibrariesDebug);
        doReplaceQtLibNamesWithFilePath(linkerNamesToFilePathsRelease,
                                        module.dynamicLibrariesRelease);
        doReplaceQtLibNamesWithFilePath(linkerNamesToFilePathsRelease,
                                        module.staticLibrariesRelease);
    }
}

function doSetupLibraries(modInfo, qtProps, debugBuild, nonExistingPrlFiles, androidAbi) {
    if (!modInfo.hasLibrary)
        return; // Can happen for Qt4 convenience modules, like "widgets".

    if (debugBuild) {
        if (!qtProps.buildVariant.contains("debug"))
            return;
        var modulesNeverBuiltAsDebug = ["bootstrap", "qmldevtools"];
        for (var i = 0; i < modulesNeverBuiltAsDebug.length; ++i) {
            var m = modulesNeverBuiltAsDebug[i];
            if (modInfo.qbsName === m || modInfo.qbsName === m + "-private")
                return;
        }
    } else if (!qtProps.buildVariant.contains("release")) {
        return;
    }

    var libs = modInfo.isStaticLibrary
            ? (debugBuild ? modInfo.staticLibrariesDebug : modInfo.staticLibrariesRelease)
            : (debugBuild ? modInfo.dynamicLibrariesDebug : modInfo.dynamicLibrariesRelease);
    var frameworks = debugBuild ? modInfo.frameworksDebug : modInfo.frameworksRelease;
    var frameworkPaths = debugBuild ? modInfo.frameworkPathsDebug : modInfo.frameworkPathsRelease;
    var flags = debugBuild ? modInfo.linkerFlagsDebug : modInfo.linkerFlagsRelease;
    var libFilePath;

    if (qtProps.mkspecName.contains("ios") && modInfo.isStaticLibrary) {
        libs.push("z", "m");
        if (qtProps.qtMajorVersion === 5 && qtProps.qtMinorVersion < 8) {
            var platformSupportModule = makeQtModuleInfo("QtPlatformSupport", "platformsupport");
            libs.push(ProviderUtils.qtLibNameForLinker(platformSupportModule, qtProps, debugBuild));
        }
        if (modInfo.name === "qios") {
            flags.push("-force_load", FileInfo.joinPaths(
                           qtProps.pluginPath, "platforms",
                           ProviderUtils.qtLibBaseName(
                                modInfo, "libqios", debugBuild, qtProps) + ".a"));
        }
    }
    var prlFilePath = modInfo.isPlugin
            ? FileInfo.joinPaths(qtProps.pluginPath, modInfo.pluginData.type)
            : (modInfo.libDir ? modInfo.libDir : qtProps.libraryPath);
    var libDir = prlFilePath;
    if (ProviderUtils.qtIsFramework(modInfo, qtProps)) {
        prlFilePath = FileInfo.joinPaths(
            prlFilePath,
            ProviderUtils.qtLibraryBaseName(modInfo, qtProps, false) + ".framework");
        libDir = prlFilePath;
        if (Utilities.versionCompare(qtProps.qtVersion, "5.14") >= 0)
            prlFilePath = FileInfo.joinPaths(prlFilePath, "Resources");
    }
    var baseName = ProviderUtils.qtLibraryBaseName(modInfo, qtProps, debugBuild);
    if (!qtProps.mkspecName.startsWith("win") && !ProviderUtils.qtIsFramework(modInfo, qtProps))
        baseName = "lib" + baseName;
    prlFilePath = FileInfo.joinPaths(prlFilePath, baseName);
    var isNonStaticQt4OnWindows = qtProps.mkspecName.startsWith("win")
            && !modInfo.isStaticLibrary && qtProps.qtMajorVersion < 5;
    if (isNonStaticQt4OnWindows)
        prlFilePath = prlFilePath.slice(0, prlFilePath.length - 1); // The prl file base name does *not* contain the version number...
    // qt for android versions 6.0 and 6.1 don't have the architecture suffix in the prl file
    if (androidAbi.length > 0
            && modInfo.name !== "QtBootstrap"
            && (modInfo.name !== "QtQmlDevTools" || modInfo.name === "QtQmlDevTools"
                && Utilities.versionCompare(qtProps.qtVersion, "6.2") >= 0)
            && (Utilities.versionCompare(qtProps.qtVersion, "6.0") < 0
                || Utilities.versionCompare(qtProps.qtVersion, "6.2") >= 0)) {
        prlFilePath += "_";
        prlFilePath += androidAbi;
    }

    prlFilePath += ".prl";

    try {
        var prlFile = new TextFile(prlFilePath, TextFile.ReadOnly);
        while (!prlFile.atEof()) {
            var line = prlFile.readLine().trim();
            var equalsOffset = line.indexOf('=');
            if (equalsOffset === -1)
                continue;
            if (line.startsWith("QMAKE_PRL_TARGET")) {
                var isMingw = qtProps.mkspecName.startsWith("win")
                        && qtProps.mkspecName.contains("g++");
                var isQtVersionBefore56 = qtProps.qtMajorVersion < 5
                        || (qtProps.qtMajorVersion === 5 && qtProps.qtMinorVersion < 6);

                // QMAKE_PRL_TARGET has a "lib" prefix, except for mingw.
                // Of course, the exception has an exception too: For static libs, mingw *does*
                // have the "lib" prefix.
                var libFileName = "";
                if (isQtVersionBefore56 && qtProps.qtMajorVersion === 5 && isMingw
                        && !modInfo.isStaticLibrary) {
                    libFileName += "lib";
                }

                libFileName += line.slice(equalsOffset + 1).trim();
                if (isNonStaticQt4OnWindows)
                    libFileName += 4; // This is *not* part of QMAKE_PRL_TARGET...
                if (isQtVersionBefore56) {
                    if (qtProps.mkspecName.contains("msvc")) {
                        libFileName += ".lib";
                    } else if (isMingw) {
                        libFileName += ".a";
                        if (!File.exists(FileInfo.joinPaths(libDir, libFileName)))
                            libFileName = libFileName.slice(0, -2) + ".dll";
                    }
                }
                libFilePath = FileInfo.joinPaths(libDir, libFileName);
                continue;
            }
            if (line.startsWith("QMAKE_PRL_CONFIG")) {
                modInfo.config = splitNonEmpty(line.slice(equalsOffset + 1).trim(), ' ');
                continue;
            }
            if (!line.startsWith("QMAKE_PRL_LIBS ="))
                continue;

            var parts = extractPaths(line.slice(equalsOffset + 1).trim(), prlFilePath);
            for (i = 0; i < parts.length; ++i) {
                var part = parts[i];
                part = part.replace("$$[QT_INSTALL_LIBS]", qtProps.libraryPath);
                part = part.replace("$$[QT_INSTALL_PLUGINS]", qtProps.pluginPath);
                part = part.replace("$$[QT_INSTALL_PREFIX]", qtProps.installPrefixPath);
                if (part.startsWith("-l")) {
                    libs.push(part.slice(2));
                } else if (part.startsWith("-L")) {
                    modInfo.libraryPaths.push(part.slice(2));
                } else if (part.startsWith("-F")) {
                    frameworkPaths.push(part.slice(2));
                } else if (part === "-framework") {
                    if (++i < parts.length)
                        frameworks.push(parts[i]);
                } else if (part === "-pthread") {
                    // prl files for android have QMAKE_PRL_LIBS = -llog -pthread but the pthread
                    // functionality is included in libc.
                    if (androidAbi.length === 0)
                        libs.push("pthread");
                } else if (part.startsWith('-')) { // Some other option
                    console.debug("QMAKE_PRL_LIBS contains non-library option '" + part
                                  + "' in file '" + prlFilePath + "'");
                    flags.push(part);
                } else if (part.startsWith("/LIBPATH:")) {
                    libraryPaths.push(part.slice(9).replace(/\\/g, '/'));
                } else { // Assume it's a file path/name.
                    libs.push(part.replace(/\\/g, '/'));
                }
            }
        }
    } catch (e) {
        // qt_ext_lib_extX.pri (usually) don't have a corresponding prl file.
        // So the pri file variable QMAKE_LIBS_LIBX points to the library
        if (modInfo.isExternal) {
            libFilePath = debugBuild ? modInfo.staticLibrariesDebug[0] :
                                       modInfo.staticLibrariesRelease[0];
        }
        if (!libFilePath || !File.exists(libFilePath))
            libFilePath = guessLibraryFilePath(prlFilePath, libDir, qtProps);
        if (nonExistingPrlFiles.contains(prlFilePath))
            return;
        nonExistingPrlFiles.push(prlFilePath);
        if (modInfo.mustExist) {
            console.warn("Could not open prl file '"
                         + FileInfo.toNativeSeparators(prlFilePath) + "' for module '"
                         + modInfo.name
                         + "' (" + e + "), and failed to deduce the library file path. "
                         + " This module will likely not be usable by qbs.");
        }
    }
    finally {
        if (prlFile)
            prlFile.close();
    }

    if (debugBuild)
        modInfo.libFilePathDebug = libFilePath;
    else
        modInfo.libFilePathRelease = libFilePath;
}

function setupLibraries(qtModuleInfo, qtProps, nonExistingPrlFiles, androidAbi) {
    doSetupLibraries(qtModuleInfo, qtProps, true, nonExistingPrlFiles, androidAbi);
    doSetupLibraries(qtModuleInfo, qtProps, false, nonExistingPrlFiles, androidAbi);
}

function allQt4Modules(qtProps) {
    // as per http://doc.qt.io/qt-4.8/modules.html + private stuff.
    var modules = [];

    var core = makeQtModuleInfo("QtCore", "core");
    core.compilerDefines.push("QT_CORE_LIB");
    if (qtProps.qtNameSpace)
        core.compilerDefines.push("QT_NAMESPACE=" + qtProps.qtNameSpace);
    modules.push(core,
                 makeQtModuleInfo("QtCore", "core-private", ["core"]),
                 makeQtModuleInfo("QtGui", "gui"),
                 makeQtModuleInfo("QtGui", "gui-private", ["gui"]),
                 makeQtModuleInfo("QtMultimedia", "multimedia", ["gui", "network"]),
                 makeQtModuleInfo("QtMultimedia", "multimedia-private", ["multimedia"]),
                 makeQtModuleInfo("QtNetwork", "network"),
                 makeQtModuleInfo("QtNetwork", "network-private", ["network"]),
                 makeQtModuleInfo("QtOpenGL", "opengl", ["gui"]),
                 makeQtModuleInfo("QtOpenGL", "opengl-private", ["opengl"]),
                 makeQtModuleInfo("QtOpenVG", "openvg", ["gui"]),
                 makeQtModuleInfo("QtScript", "script"),
                 makeQtModuleInfo("QtScript", "script-private", ["script"]),
                 makeQtModuleInfo("QtScriptTools", "scripttools", ["script", "gui"]),
                 makeQtModuleInfo("QtScriptTools", "scripttools-private", ["scripttools"]),
                 makeQtModuleInfo("QtSql", "sql"),
                 makeQtModuleInfo("QtSql", "sql-private", ["sql"]),
                 makeQtModuleInfo("QtSvg", "svg", ["gui"]),
                 makeQtModuleInfo("QtSvg", "svg-private", ["svg"]),
                 makeQtModuleInfo("QtWebKit", "webkit", ["gui", "network"]),
                 makeQtModuleInfo("QtWebKit", "webkit-private", ["webkit"]),
                 makeQtModuleInfo("QtXml", "xml"),
                 makeQtModuleInfo("QtXml", "xml-private", ["xml"]),
                 makeQtModuleInfo("QtXmlPatterns", "xmlpatterns", ["network"]),
                 makeQtModuleInfo("QtXmlPatterns", "xmlpatterns-private", ["xmlpatterns"]),
                 makeQtModuleInfo("QtDeclarative", "declarative", ["gui", "script"]),
                 makeQtModuleInfo("QtDeclarative", "declarative-private", ["declarative"]),
                 makeQtModuleInfo("QtDesigner", "designer", ["gui", "xml"]),
                 makeQtModuleInfo("QtDesigner", "designer-private", ["designer"]),
                 makeQtModuleInfo("QtUiTools", "uitools"),
                 makeQtModuleInfo("QtUiTools", "uitools-private", ["uitools"]),
                 makeQtModuleInfo("QtHelp", "help", ["network", "sql"]),
                 makeQtModuleInfo("QtHelp", "help-private", ["help"]),
                 makeQtModuleInfo("QtTest", "testlib"),
                 makeQtModuleInfo("QtTest", "testlib-private", ["testlib"]));
    if (qtProps.mkspecName.startsWith("win")) {
        var axcontainer = makeQtModuleInfo("QAxContainer", "axcontainer");
        axcontainer.modulePrefix = "Q";
        axcontainer.isStaticLibrary = true;
        axcontainer.includePaths.push(FileInfo.joinPaths(qtProps.includePath, "ActiveQt"));
        modules.push(axcontainer);

        var axserver = makeQtModuleInfo("QAxServer", "axserver");
        axserver.modulePrefix = "Q";
        axserver.isStaticLibrary = true;
        axserver.compilerDefines.push("QAXSERVER");
        axserver.includePaths.push(FileInfo.joinPaths(qtProps.includePath, "ActiveQt"));
        modules.push(axserver);
    } else {
        modules.push(makeQtModuleInfo("QtDBus", "dbus"));
        modules.push(makeQtModuleInfo("QtDBus", "dbus-private", ["dbus"]));
    }

    var designerComponentsPrivate = makeQtModuleInfo(
                "QtDesignerComponents", "designercomponents-private",
                ["gui-private", "designer-private"]);
    designerComponentsPrivate.hasLibrary = true;
    modules.push(designerComponentsPrivate);

    var phonon = makeQtModuleInfo("Phonon", "phonon");
    phonon.includePaths = qt4ModuleIncludePaths(phonon, qtProps);
    modules.push(phonon);

    // Set up include paths that haven't been set up before this point.
    for (i = 0; i < modules.length; ++i) {
        var module = modules[i];
        if (module.includePaths.length > 0)
            continue;
        module.includePaths = qt4ModuleIncludePaths(module, qtProps);
    }

    // Set up compiler defines haven't been set up before this point.
    for (i = 0; i < modules.length; ++i) {
        module = modules[i];
        if (module.compilerDefines.length > 0)
            continue;
        module.compilerDefines.push("QT_" + module.qbsName.toUpperCase() + "_LIB");
    }

    // These are for the convenience of project file authors. It allows them
    // to add a dependency to e.g. "Qt.widgets" without a version check.
    var virtualModule = makeQtModuleInfo(undefined, "widgets", ["core", "gui"]);
    virtualModule.hasLibrary = false;
    modules.push(virtualModule);
    virtualModule = makeQtModuleInfo(undefined, "quick", ["declarative"]);
    virtualModule.hasLibrary = false;
    modules.push(virtualModule);
    virtualModule = makeQtModuleInfo(undefined, "concurrent");
    virtualModule.hasLibrary = false;
    modules.push(virtualModule);
    virtualModule = makeQtModuleInfo(undefined, "printsupport", ["core", "gui"]);
    virtualModule.hasLibrary = false;
    modules.push(virtualModule);

    addTestModule(modules);
    addDesignerComponentsModule(modules);

    var modulesThatCanBeDisabled = [
                "xmlpatterns", "multimedia", "phonon", "svg", "webkit", "script", "scripttools",
                "declarative", "gui", "dbus", "opengl", "openvg"];
    var nonExistingPrlFiles = [];
    for (i = 0; i < modules.length; ++i) {
        module = modules[i];
        var name = module.qbsName;
        var privateIndex = name.indexOf("-private");
        if (privateIndex !== -1)
            name = name.slice(0, privateIndex);
        if (modulesThatCanBeDisabled.contains(name))
            module.mustExist = false;
        if (qtProps.staticBuild)
            module.isStaticLibrary = true;
        setupLibraries(module, qtProps, nonExistingPrlFiles, "");
    }
    replaceQtLibNamesWithFilePath(modules, qtProps);

    return modules;
}

function getFileContentsRecursively(filePath) {
    var file = new TextFile(filePath, TextFile.ReadOnly);
    var lines = splitNonEmpty(file.readAll(), '\n');
    for (var i = 0; i < lines.length; ++i) {
        var includeString = "include(";
        var line = lines[i].trim();
        if (!line.startsWith(includeString))
            continue;
        var offset = includeString.length;
        var closingParenPos = line.indexOf(')', offset);
        if (closingParenPos === -1) {
            console.warn("Invalid include statement in '"
                    + FileInfo.toNativeSeparators(filePath) + "'");
            continue;
        }
        var includedFilePath = line.slice(offset, closingParenPos);
        if (!FileInfo.isAbsolutePath(includedFilePath))
            includedFilePath = FileInfo.joinPaths(FileInfo.path(filePath), includedFilePath);
        var includedContents = getFileContentsRecursively(includedFilePath);
        var j = i;
        for (var k = 0; k < includedContents.length; ++k)
            lines.splice(++j, 0, includedContents[k]);
        lines.splice(i--, 1);
    }
    file.close();
    return lines;
}

function extractPaths(rhs, filePath) {
    var paths = [];
    var startIndex = 0;
    for (;;) {
        while (startIndex < rhs.length && rhs.charAt(startIndex) === ' ')
            ++startIndex;
        if (startIndex >= rhs.length)
            break;
        var endIndex;
        if (rhs.charAt(startIndex) === '"') {
            ++startIndex;
            endIndex = rhs.indexOf('"', startIndex);
            if (endIndex === -1) {
                console.warn("Unmatched quote in file '"
                    + FileInfo.toNativeSeparators(filePath) + "'");
                break;
            }
        } else {
            endIndex = rhs.indexOf(' ', startIndex + 1);
            if (endIndex === -1)
                endIndex = rhs.length;
        }
        paths.push(FileInfo.cleanPath(rhs.slice(startIndex, endIndex)
                                      .replace("$$PWD", FileInfo.path(filePath))));
        startIndex = endIndex + 1;
    }
    return paths;
}

function removeDuplicatedDependencyLibs(modules) {
    var revDeps = {};
    var currentPath = [];
    var getLibraries;
    var getLibFilePath;

    function setupReverseDependencies(modules) {
        var moduleByName = {};
        for (var i = 0; i < modules.length; ++i)
            moduleByName[modules[i].qbsName] = modules[i];
        for (i = 0; i < modules.length; ++i) {
            var module = modules[i];
            for (var j = 0; j < module.dependencies.length; ++j) {
                var depmod = moduleByName[module.dependencies[j]];
                if (!depmod)
                    continue;
                if (!revDeps[depmod.qbsName])
                    revDeps[depmod.qbsName] = [];
                revDeps[depmod.qbsName].push(module);
            }
        }
    }

    function roots(modules) {
        var result = [];
        for (i = 0; i < modules.length; ++i) {
            var module = modules[i]
            if (module.dependencies.length === 0)
                result.push(module);
        }
        return result;
    }

    function traverse(module, libs) {
        if (currentPath.contains(module))
            return;
        currentPath.push(module);
        var moduleLibraryLists = getLibraries(module);
        for (var i = 0; i < moduleLibraryLists.length; ++i) {
            var modLibList = moduleLibraryLists[i];
            for (j = modLibList.length - 1; j >= 0; --j) {
                if (libs.contains(modLibList[j]))
                    modLibList.splice(j, 1);
            }
        }

        var libFilePath = getLibFilePath(module);
        if (libFilePath)
            libs.push(libFilePath);
        for (i = 0; i < moduleLibraryLists.length; ++i)
            libs = libs.concat(moduleLibraryLists[i]);
        libs.sort();

        var deps = revDeps[module.qbsName];
        for (i = 0; i < (deps || []).length; ++i)
            traverse(deps[i], libs);

        currentPath.pop();
    }

    setupReverseDependencies(modules);

    // Traverse the debug variants of modules.
    getLibraries = function(module) {
        return [module.dynamicLibrariesDebug, module.staticLibrariesDebug];
    };
    getLibFilePath = function(module) { return module.libFilePathDebug; };
    var rootModules = roots(modules);
    for (var i = 0; i < rootModules.length; ++i)
        traverse(rootModules[i], []);

    // Traverse the release variants of modules.
    getLibraries = function(module) {
        return [module.dynamicLibrariesRelease, module.staticLibrariesRelease];
    };
    getLibFilePath = function(module) { return module.libFilePathRelease; };
    for (i = 0; i < rootModules.length; ++i)
        traverse(rootModules[i], []);
}

function allQt5Modules(qtProps, androidAbi) {
    var nonExistingPrlFiles = [];
    var modules = [];
    var modulesDir = FileInfo.joinPaths(qtProps.mkspecBasePath, "modules");
    var modulePriFiles = File.directoryEntries(modulesDir, File.Files);
    for (var i = 0; i < modulePriFiles.length; ++i) {
        var priFileName = modulePriFiles[i];
        var priFilePath = FileInfo.joinPaths(modulesDir, priFileName);
        var externalFileNamePrefix = "qt_ext_";
        var moduleFileNamePrefix = "qt_lib_";
        var pluginFileNamePrefix = "qt_plugin_";
        var moduleFileNameSuffix = ".pri";
        var fileHasExternalPrefix = priFileName.startsWith(externalFileNamePrefix);
        var fileHasModulePrefix = priFileName.startsWith(moduleFileNamePrefix);
        var fileHasPluginPrefix = priFileName.startsWith(pluginFileNamePrefix);
        if (!fileHasPluginPrefix && !fileHasModulePrefix && !fileHasExternalPrefix
                || !priFileName.endsWith(moduleFileNameSuffix)) {
            continue;
        }
        var moduleInfo = makeQtModuleInfo();
        moduleInfo.isPlugin = fileHasPluginPrefix;
        moduleInfo.isExternal = !moduleInfo.isPlugin && !fileHasModulePrefix;
        var fileNamePrefix = moduleInfo.isPlugin ? pluginFileNamePrefix : moduleInfo.isExternal
                                                   ? externalFileNamePrefix : moduleFileNamePrefix;
        moduleInfo.qbsName = priFileName.slice(fileNamePrefix.length, -moduleFileNameSuffix.length);
        if (moduleInfo.isPlugin) {
            moduleInfo.name = moduleInfo.qbsName;
            moduleInfo.isStaticLibrary = true;
        }
        var moduleKeyPrefix = (moduleInfo.isPlugin ? "QT_PLUGIN" : "QT")
                + '.' + moduleInfo.qbsName + '.';
        moduleInfo.qbsName = moduleInfo.qbsName.replace("_private", "-private");
        var hasV2 = false;
        var hasModuleEntry = false;
        var lines = getFileContentsRecursively(priFilePath);
        if (moduleInfo.isExternal) {
            moduleInfo.name = "qt" + moduleInfo.qbsName;
            moduleInfo.isStaticLibrary = true;
            for (var k = 0; k < lines.length; ++k) {
                var extLine = lines[k].trim();
                var extFirstEqualsOffset = extLine.indexOf('=');
                if (extFirstEqualsOffset === -1)
                    continue;
                var extKey = extLine.slice(0, extFirstEqualsOffset).trim();
                var extValue = extLine.slice(extFirstEqualsOffset + 1).trim();
                if (!extKey.startsWith("QMAKE_") || !extValue)
                    continue;

                var elements = extKey.split('_');
                if (elements.length >= 3) {
                    if (elements[1] === "LIBS") {
                        extValue = extValue.replace("/home/qt/work/qt/qtbase/lib",
                                                    qtProps.libraryPath);
                        extValue = extValue.replace("$$[QT_INSTALL_LIBS]", qtProps.libraryPath);
                        extValue = extValue.replace("$$[QT_INSTALL_LIBS/get]", qtProps.libraryPath);
                        if (elements.length === 4 ) {
                            if (elements[3] === androidAbi) {
                                moduleInfo.staticLibrariesRelease.push(extValue);
                                moduleInfo.staticLibrariesDebug.push(extValue);
                            }
                        } else if (elements.length === 5 ) {
                            // That's for "x86_64"
                            var abi = elements[3] + '_' + elements[4];
                            if (abi === androidAbi) {
                                moduleInfo.staticLibrariesRelease.push(extValue);
                                moduleInfo.staticLibrariesDebug.push(extValue);
                            }
                        } else {
                            moduleInfo.staticLibrariesRelease.push(extValue);
                            moduleInfo.staticLibrariesDebug.push(extValue);
                        }
                    } else if (elements[1] === "INCDIR") {
                        moduleInfo.includePaths.push(extValue.replace("$$[QT_INSTALL_HEADERS]",
                                                                      qtProps.includePath));
                    }
                }
            }
            moduleInfo.compilerDefines.push("QT_" + moduleInfo.qbsName.toUpperCase() + "_LIB");
            moduleInfo.mustExist = false;
        } else {
            for (var j = 0; j < lines.length; ++j) {
                var line = lines[j].trim();
                var firstEqualsOffset = line.indexOf('=');
                if (firstEqualsOffset === -1)
                    continue;
                var key = line.slice(0, firstEqualsOffset).trim();
                var value = line.slice(firstEqualsOffset + 1).trim();
                if (!key.startsWith(moduleKeyPrefix) || !value)
                    continue;
                if (key.endsWith(".name")) {
                    moduleInfo.name = value;
                } else if (key.endsWith(".module")) {
                    hasModuleEntry = true;
                } else if (key.endsWith(".depends")) {
                    moduleInfo.dependencies = splitNonEmpty(value, ' ');
                    for (var k = 0; k < moduleInfo.dependencies.length; ++k) {
                        moduleInfo.dependencies[k]
                                = moduleInfo.dependencies[k].replace("_private", "-private");
                    }
                } else if (key.endsWith(".module_config")) {
                    var elems = splitNonEmpty(value, ' ');
                    for (k = 0; k < elems.length; ++k) {
                        var elem = elems[k];
                        if (elem === "no_link")
                            moduleInfo.hasLibrary = false;
                        else if (elem === "staticlib")
                            moduleInfo.isStaticLibrary = true;
                        else if (elem === "internal_module")
                            moduleInfo.isPrivate = true;
                        else if (elem === "v2")
                            hasV2 = true;
                    }
                } else if (key.endsWith(".includes")) {
                    moduleInfo.includePaths = extractPaths(value, priFilePath);
                    for (k = 0; k < moduleInfo.includePaths.length; ++k) {
                        moduleInfo.includePaths[k] = moduleInfo.includePaths[k]
                        .replace("$$QT_MODULE_INCLUDE_BASE", qtProps.includePath)
                        .replace("$$QT_MODULE_HOST_LIB_BASE", qtProps.hostLibraryPath)
                        .replace("$$QT_MODULE_LIB_BASE", qtProps.libraryPath);
                    }
                } else if (key.endsWith(".libs")) {
                    var libDirs = extractPaths(value, priFilePath);
                    if (libDirs.length === 1) {
                        moduleInfo.libDir = libDirs[0]
                        .replace("$$QT_MODULE_HOST_LIB_BASE", qtProps.hostLibraryPath)
                        .replace("$$QT_MODULE_LIB_BASE", qtProps.libraryPath);
                    } else {
                        moduleInfo.libDir = qtProps.libraryPath;
                    }
                } else if (key.endsWith(".DEFINES")) {
                    moduleInfo.compilerDefines = splitNonEmpty(value, ' ');
                } else if (key.endsWith(".VERSION")) {
                    moduleInfo.version = value;
                } else if (key.endsWith(".plugin_types")) {
                    moduleInfo.supportedPluginTypes = splitNonEmpty(value, ' ');
                } else if (key.endsWith(".TYPE")) {
                    moduleInfo.pluginData.type = value;
                } else if (key.endsWith(".EXTENDS")) {
                    moduleInfo.pluginData["extends"] = splitNonEmpty(value, ' ');
                    for (k = 0; k < moduleInfo.pluginData["extends"].length; ++k) {
                        if (moduleInfo.pluginData["extends"][k] === "-") {
                            moduleInfo.pluginData["extends"].splice(k, 1);
                            moduleInfo.pluginData.autoLoad = false;
                            break;
                        }
                    }
                } else if (key.endsWith(".CLASS_NAME")) {
                    moduleInfo.pluginData.className = value;
                }
            }
        }
        if (hasV2 && !hasModuleEntry && !moduleInfo.isStaticLibrary)
            moduleInfo.hasLibrary = false;

        // Fix include paths for Apple frameworks.
        // The qt_lib_XXX.pri files contain wrong values for versions < 5.6.
        if (!hasV2 && ProviderUtils.qtIsFramework(moduleInfo, qtProps)) {
            moduleInfo.includePaths = [];
            var baseIncDir = frameworkHeadersPath(moduleInfo, qtProps);
            if (moduleInfo.isPrivate) {
                baseIncDir = FileInfo.joinPaths(baseIncDir, moduleInfo.version);
                moduleInfo.includePaths.push(baseIncDir,
                                             FileInfo.joinPaths(baseIncDir, moduleInfo.name));
            } else {
                moduleInfo.includePaths.push(baseIncDir);
            }
        }

        setupLibraries(moduleInfo, qtProps, nonExistingPrlFiles, androidAbi);

        modules.push(moduleInfo);
        if (moduleInfo.qbsName === "testlib")
            addTestModule(modules);
        if (moduleInfo.qbsName === "designercomponents-private")
            addDesignerComponentsModule(modules);
    }

    replaceQtLibNamesWithFilePath(modules, qtProps);
    removeDuplicatedDependencyLibs(modules);

    return modules;
}

function getQtInfo(qmakeFilePath) {
    if (!File.exists(qmakeFilePath)) {
        throw "The specified qmake file path '"
            + FileInfo.toNativeSeparators(qmakeFilePath) + "' does not exist.";
    }
    var qtProps = getQtProperties(qmakeFilePath);
    var androidAbis = [];
    if (qtProps.androidAbis !== undefined) {
        // Multiple androidAbis detected: Qt >= 5.14
        androidAbis = qtProps.androidAbis;
    } else {
        // Single abi detected: Qt < 5.14
        androidAbis.push('');
    }
    if (androidAbis.length > 1)
        console.info("Qt with multiple abi detected: '" + androidAbis + "'");

    var result = {};
    result.qtProps = qtProps;
    result.abiInfos = [];
    result.qmakeFilePath = qmakeFilePath;
    for (a = 0; a < androidAbis.length; ++a) {
        var abiInfo = {};
        if (androidAbis.length > 1)
            console.info("Found abi '" + androidAbis[a] + "'...");
        abiInfo.androidAbi = androidAbis[a];
        var allModules = qtProps.qtMajorVersion < 5
            ? allQt4Modules(qtProps) : allQt5Modules(qtProps, androidAbis[a]);;
        abiInfo.modules = {};
        for (var i = 0; i < allModules.length; ++i) {
            var module = allModules[i];
            abiInfo.modules[module.qbsName] = module;
        }
        abiInfo.pluginsByType = {};
        abiInfo.nonEssentialPlugins = [];
        for (var moduleName in abiInfo.modules) {
            var m = abiInfo.modules[moduleName];
            if (m.isPlugin) {
                if (!abiInfo.pluginsByType[m.pluginData.type])
                    abiInfo.pluginsByType[m.pluginData.type] = [];
                abiInfo.pluginsByType[m.pluginData.type].push(m.name);
                if (!m.pluginData.autoLoad)
                    abiInfo.nonEssentialPlugins.push(m.name);
            }
        }

        result.abiInfos.push(abiInfo);
    }
    return result;
}

function configure(qmakeFilePaths) {
    var result = [];
    qmakeFilePaths = getQmakeFilePaths(qmakeFilePaths);
    if (!qmakeFilePaths || qmakeFilePaths.length === 0)
        return result;
    for (var i = 0; i < qmakeFilePaths.length; ++i) {
        var qtInfo = {};
        try {
            console.info("Getting info about Qt at '"
                    + FileInfo.toNativeSeparators(qmakeFilePaths[i]) + "'...");
            qtInfo = getQtInfo(qmakeFilePaths[i]);
            result.push(qtInfo);
        } catch (e) {
            console.warn("Error getting info about Qt for '"
                + FileInfo.toNativeSeparators(qmakeFilePaths[i]) + "': " + e);
            throw e;
        }
    }
    return result;
}
