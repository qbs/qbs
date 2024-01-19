/****************************************************************************
**
** Copyright (C) 2021 Ivan Komissarov (abbapoh@gmail.com)
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

import qbs.Environment
import qbs.File
import qbs.FileInfo
import qbs.Host
import qbs.ModUtils
import qbs.PkgConfig
import qbs.ProviderUtils
import qbs.Probes
import qbs.Process
import qbs.TextFile

import "Qt/setup-qt.js" as SetupQt

ModuleProvider {
    property string executableFilePath
    property stringList extraPaths
    property stringList libDirs
    property bool staticMode: false
    property bool definePrefix: Host.os().includes("windows")

    // We take the sysroot default from qbs.sysroot, except for Xcode toolchains, where
    // the sysroot points into the Xcode installation and does not contain .pc files.
    property path sysroot: qbs.toolchain && qbs.toolchain.includes("xcode")
                           ? undefined : qbs.sysroot

    Probes.QbsPkgConfigProbe {
        id: theProbe
        // TODO: without explicit 'parent' we do not have access to the fake "qbs" scope
        _executableFilePath: parent.executableFilePath
        _extraPaths: parent.extraPaths
        _sysroot: parent.sysroot
        _libDirs: parent.libDirs
        _staticMode: parent.staticMode
        _definePrefix: parent.definePrefix
        _mergeDependencies: parent.mergeDependencies
    }

    Probes.QmakeProbe {
        id: qmakeProbe
        condition: moduleName.startsWith("Qt") && theProbe.qmakePaths
        qmakePaths: theProbe.qmakePaths
    }

    isEager: false

    relativeSearchPaths: {
        function getModuleInfo(pkg, staticMode) {
            var result = {};

            var mapper = function(flag) { return flag.value; }
            var typeFilter = function(type) {
                return function(flag) { return flag.type === type; }
            }

            function getLibsInfo(libs) {
                var result = {};
                result.dynamicLibraries = libs.filter(typeFilter(PkgConfig.LibraryName)).map(mapper);
                result.staticLibraries =
                        libs.filter(typeFilter(PkgConfig.StaticLibraryName)).map(mapper);
                result.libraryPaths = libs.filter(typeFilter(PkgConfig.LibraryPath)).map(mapper);
                result.frameworks = libs.filter(typeFilter(PkgConfig.Framework)).map(mapper);
                result.frameworkPaths =
                        libs.filter(typeFilter(PkgConfig.FrameworkPath)).map(mapper);
                result.driverLinkerFlags =
                        libs.filter(typeFilter(PkgConfig.LinkerFlag)).map(mapper);
                return result;
            }

            result.version = pkg.version;
            result.includePaths = pkg.cflags.filter(typeFilter(PkgConfig.IncludePath)).map(mapper);
            result.systemIncludePaths =
                    pkg.cflags.filter(typeFilter(PkgConfig.SystemIncludePath)).map(mapper);
            result.defines = pkg.cflags.filter(typeFilter(PkgConfig.Define)).map(mapper);
            result.commonCompilerFlags =
                    pkg.cflags.filter(typeFilter(PkgConfig.CompilerFlag)).map(mapper);

            var allLibs = pkg.libs;
            if (staticMode)
                allLibs = allLibs.concat(pkg.libsPrivate);
            var libsInfo = getLibsInfo(allLibs);
            for (var key in libsInfo) {
                result[key] = libsInfo[key];
            }

            return result;
        }

        function getModuleDependencies(pkg, staticMode) {
            var mapper = function(p) {
                var result = {};
                for (var key in p)
                    result[key] = p[key];
                result.name = ProviderUtils.pkgConfigToModuleName(result.name);
                return result;
            }
            var result = pkg.requires.map(mapper);
            if (staticMode)
                result = result.concat(pkg.requiresPrivate.map(mapper));
            return result;
        }

        console.debug("Running pkgconfig provider for " + moduleName + ".");

        var outputDir = FileInfo.joinPaths(outputBaseDir, "modules");
        File.makePath(outputDir);

        // TODO: ponder how we can solve forward mapping with Packages so we can fill deps
        var moduleMapping = {
            "protobuf": "protobuflib"
        }
        var reverseMapping = {}
        for (var key in moduleMapping)
            reverseMapping[moduleMapping[key]] = key

        if (moduleName.startsWith("Qt")) {
            function setupQt(packageName, qtInfos) {
                if (qtInfos === undefined || qtInfos.length === 0)
                    return [];
                var qtProviderDir = FileInfo.joinPaths(path, "Qt");
                return SetupQt.doSetup(packageName, qtInfos, outputBaseDir, qtProviderDir);
            }

            if (!sysroot) {
                return setupQt(moduleName, qmakeProbe.qtInfos);
            }
            return [];
        }

        var pkg;
        pkg = theProbe.packages[reverseMapping[moduleName]];
        if (pkg === undefined)
            pkg = theProbe.packagesByModuleName[moduleName];
        if (pkg === undefined)
            return [];

        if (pkg.isBroken) {
            console.warn("Failed to load " + moduleName + " as it's pkg-config package is broken");
            return [];
        }
        var moduleInfo = getModuleInfo(pkg, staticMode);
        var deps = getModuleDependencies(pkg, staticMode);

        var moduleDir = FileInfo.joinPaths(outputDir, moduleName);
        File.makePath(moduleDir);
        var module =
                new TextFile(FileInfo.joinPaths(moduleDir, "module.qbs"), TextFile.WriteOnly);
        module.writeLine("Module {");
        module.writeLine("    version: " + ModUtils.toJSLiteral(moduleInfo.version));
        module.writeLine("    Depends { name: 'cpp' }");
        deps.forEach(function(dep) {
            var depName = ProviderUtils.pkgConfigToModuleName(
                    moduleMapping[dep.name] ? moduleMapping[dep.name] : dep.name);
            module.write("    Depends { name: '" + depName + "'");
            for (var k in dep) {
                if (k === "name")
                    continue;
                module.write("; " + k + ": " + ModUtils.toJSLiteral(dep[k]));
            }
            module.writeLine(" }");
        })
        function writeProperty(propertyName) {
            var value = moduleInfo[propertyName];
            if (value.length !== 0) { // skip empty props for simplicity of the module file
                module.writeLine(
                        "    cpp." + propertyName + ":" + ModUtils.toJSLiteral(value));
            }
        }
        writeProperty("includePaths");
        writeProperty("systemIncludePaths");
        writeProperty("defines");
        writeProperty("commonCompilerFlags");
        writeProperty("dynamicLibraries");
        writeProperty("staticLibraries");
        writeProperty("libraryPaths");
        writeProperty("frameworks");
        writeProperty("frameworkPaths");
        writeProperty("driverLinkerFlags");
        module.writeLine("}");
        module.close();

        return "";
    }
}
