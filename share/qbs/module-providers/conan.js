/****************************************************************************
**
** Copyright (C) 2024 Kai Dohmen
** Copyright (C) 2024 Ivan Komissarov (abbapoh@gmail.com).
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
var PathProbe = require("../imports/qbs/Probes/path-probe.js")
var PathTools = require("qbs.PathTools");
var TextFile = require("qbs.TextFile");
var Utilities = require("qbs.Utilities");

const architectureMap = {
    'x86': 'x86',
    'x86_64': 'x86_64',
    'ppc32be': 'ppc',
    'ppc32': 'ppc',
    'ppc64le': 'ppc64',
    'ppc64': 'ppc64',
    'armv4': 'arm',
    'armv4i': 'arm',
    'armv5el': 'arm',
    'armv5hf': 'arm',
    'armv6': 'arm',
    'armv7': 'arm',
    'armv7hf': 'arm',
    'armv7s': 'arm',
    'armv7k': 'arm',
    'armv8': 'arm64',
    'armv8_32': 'arm64',
    'armv8.3': 'arm64',
    'sparc': 'sparc',
    'sparcv9': 'sparc64',
    'mips': 'mips',
    'mips64': 'mips64',
    'avr': 'avr',
    's390': 's390x',
    's390x': 's390x',
    'sh4le': 'sh'
}

const platformMap = {
    'Windows': 'windows',
    'WindowsStore': 'windows',
    'WindowsCE': 'windows',
    'Linux': 'linux',
    'Macos': 'macos',
    'Android': 'android',
    'iOS': 'ios',
    'watchOS': 'watchos',
    'tvOS': 'tvos',
    'FreeBSD': 'freebsd',
    'SunOS': 'solaris',
    'AIX': 'aix',
    'Emscripten': undefined,
    'Arduino': 'none',
    'Neutrino': 'qnx',
    'VxWorks': 'vxworks',
}

function findLibs(libnames, libdirs, libtypes, targetOS, forImport) {
    var result = [];
    if (libnames === null || libdirs === null)
        return result;
    libnames.forEach(function(libraryName) {
        const suffixes = PathTools.librarySuffixes(targetOS, libtypes, forImport);
        const lib = PathProbe.configure(
            undefined, // selectors
            [libraryName],
            suffixes,
            PathTools.libraryNameFilter(),
            undefined, // candidateFilter
            libdirs, // searchPaths
            undefined, // pathSuffixes
            [], // platformSearchPaths
            undefined, // environmentPaths
            undefined // platformEnvironmentPaths
        );
        if (lib.found)
            result.push(lib.files[0].filePath);
    });
    return result;
}

function getLibraryTypes(options) {
    if (options !== undefined && options !== null) {
        if (options.shared === undefined)
            return ["shared", "static"];
        else if (options.shared === "True")
            return ["shared"];
        else if (options.shared === "False")
            return ["static"];
    }
    return ["shared", "static"];
}

function configure(installDirectory, moduleName, outputBaseDir, jsonProbe) {
    const moduleMapping = {"protobuflib": "protobuf"}
    const realModuleName = moduleMapping[moduleName] || moduleName;

    const moduleFilePath =
        FileInfo.joinPaths(installDirectory, "conan-qbs-deps", realModuleName + ".json")
    if (!File.exists(moduleFilePath))
        return [];

    var reverseMapping = {}
    for (var key in moduleMapping)
        reverseMapping[moduleMapping[key]] = key

    console.info("Setting up Conan module '" + moduleName + "'");

    var moduleFile = new TextFile(moduleFilePath, TextFile.ReadOnly);
    const moduleInfo = JSON.parse(moduleFile.readAll());

    const outputDir = FileInfo.joinPaths(outputBaseDir, "modules", moduleName.replace(".", "/"));
    File.makePath(outputDir);
    const outputFilePath = FileInfo.joinPaths(outputDir, "module.qbs");

    const cppInfo = moduleInfo.cpp_info;

    var moduleContent = "";

    // function write(data) { moduleContent = moduleContent + data }
    function writeLine(data) { moduleContent = moduleContent + data + "\n"; }

    function writeProperty(propertyName, propertyValue) {
        if (propertyValue === undefined || propertyValue === null)
            propertyValue = [];
        writeLine("    readonly property stringList " + propertyName
            + ": " + ModUtils.toJSLiteral(propertyValue));
    }
    function writeCppProperty(propertyName, propertyValue) {
        writeProperty(propertyName, propertyValue);
        // skip empty props for simplicity of the module file
        if (propertyValue !== undefined && propertyValue != null && propertyValue.length !== 0) {
            writeLine("    cpp." + propertyName + ": " + propertyName);
        }
    }

    writeLine("Module {");

    writeLine("    version: " + ModUtils.toJSLiteral(moduleInfo.version));

    const architecture = architectureMap[moduleInfo.settings.arch];
    const platform = platformMap[moduleInfo.settings.os];
    const targetOS = Utilities.canonicalPlatform(platform);

    writeLine("    readonly property string architecture: " + ModUtils.toJSLiteral(architecture));
    if (platform !== undefined)
        writeLine("    readonly property string platform: " + ModUtils.toJSLiteral(platform));
    writeLine("    condition: true");
    if (architecture !== undefined) {
        writeLine("    && (!qbs.architecture || qbs.architecture === architecture)");
    }
    if (platform !== undefined) {
        if (["ios", "tvos", "watchos"].includes(platform)) {
            writeLine("        && (qbs.targetPlatform === platform || qbs.targetPlatform === platform + \"-simulator\")");
        } else {
            writeLine("    && qbs.targetPlatform === platform");
        }
    }

    writeLine("    Depends { name: 'cpp' }");

    moduleInfo.dependencies.forEach(function(dep) {
        const realDepName = reverseMapping[dep.name] || dep.name;
        writeLine("    Depends { name: " + ModUtils.toJSLiteral(realDepName)
            + "; version: " + ModUtils.toJSLiteral(dep.version) + "}");
    });

    const buildBindirs = FileInfo.fromNativeSeparators(moduleInfo.build_bindirs);
    writeLine("    readonly property stringList hostBinDirs: (" + ModUtils.toJSLiteral(buildBindirs) + ")");
    const targetBindirs = FileInfo.fromNativeSeparators(cppInfo.bindirs);
    writeLine("    readonly property stringList binDirs: (" + ModUtils.toJSLiteral(targetBindirs) + ")");

    // TODO: there's a weird issue with system include dirs with xcode-less clang - incorrect include order?
    writeCppProperty("includePaths", FileInfo.fromNativeSeparators(cppInfo.includedirs));
    writeCppProperty("frameworkPaths", FileInfo.fromNativeSeparators(cppInfo.frameworkdirs));
    writeCppProperty("frameworks", cppInfo.frameworks);
    writeCppProperty("defines", cppInfo.defines);
    writeCppProperty("cFlags", cppInfo.cflags);
    writeCppProperty("cxxFlags", cppInfo.cxxflags);
    writeCppProperty("linkerFlags", (cppInfo.sharedlinkflags || []).concat(cppInfo.exelinkflags || []));

    writeProperty("resources", FileInfo.fromNativeSeparators(cppInfo.resdirs));

    const isForImport = targetOS.includes("windows")
    const libraryTypes = getLibraryTypes(moduleInfo.options);
    libraryTypes.forEach(function(libraryType){
        const cppInfoLibs = cppInfo.libs === null ? [] : cppInfo.libs;
        const libdirs = FileInfo.fromNativeSeparators(cppInfo.libdirs);
        const libs = findLibs(cppInfoLibs, libdirs, libraryType, targetOS, isForImport)
            .concat(cppInfo.system_libs === null ? [] : cppInfo.system_libs);
        const property = libraryType === "shared" ? "dynamicLibraries" : "staticLibraries";
        writeCppProperty(property, libs);
    });

    writeLine("}");

    var moduleFile = new TextFile(outputFilePath, TextFile.WriteOnly);
    moduleFile.write(moduleContent);
    moduleFile.close();

    return "";
}
