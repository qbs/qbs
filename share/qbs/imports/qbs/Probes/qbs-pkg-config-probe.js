/****************************************************************************
**
** Copyright (C) 2023 Ivan Komissarov (abbapoh@gmail.com)
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

var Environment = require("qbs.Environment");
var File = require("qbs.File");
var FileInfo = require("qbs.FileInfo");
var PkgConfig = require("qbs.PkgConfig");
var ProviderUtils = require("qbs.ProviderUtils");
var Process = require("qbs.Process");

function getQmakePaths(pkg) {
    var packageName = pkg.baseFileName;
    if (packageName === "QtCore"
            || packageName === "Qt5Core"
            || packageName === "Qt6Core") {
        var binDir = pkg.variables["bindir"] || pkg.variables["host_bins"];
        if (!binDir) {
            if (packageName === "QtCore") { // Qt4 does not have host_bins
                var mocLocation = pkg.variables["moc_location"];
                if (!mocLocation) {
                    console.warn("No moc_location variable in " + packageName);
                    return;
                }
                binDir = FileInfo.path(mocLocation);
            } else {
                console.warn("No 'bindir' or 'host_bins' variable in " + packageName);
                return;
            }
        }
        var suffix = FileInfo.executableSuffix();
        var ret = [];
        const infixes = ["", "5", "6"];
        for (var index in infixes) {
            const infix = infixes[index];
            var fileName = FileInfo.joinPaths(binDir, "qmake" + infix + suffix);
            if (File.exists(fileName)) {
                ret.push(fileName);
            }
        }
        return ret;
    }
}

function configure(
    executableFilePath, extraPaths, libDirs, staticMode, definePrefix, sysroot) {

    var result = {};
    result.packages = [];
    result.packagesByModuleName = {};
    result.brokenPackages = [];
    result.qtInfos = [];

    var options = {};
    options.libDirs = libDirs;
    options.sysroot = sysroot;
    options.definePrefix = definePrefix;
    if (options.sysroot)
        options.allowSystemLibraryPaths = true;
    options.staticMode = staticMode;
    options.extraPaths = extraPaths;
    if (options.sysroot && !options.libDirs) {
        options.libDirs = [
            options.sysroot + "/usr/lib/pkgconfig",
            options.sysroot + "/usr/share/pkgconfig"
        ];
    }
    if (!options.libDirs) {
        // if we have pkg-config/pkgconf installed, let's ask it for its search paths (since
        // built-in search paths can differ between platforms)
        if (executableFilePath) {
            var p = new Process()
            if (p.exec(executableFilePath, ['pkg-config', '--variable=pc_path']) === 0) {
                var stdout = p.readStdOut().trim();
                options.libDirs = stdout ? stdout.split(FileInfo.pathListSeparator()): [];
                var installDir = FileInfo.path(executableFilePath);
                options.libDirs = options.libDirs.map(function(path){
                    if (FileInfo.isAbsolutePath(path))
                        return path;
                    return FileInfo.cleanPath(FileInfo.joinPaths(installDir, path));
                });
            }
        }
    }
    var pkgConfig = new PkgConfig(options);
    result.packages = pkgConfig.packages();
    for (var packageName in result.packages) {
        var pkg = result.packages[packageName];
        var moduleName = ProviderUtils.pkgConfigToModuleName(packageName);
        result.packagesByModuleName[moduleName] = pkg;

        if (packageName.startsWith("Qt")) {
            if (!sysroot) {
                var qmakePaths = getQmakePaths(pkg);
                if (qmakePaths !== undefined)
                    result.qmakePaths = qmakePaths;
            }
        }
    }
    return result;
}
