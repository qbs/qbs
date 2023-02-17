/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
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
import qbs.ModUtils
import qbs.TextFile
import qbs.Utilities
import "../../modules/Android/ndk/utils.js" as NdkUtils
import 'gcc.js' as Gcc

LinuxGCC {
    Depends { name: "Android.ndk" }

    condition: qbs.targetOS.includes("android") && qbs.toolchain && qbs.toolchain.includes("llvm")
    priority: 2
    rpaths: []

    // toolchains/llvm/prebuilt/linux-x86_64/sysroot/usr/lib/x86_64-linux-android
    cxxLanguageVersion: "c++14"

    property string archLibsDir:  {
        switch (qbs.architecture) {
        case "arm64":
            return "aarch64";
        case "armv7a":
            return "arm";
        case "x86_64":
            return qbs.architecture;
        case "x86":
            return "i686";
        }
    }
    property string targetDir: "android" + (["armeabi", "armeabi-v7a"].includes(Android.ndk.abi) ? "eabi" : "")
    property string triple: [archLibsDir, targetSystem, targetDir].join("-")
    property string libsDir: FileInfo.joinPaths(sysroot, "usr", "lib", triple);

    property string sharedStlFilePath: (libsDir && Android.ndk.appStl.endsWith("_shared"))
        ? FileInfo.joinPaths(libsDir, dynamicLibraryPrefix + Android.ndk.appStl + dynamicLibrarySuffix)
        : undefined
    property string staticStlFilePath: (libsDir && Android.ndk.appStl.endsWith("_static"))
        ? FileInfo.joinPaths(libsDir, Android.ndk.platformVersion,
                             NdkUtils.stlFileName(staticLibraryPrefix, Android.ndk,
                                                  staticLibrarySuffix))
        : undefined

    Group {
        name: "Android STL"
        condition: product.cpp.sharedStlFilePath && product.cpp.shouldLink
        files: product.cpp.sharedStlFilePath ? [product.cpp.sharedStlFilePath] : []
        fileTags: "android.stl"
    }

    toolchainInstallPath: FileInfo.joinPaths(Android.ndk.ndkDir, "toolchains",
                                             "llvm", "prebuilt",
                                             Android.ndk.hostArch, "bin")

    property string toolchainTriple: [targetAbi === "androideabi" ? "arm" : targetArch,
                                      targetSystem, targetAbi].join("-")

    internalVersion: undefined
    toolchainPrefix: undefined

    machineType: {
        if (Android.ndk.abi === "armeabi-v7a")
            return "armv7-a";
    }

    qbs.optimization: targetAbi === "androideabi" ? "small" : base

    enableExceptions: Android.ndk.appStl !== "system"
    enableRtti: Android.ndk.appStl !== "system"

    commonCompilerFlags: NdkUtils.commonCompilerFlags(qbs.toolchain, qbs.buildVariant, Android.ndk)

    linkerFlags: NdkUtils.commonLinkerFlags(Android.ndk);
    driverLinkerFlags: {
        var flags = ["-fuse-ld=lld", "-Wl,--exclude-libs,libgcc.a", "-nostdlib++"];
        // See https://android.googlesource.com/platform/ndk/+/ndk-release-r21/docs/BuildSystemMaintainers.md#Unwinding
        if (Android.ndk.abi === "armeabi-v7a") {
            flags = flags.concat(["-Wl,--exclude-libs,libgcc_real.a"]);
            if (Android.ndk.appStl.startsWith("c++"))
                flags = flags.concat(["-Wl,--exclude-libs,libunwind.a"]);
        }
        return flags;
    }

    platformDriverFlags: ["-fdata-sections", "-ffunction-sections", "-funwind-tables",
                          "-fstack-protector-strong", "-no-canonical-prefixes"]

    dynamicLibraries: {
        var libs = ["c", "m"];
        if (sharedStlFilePath)
            libs.push(FileInfo.joinPaths(libsDir, Android.ndk.platformVersion,
                                         NdkUtils.stlFileName(dynamicLibraryPrefix, Android.ndk,
                                                              dynamicLibrarySuffix)));
        return libs;
    }
    staticLibraries: staticStlFilePath

    // When using ndk r19c, llvm doesn't add sysroot/usr/include/c++/v1 to the path
    // But it works starting with ndk r20b
    systemIncludePaths: (Utilities.versionCompare(Android.ndk.version, "20") < 0) ?
                            FileInfo.joinPaths(sysroot, "usr", "include", "c++", "v1") : []

    defines: ["ANDROID", "__ANDROID__"]

    binutilsPath: FileInfo.joinPaths(Android.ndk.ndkDir, "toolchains", "llvm", "prebuilt",
                                     Android.ndk.hostArch, "bin");
    binutilsPathPrefix: FileInfo.joinPaths(binutilsPath, "llvm-")
    sysroot: FileInfo.joinPaths(Android.ndk.ndkDir, "toolchains", "llvm", "prebuilt",
                                Android.ndk.hostArch, "sysroot")

    targetArch: {
        switch (qbs.architecture) {
        case "arm64":
            return "aarch64";
        case "armv5":
        case "armv5te":
            return "armv5te";
        case "armv7a":
        case "x86_64":
            return qbs.architecture;
        case "x86":
            return "i686";
        }
    }

    target: [targetArch, targetSystem, targetAbi].join("-")
    targetSystem: "linux"
    targetAbi: "android" + (["armeabi", "armeabi-v7a"].includes(Android.ndk.abi) ? "eabi" : "") +
               Android.ndk.platformVersion

    endianness: "little"

    Rule {
        condition: shouldLink
        inputs: "dynamiclibrary"
        Artifact {
            filePath: FileInfo.joinPaths("stripped-libs", input.fileName)
            fileTags: "android.nativelibrary"
        }
        prepare: {
            var stripArgs = ["--strip-all", "-o", output.filePath, input.filePath];
            var stripCmd = new Command(product.cpp.stripPath, stripArgs);
            stripCmd.description = "stripping unneeded symbols from " + input.fileName;
            return stripCmd;
        }
    }

    _skipAllChecks: !shouldLink

    validate: {
        if (_skipAllChecks)
            return;
        var baseValidator = new ModUtils.PropertyValidator("qbs");
        baseValidator.addCustomValidator("architecture", targetArch, function (value) {
            return value !== undefined;
        }, "unknown Android architecture '" + qbs.architecture + "'.");

        var validator = new ModUtils.PropertyValidator("cpp");
        validator.setRequiredProperty("targetArch", targetArch);

        return baseValidator.validate() && validator.validate();
    }
}
