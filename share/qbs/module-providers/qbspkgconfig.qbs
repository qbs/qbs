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
import qbs.Process
import qbs.TextFile

import "Qt/setup-qt.js" as SetupQt

ModuleProvider {
    property string executableFilePath
    property stringList extraPaths
    property stringList libDirs
    property bool staticMode: false
    property path sysroot: {
        if (qbs.targetOS.contains("macos"))
            return "";
        return qbs.sysroot;
    }
    property bool mergeDependencies: true

    relativeSearchPaths: {

        function exeSuffix(qbs) { return Host.os().contains("windows") ? ".exe" : ""; }

        // we need Probes in Providers...
        function getPkgConfigExecutable(qbs) {
            function splitNonEmpty(s, c) { return s.split(c).filter(function(e) { return e; }) }

            var pathValue = Environment.getEnv("PATH");
            if (!pathValue)
                return undefined;
            var dirs = splitNonEmpty(pathValue, FileInfo.pathListSeparator());
            var suffix = exeSuffix(qbs);
            var filePaths = [];
            for (var i = 0; i < dirs.length; ++i) {
                var candidate = FileInfo.joinPaths(dirs[i], "pkg-config" + suffix);
                var canonicalCandidate = FileInfo.canonicalPath(candidate);
                if (!canonicalCandidate || !File.exists(canonicalCandidate))
                    continue;
                return canonicalCandidate;
            }
            return undefined;
        }

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

        function getModuleName(packageName) { return packageName.replace('.', '-'); }

        function getModuleDependencies(pkg, staticMode) {
            var mapper = function(p) {
                var result = {};
                for (var key in p)
                    result[key] = p[key];
                result.name = getModuleName(result.name);
                return result;
            }
            var result = pkg.requires.map(mapper);
            if (staticMode)
                result = result.concat(pkg.requiresPrivate.map(mapper));
            return result;
        }

        console.debug("Running pkgconfig provider.")

        var outputDir = FileInfo.joinPaths(outputBaseDir, "modules");
        File.makePath(outputDir);

        var options = {};
        options.libDirs = libDirs;
        options.sysroot = sysroot;
        options.staticMode = staticMode;
        options.mergeDependencies = mergeDependencies;
        options.extraPaths = extraPaths;
        if (options.sysroot && !options.libDirs) {
            options.libDirs = [
                sysroot + "/usr/lib/pkgconfig",
                sysroot + "/usr/share/pkgconfig"
            ];
        }
        if (!options.libDirs) {
            // if we have pkg-config installed, let's ask it for its search paths (since
            // built-in search paths can differ between platforms)
            var executable = executableFilePath ? executableFilePath : getPkgConfigExecutable(qbs);
            if (executable) {
                var p = new Process()
                if (p.exec(executable, ['pkg-config', '--variable=pc_path']) === 0) {
                    var stdout = p.readStdOut().trim();
                    // TODO: pathListSeparator? depends on what pkg-config prints on Windows
                    options.libDirs = stdout ? stdout.split(':'): [];
                }
            }
        }

        function setupQt(pkg) {
            var packageName = pkg.baseFileName;
            if (packageName === "QtCore"
                    || packageName === "Qt5Core"
                    || packageName === "Qt6Core") {
                var hostBins = pkg.variables["host_bins"];
                if (!hostBins) {
                    if (packageName === "QtCore") { // Qt4 does not have host_bins
                        var mocLocation = pkg.variables["moc_location"];
                        if (!mocLocation) {
                            console.warn("No moc_location variable in " + packageName);
                            return;
                        }
                        hostBins = FileInfo.path(mocLocation);
                    } else {
                        console.warn("No host_bins variable in " + packageName);
                        return;
                    }
                }
                var suffix = exeSuffix(qbs);
                var qmakePaths = [FileInfo.joinPaths(hostBins, "qmake" + suffix)];
                var qtProviderDir = FileInfo.joinPaths(path, "Qt");
                SetupQt.doSetup(qmakePaths, outputBaseDir, qtProviderDir, qbs);
            }
        }

        var moduleMapping = {
            "protobuf": "protobuflib",
            "grpc++": "grpcpp"
        }

        var pkgConfig = new PkgConfig(options);

        var brokenPackages = [];
        var packages = pkgConfig.packages();
        for (var packageName in packages) {
            var pkg = packages[packageName];
            if (pkg.isBroken) {
                brokenPackages.push(pkg);
                continue;
            }
            if (packageName.startsWith("Qt")) {
                setupQt(pkg);
                continue;
            }
            var moduleName = moduleMapping[packageName]
                    ? moduleMapping[packageName]
                    : getModuleName(packageName);
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
                module.write("    Depends { name: '" + dep.name + "'");
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
        }

        if (brokenPackages.length !== 0) {
            console.warn("Failed to load some pkg-config packages:");
            for (var i = 0; i < brokenPackages.length; ++i) {
                console.warn("    " + brokenPackages[i].filePath
                    + ": " + brokenPackages[i].errorText);
            }
        }

        return "";
    }
}
