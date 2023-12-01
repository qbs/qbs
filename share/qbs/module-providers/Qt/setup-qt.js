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

var File = require("qbs.File");
var FileInfo = require("qbs.FileInfo");
var ModUtils = require("qbs.ModUtils");
var ProviderUtils = require("qbs.ProviderUtils");
var TextFile = require("qbs.TextFile");
var Utilities = require("qbs.Utilities");

function extractQbsArchs(module, qtProps) {
    if (qtProps.mkspecName.startsWith("macx-")) {
        var archs = [];
        if (module.libFilePathRelease)
            archs = Utilities.getArchitecturesFromBinary(module.libFilePathRelease);
        return archs;
    }
    var qbsArch = Utilities.canonicalArchitecture(qtProps.architecture);
    if (qbsArch === "arm" && qtProps.mkspecPath.contains("android"))
        qbsArch = "armv7a";
    // Qt4 has "QT_ARCH = windows" in qconfig.pri for both MSVC and mingw.
    if (qbsArch === "windows")
        return []

    return [qbsArch];
}

function qbsTargetPlatformFromQtMkspec(qtProps) {
    var mkspec = qtProps.mkspecName;
    var idx = mkspec.lastIndexOf('/');
    if (idx !== -1)
        mkspec = mkspec.slice(idx + 1);
    if (mkspec.startsWith("aix-"))
        return "aix";
    if (mkspec.startsWith("android-"))
        return "android";
    if (mkspec.startsWith("cygwin-"))
        return "windows";
    if (mkspec.startsWith("darwin-"))
        return "macos";
    if (mkspec.startsWith("freebsd-"))
        return "freebsd";
    if (mkspec.startsWith("haiku-"))
        return "haiku";
    if (mkspec.startsWith(("hpux-")) || mkspec.startsWith(("hpuxi-")))
        return "hpux";
    if (mkspec.startsWith("hurd-"))
        return "hurd";
    if (mkspec.startsWith("integrity-"))
        return "integrity";
    if (mkspec.startsWith("linux-"))
        return "linux";
    if (mkspec.startsWith("macx-")) {
        if (mkspec.startsWith("macx-ios-"))
            return "ios";
        if (mkspec.startsWith("macx-tvos-"))
            return "tvos";
        if (mkspec.startsWith("macx-watchos-"))
            return "watchos";
        return "macos";
    }
    if (mkspec.startsWith("netbsd-"))
        return "netbsd";
    if (mkspec.startsWith("openbsd-"))
        return "openbsd";
    if (mkspec.startsWith("qnx-"))
        return "qnx";
    if (mkspec.startsWith("solaris-"))
        return "solaris";
    if (mkspec.startsWith("vxworks-"))
        return "vxworks";
    if (ProviderUtils.isDesktopWindowsQt(qtProps) || mkspec.startsWith("winrt-"))
        return "windows";
}

function pathToJSLiteral(path) { return JSON.stringify(FileInfo.fromNativeSeparators(path)); }

function defaultQpaPlugin(module, qtProps) {
    if (qtProps.qtMajorVersion < 5)
        return undefined;
    if (qtProps.qtMajorVersion === 5 && qtProps.qtMinorVersion < 8) {
        var qConfigPri = new TextFile(FileInfo.joinPaths(qtProps.mkspecBasePath, "qconfig.pri"));
        var magicString = "QT_DEFAULT_QPA_PLUGIN =";
        while (!qConfigPri.atEof()) {
            var line = qConfigPri.readLine().trim();
            if (line.startsWith(magicString))
                return line.slice(magicString.length).trim();
        }
        qConfigPri.close();
    } else {
        var gtGuiHeadersPath = qtProps.frameworkBuild
                ? FileInfo.joinPaths(qtProps.libraryPath, "QtGui.framework", "Headers")
                : FileInfo.joinPaths(qtProps.includePath, "QtGui");
        var qtGuiConfigHeader = FileInfo.joinPaths(gtGuiHeadersPath, "qtgui-config.h");
        var headerFiles = [];
        headerFiles.push(qtGuiConfigHeader);
        while (headerFiles.length > 0) {
            var filePath = headerFiles.shift();
            var headerFile = new TextFile(filePath, TextFile.ReadOnly);
            var regexp = /^#define QT_QPA_DEFAULT_PLATFORM_NAME "(.+)".*$/;
            var includeRegexp = /^#include "(.+)".*$/;
            while (!headerFile.atEof()) {
                line = headerFile.readLine().trim();
                var match = line.match(regexp);
                if (match)
                    return 'q' + match[1];
                match = line.match(includeRegexp);
                if (match) {
                    var includedFile = match[1];
                    if (!FileInfo.isAbsolutePath(includedFile)) {
                        includedFile = FileInfo.cleanPath(
                                    FileInfo.joinPaths(FileInfo.path(filePath), includedFile));
                    }
                    headerFiles.push(includedFile);
                }
            }
            headerFile.close();
        }
    }

    if (module.isStaticLibrary)
        console.warn("Could not determine default QPA plugin for static Qt.");
}

function libraryFileTag(module, qtProps) {
    if (module.isStaticLibrary)
        return "staticlibrary";
    return ProviderUtils.isMsvcQt(qtProps) || qtProps.mkspecName.startsWith("win32-g++")
            ? "dynamiclibrary_import" : "dynamiclibrary";
}

function findVariable(content, start) {
    var result = [-1, -1];
    result[0] = content.indexOf('@', start);
    if (result[0] === -1)
        return result;
    result[1] = content.indexOf('@', result[0] + 1);
    if (result[1] === -1) {
        result[0] = -1;
        return result;
    }
    var forbiddenChars = [' ', '\n'];
    for (var i = 0; i < forbiddenChars.length; ++i) {
        var forbidden = forbiddenChars[i];
        var k = content.indexOf(forbidden, result[0] + 1);
        if (k !== -1 && k < result[1])
            return findVariable(content, result[0] + 1);
    }
    return result;
}

function minVersionJsString(minVersion) {
    return !minVersion ? "" : ModUtils.toJSLiteral(minVersion);
}

function abiToArchitecture(abi) {
    switch (abi) {
    case "armeabi-v7a":
        return "armv7a";
    case "arm64-v8a":
        return "arm64";
    case "x86":
    case "x86_64":
    default:
        return abi;
    }
}

function replaceSpecialValues(content, module, qtProps, abi) {
    var architectures = [];
    if (abi.length > 0)
        architectures.push(abiToArchitecture(abi));
    else
        architectures = extractQbsArchs(module, qtProps);
    var dict = {
        archs: ModUtils.toJSLiteral(architectures),
        targetPlatform: ModUtils.toJSLiteral(qbsTargetPlatformFromQtMkspec(qtProps)),
        config: ModUtils.toJSLiteral(qtProps.configItems),
        qtConfig: ModUtils.toJSLiteral(qtProps.qtConfigItems),
        binPath: ModUtils.toJSLiteral(qtProps.binaryPath),
        installPath: ModUtils.toJSLiteral(qtProps.installPath),
        installPrefixPath: ModUtils.toJSLiteral(qtProps.installPrefixPath),
        libPath: ModUtils.toJSLiteral(qtProps.libraryPath),
        libExecPath: ModUtils.toJSLiteral(qtProps.libExecPath),
        qmlLibExecPath: ModUtils.toJSLiteral(qtProps.qmlLibExecPath),
        pluginPath: ModUtils.toJSLiteral(qtProps.pluginPath),
        incPath: ModUtils.toJSLiteral(qtProps.includePath),
        docPath: ModUtils.toJSLiteral(qtProps.documentationPath),
        helpGeneratorLibExecPath: ModUtils.toJSLiteral(qtProps.helpGeneratorLibExecPath),
        mkspecName: ModUtils.toJSLiteral(qtProps.mkspecName),
        mkspecPath: ModUtils.toJSLiteral(qtProps.mkspecPath),
        version: ModUtils.toJSLiteral(qtProps.qtVersion),
        libInfix: ModUtils.toJSLiteral(qtProps.qtLibInfix),
        availableBuildVariants: ModUtils.toJSLiteral(qtProps.buildVariant),
        staticBuild: ModUtils.toJSLiteral(qtProps.staticBuild),
        frameworkBuild: ModUtils.toJSLiteral(qtProps.frameworkBuild),
        name: ModUtils.toJSLiteral(ProviderUtils.qtModuleNameWithoutPrefix(module)),
        has_library: ModUtils.toJSLiteral(module.hasLibrary),
        dependencies: ModUtils.toJSLiteral(module.dependencies),
        includes: ModUtils.toJSLiteral(module.includePaths),
        staticLibsDebug: ModUtils.toJSLiteral(module.staticLibrariesDebug),
        staticLibsRelease: ModUtils.toJSLiteral(module.staticLibrariesRelease),
        dynamicLibsDebug: ModUtils.toJSLiteral(module.dynamicLibrariesDebug),
        dynamicLibsRelease: ModUtils.toJSLiteral(module.dynamicLibrariesRelease),
        linkerFlagsDebug: ModUtils.toJSLiteral(module.linkerFlagsDebug),
        linkerFlagsRelease: ModUtils.toJSLiteral(module.linkerFlagsRelease),
        libraryPaths: ModUtils.toJSLiteral(module.libraryPaths),
        frameworkPathsDebug: ModUtils.toJSLiteral(module.frameworkPathsDebug),
        frameworkPathsRelease: ModUtils.toJSLiteral(module.frameworkPathsRelease),
        frameworksDebug: ModUtils.toJSLiteral(module.frameworksDebug),
        frameworksRelease: ModUtils.toJSLiteral(module.frameworksRelease),
        libFilePathDebug: ModUtils.toJSLiteral(module.libFilePathDebug),
        libFilePathRelease: ModUtils.toJSLiteral(module.libFilePathRelease),
        libNameForLinkerDebug:
            ModUtils.toJSLiteral(ProviderUtils.qtLibNameForLinker(module, qtProps, true)),
        pluginTypes: ModUtils.toJSLiteral(module.supportedPluginTypes),
        moduleConfig: ModUtils.toJSLiteral(module.config),
        libNameForLinkerRelease:
            ModUtils.toJSLiteral(ProviderUtils.qtLibNameForLinker(module, qtProps, false)),
        entryPointLibsDebug: ModUtils.toJSLiteral(qtProps.entryPointLibsDebug),
        entryPointLibsRelease: ModUtils.toJSLiteral(qtProps.entryPointLibsRelease),
        minWinVersion_optional: minVersionJsString(qtProps.windowsVersion),
        minMacVersion_optional: minVersionJsString(qtProps.macosVersion),
        minIosVersion_optional: minVersionJsString(qtProps.iosVersion),
        minTvosVersion_optional: minVersionJsString(qtProps.tvosVersion),
        minWatchosVersion_optional: minVersionJsString(qtProps.watchosVersion),
        minAndroidVersion_optional: minVersionJsString(qtProps.androidVersion),
    };

    var additionalContent = "";
    var compilerDefines = ModUtils.toJSLiteral(module.compilerDefines);
    if (module.qbsName === "declarative" || module.qbsName === "quick") {
        var debugMacro = module.qbsName === "declarative" || qtProps.qtMajorVersion < 5
                ? "QT_DECLARATIVE_DEBUG" : "QT_QML_DEBUG";
        var indent = "    ";
        additionalContent = "property bool qmlDebugging: false\n"
                + indent + "property string qmlPath";
        if (qtProps.qmlPath)
            additionalContent += ": " + pathToJSLiteral(qtProps.qmlPath) + '\n';
        else
            additionalContent += '\n';

        additionalContent += indent + "property string qmlImportsPath: "
                + pathToJSLiteral(qtProps.qmlImportPath);

        compilerDefines = "{\n"
                + indent + indent + "var result = " + compilerDefines + ";\n"
                + indent + indent + "if (qmlDebugging)\n"
                + indent + indent + indent + "result.push(\"" + debugMacro + "\");\n"
                + indent + indent + "return result;\n"
                + indent + "}";
    }
    dict.defines = compilerDefines;
    if (module.qbsName === "gui")
        dict.defaultQpaPlugin = ModUtils.toJSLiteral(defaultQpaPlugin(module, qtProps));
    if (module.qbsName === "qml")
        dict.qmlPath = pathToJSLiteral(qtProps.qmlPath);
    if (module.isStaticLibrary && module.qbsName !== "core") {
        if (additionalContent)
            additionalContent += "\n    ";
        additionalContent += "isStaticLibrary: true";
    }
    if (module.isPlugin) {
        dict.className = ModUtils.toJSLiteral(module.pluginData.className);
        dict["extends"] = ModUtils.toJSLiteral(module.pluginData["extends"]);
    }
    indent = "    ";
    var metaTypesFile = qtProps.libraryPath + "/metatypes/qt"
            + qtProps.qtMajorVersion + module.qbsName + "_metatypes.json";
    if (File.exists(metaTypesFile)) {
        if (additionalContent)
            additionalContent += "\n" + indent;
        additionalContent += "Group {\n";
        additionalContent += indent + indent + "files: " + JSON.stringify(metaTypesFile) + "\n"
            + indent + indent + "filesAreTargets: true\n"
            + indent + indent + "fileTags: [\"qt.core.metatypes\"]\n"
            + indent + "}";
    }
    if (module.hasLibrary && !ProviderUtils.qtIsFramework(module, qtProps)) {
        if (additionalContent)
            additionalContent += "\n" + indent;
        additionalContent += "Group {\n";
        if (module.isPlugin) {
            additionalContent += indent + indent
                    + "condition: enableLinking\n";
        }
        additionalContent += indent + indent + "files: libFilePath\n"
                + indent + indent + "filesAreTargets: true\n"
                + indent + indent + "fileTags: [\"" + libraryFileTag(module, qtProps)
                                  + "\"]\n"
                + indent + "}";
    }
    dict.additionalContent = additionalContent;

    for (var pos = findVariable(content, 0); pos[0] !== -1;
         pos = findVariable(content, pos[0])) {
        var varName = content.slice(pos[0] + 1, pos[1]);
        var replacement = dict[varName] || "";
        if (!replacement && varName.endsWith("_optional")) {
            var prevNewline = content.lastIndexOf('\n', pos[0]);
            if (prevNewline === -1)
                prevNewline = 0;
            var nextNewline = content.indexOf('\n', pos[0]);
            if (nextNewline === -1)
                prevNewline = content.length;
            content = content.slice(0, prevNewline) + content.slice(nextNewline);
            pos[0] = prevNewline;
        } else {
            content = content.slice(0, pos[0]) + replacement + content.slice(pos[1] + 1);
            pos[0] += replacement.length;
        }
    }
    return content;
}

function copyTemplateFile(fileName, targetDirectory, qtProps, abi, location, allFiles, module,
                          pluginMap, nonEssentialPlugins)
{
    if (!File.makePath(targetDirectory)) {
        throw "Cannot create directory '" + FileInfo.toNativeSeparators(targetDirectory) + "'.";
    }
    var sourceFile = new TextFile(FileInfo.joinPaths(location, "templates", fileName),
                                  TextFile.ReadOnly);
    var newContent = sourceFile.readAll();
    if (module) {
        newContent = replaceSpecialValues(newContent, module, qtProps, abi);
    } else {
        newContent = newContent.replace("@allPluginsByType@",
                                        '(' + ModUtils.toJSLiteral(pluginMap) + ')');
        newContent = newContent.replace("@nonEssentialPlugins@",
                                        ModUtils.toJSLiteral(nonEssentialPlugins));
        newContent = newContent.replace("@version@", ModUtils.toJSLiteral(qtProps.qtVersion));
    }
    sourceFile.close();
    var targetPath = FileInfo.joinPaths(targetDirectory, fileName);
    allFiles.push(targetPath);
    var targetFile = new TextFile(targetPath, TextFile.WriteOnly);
    targetFile.write(newContent);
    targetFile.close();
}

function setupOneQt(moduleName, qtInfo, outputBaseDir, uniquify, location) {
    var qtProps = qtInfo.qtProps;

    var relativeSearchPaths = [];
    for (a = 0; a < qtInfo.abiInfos.length; ++a) {
        var abiInfo = qtInfo.abiInfos[a];
        var androidAbi = abiInfo.androidAbi;
        if (qtInfo.abiInfos.length > 1)
            console.info("Configuring abi '" + androidAbi + "'...");

        var relativeSearchPath = uniquify ? Utilities.getHash(qtInfo.qmakeFilePath) : "";
        relativeSearchPath = FileInfo.joinPaths(relativeSearchPath, androidAbi);
        var qbsQtModuleBaseDir = FileInfo.joinPaths(outputBaseDir, relativeSearchPath,
                                                    "modules", "Qt");
        // TODO:
        // if (File.exists(qbsQtModuleBaseDir))
            // File.remove(qbsQtModuleBaseDir);

        var allFiles = [];
        if (moduleName === "plugin_support") {
            copyTemplateFile("plugin_support.qbs",
                            FileInfo.joinPaths(qbsQtModuleBaseDir, "plugin_support"), qtProps,
                            androidAbi, location, allFiles, undefined, abiInfo.pluginsByType,
                            abiInfo.nonEssentialPlugins);
            relativeSearchPaths.push(relativeSearchPath);
            return relativeSearchPaths;
        } else if (moduleName === "android_support") {
            // Note that it's not strictly necessary to copy this one, as it has no variable content.
            // But we do it anyway for consistency.
            copyTemplateFile("android_support.qbs",
                            FileInfo.joinPaths(qbsQtModuleBaseDir, "android_support"),
                            qtProps, androidAbi, location, allFiles);
            relativeSearchPaths.push(relativeSearchPath);
            return relativeSearchPaths;
        } else if (moduleName === "qmlcache") {
            var qmlcacheStr = "qmlcache";
            if (File.exists(FileInfo.joinPaths(qtProps.qmlLibExecPath,
                                               "qmlcachegen" + FileInfo.executableSuffix()))) {
                copyTemplateFile(qmlcacheStr + ".qbs",
                                 FileInfo.joinPaths(qbsQtModuleBaseDir, qmlcacheStr), qtProps,
                                 androidAbi, location, allFiles);
            }
            relativeSearchPaths.push(relativeSearchPath);
            return relativeSearchPaths;
        }

        if (abiInfo.modules[moduleName] !== undefined) {
            var module = abiInfo.modules[moduleName];
            var qbsQtModuleDir = FileInfo.joinPaths(qbsQtModuleBaseDir, module.qbsName);
            var moduleTemplateFileName;

            if (module.qbsName === "core") {
                moduleTemplateFileName = "core.qbs";
                copyTemplateFile("moc.js", qbsQtModuleDir, qtProps, androidAbi, location,
                                 allFiles);
                copyTemplateFile("qdoc.js", qbsQtModuleDir, qtProps, androidAbi, location,
                                 allFiles);
                copyTemplateFile("rcc.js", qbsQtModuleDir, qtProps, androidAbi, location,
                                 allFiles);
            } else if (module.qbsName === "gui") {
                moduleTemplateFileName = "gui.qbs";
            } else if (module.qbsName === "scxml") {
                moduleTemplateFileName = "scxml.qbs";
            } else if (module.qbsName === "dbus") {
                moduleTemplateFileName = "dbus.qbs";
                copyTemplateFile("dbus.js", qbsQtModuleDir, qtProps, androidAbi, location,
                                 allFiles);
            } else if (module.qbsName === "qml") {
                moduleTemplateFileName = "qml.qbs";
                copyTemplateFile("qml.js", qbsQtModuleDir, qtProps, androidAbi, location,
                                 allFiles);
            } else if (module.qbsName === "quick") {
                moduleTemplateFileName = "quick.qbs";
                copyTemplateFile("quick.js", qbsQtModuleDir, qtProps, androidAbi, location,
                                 allFiles);
                copyTemplateFile("rcc.js", qbsQtModuleDir, qtProps, androidAbi, location,
                                 allFiles);
            } else if (module.isPlugin) {
                moduleTemplateFileName = "plugin.qbs";
            } else {
                moduleTemplateFileName = "module.qbs";
            }
            copyTemplateFile(moduleTemplateFileName, qbsQtModuleDir, qtProps, androidAbi,
                             location, allFiles, module);
            relativeSearchPaths.push(relativeSearchPath);
        }
    }
    return relativeSearchPaths;
}

function doSetup(moduleName, qtInfos, outputBaseDir, location) {
    if (!qtInfos || qtInfos.length === 0)
        return [];
    var uniquifySearchPath = qtInfos.length > 1;
    var allSearchPaths = [];
    moduleName = moduleName.substring(3);
    for (var i = 0; i < qtInfos.length; ++i) {
        try {
            console.info("Setting up Qt module '" + moduleName + "' for Qt located at '"
                    + FileInfo.toNativeSeparators(qtInfos[i].qmakeFilePath) + "'.");
            var searchPaths = setupOneQt(moduleName, qtInfos[i], outputBaseDir, uniquifySearchPath,
                                         location);
            if (searchPaths.length > 0) {
                for (var j = 0; j < searchPaths.length; ++j )
                    allSearchPaths.push(searchPaths[j]);
            }
        } catch (e) {
            console.warn("Error setting up Qt module '" + moduleName + "' for '"
                    + FileInfo.toNativeSeparators(qtInfos[i].qmakeFilePath) + "': " + e);
            throw e;
        }
    }
    return allSearchPaths;
}
