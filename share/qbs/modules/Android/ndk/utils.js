/****************************************************************************
**
** Copyright (C) 2015 The Qt Company Ltd.
** Contact: http://www.qt.io/licensing
**
** This file is part of the Qt Build Suite.
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

function abiNameToDirName(abiName) {
    if (abiName.startsWith("armeabi"))
        return "arm";
    if (abiName.startsWith("arm64"))
        return "arm64";
    return abiName;
}

function androidAbi(arch) {
    if (arch === "x86" || arch === "x86_64")
        return arch;
    return {
        "arm64": "arm64-v8a",
        "armv5": "armeabi",
        "armv7": "armeabi-v7a",
        "mipsel": "mips",
        "mips64el": "mips64"
    }[arch];
}

function toolchainDir(toolchain, version, abi) {
    if (toolchain && toolchain.contains("clang"))
        return "llvm-" + version;
    if (["x86", "x86_64"].contains(abi))
        return abi + "-" + version;
    return toolchainPrefix(toolchain, abi) + version;
}

function toolchainPrefix(toolchain, abi) {
    if (toolchain && toolchain.contains("clang"))
        return undefined;
    return {
        "arm64-v8a": "aarch64-linux-android-",
        "armeabi": "arm-linux-androideabi-",
        "armeabi-v7a": "arm-linux-androideabi-", // same prefix as above
        "mips": "mipsel-linux-android-",
        "mips64": "mips64el-linux-android-",
        "x86": "i686-linux-android-",
        "x86_64": "x86_64-linux-android-"
    }[abi];
}

function commonCompilerFlags(buildVariant, abi, hardFloat, armMode) {
    var flags = ["-ffunction-sections", "-funwind-tables", "-no-canonical-prefixes",
                 "-Wa,--noexecstack", "-Werror=format-security"];

    if (buildVariant === "debug")
        flags.push("-fno-omit-frame-pointer", "-fno-strict-aliasing");
    if (buildVariant === "release")
        flags.push("-fomit-frame-pointer");

    if (abi === "arm64-v8a") {
        flags.push("-fpic", "-fstack-protector", "-funswitch-loops", "-finline-limit=300");
        if (buildVariant === "release")
            flags.push("-fstrict-aliasing");
    }

    if (abi === "armeabi" || abi === "armeabi-v7a") {
        flags.push("-fpic", "-fstack-protector", "-finline-limit=64");

        if (abi === "armeabi")
            flags.push("-march=armv5te", "-mtune=xscale", "-msoft-float");

        if (abi === "armeabi-v7a") {
            flags.push("-march=armv7-a", "-mfpu=vfpv3-d16");
            flags.push(hardFloat ? "-mhard-float" : "-mfloat-abi=softfp");
        }

        if (buildVariant === "release")
            flags.push("-fno-strict-aliasing");
    }

    if (abi === "mips" || abi === "mips64") {
        flags.push("-fpic", "-finline-functions", "-fmessage-length=0",
                   "-fno-inline-functions-called-once", "-fgcse-after-reload",
                   "-frerun-cse-after-loop", "-frename-registers");
        if (buildVariant === "release")
            flags.push("-funswitch-loops", "-finline-limit=300", "-fno-strict-aliasing");
    }

    if (abi === "x86" || abi === "x86_64") {
        flags.push("-fstack-protector", "-funswitch-loops", "-finline-limit=300");
        if (buildVariant === "release")
            flags.push("-fstrict-aliasing");
    }

    if (armMode)
        flags.push("-m" + armMode);

    return flags;
}

function commonLinkerFlags(abi, hardFloat) {
    var flags = ["-no-canonical-prefixes", "-Wl,-z,noexecstack", "-Wl,-z,relro", "-Wl,-z,now"];

    if (abi === "armeabi-v7a") {
        flags.push("-march=armv7-a", "-Wl,--fix-cortex-a8");
        if (hardFloat)
            flags.push("-Wl,-no-warn-mismatch");
    }

    return flags;
}
