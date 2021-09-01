/****************************************************************************
**
** Copyright (C) 2015 The Qt Company Ltd.
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
import qbs.Probes
import qbs.Utilities

import "utils.js" as NdkUtils

Module {
    Probes.AndroidNdkProbe {
        id: ndkProbe
        environmentPaths: (ndkDir ? [ndkDir] : []).concat(base)
    }

    version: ndkProbe.ndkVersion

    readonly property string abi: NdkUtils.androidAbi(qbs.architecture)
    PropertyOptions {
        name: "abi"
        description: "Supported Android ABIs"
        allowedValues: ["arm64-v8a", "armeabi-v7a", "x86", "x86_64"]
    }
    // From https://android.googlesource.com/platform/ndk/+/refs/heads/ndk-release-r21/docs/BuildSystemMaintainers.md
    // Android Studio‘s LLDB debugger uses a binary’s build ID to locate debug information. To
    // ensure that LLDB works with a binary, pass an option like -Wl,--build-id=sha1 to Clang when
    // linking. Other --build-id= modes are OK, but avoid a plain --build-id argument when using
    // LLD, because Android Studio‘s version of LLDB doesn’t recognize LLD's default 8-byte build
    // ID. See Issue 885.

    // Plain --build-id option is nevertheless implemented when buildId property is empty.

    // Possible values (from man page of ld.lld — ELF linker from the LLVM project):
    // one of fast, md5, sha1, tree, uuid, 0xhex-string, and none. tree is an alias for sha1.
    // Build-IDs of type fast, md5, sha1, and tree are calculated from the object contents. fast is
    // not intended to be cryptographically secure.
    property string buildId: "sha1"

    // See https://developer.android.com/ndk/guides/cpp-support.html
    property string appStl: "c++_shared"

    PropertyOptions {
        name: "appStl"
        description: "C++ Runtime Libraries."
        allowedValues: ["c++_static", "c++_shared"]
    }

    property string hostArch: ndkProbe.hostArch
    property string ndkDir: ndkProbe.path
    property string ndkSamplesDir: ndkProbe.samplesDir
    property string platform: {
        switch (abi) {
        case "armeabi-v7a":
        // case "x86": // x86 has broken wstring support
            return "android-19";
        default:
            return "android-21";
        }
    }

    property int platformVersion: {
        if (platform) {
            var match = platform.match(/^android-([0-9]+)$/);
            if (match !== null) {
                return parseInt(match[1], 10);
            }
        }
    }

    property stringList abis: {
        var list = ["armeabi-v7a"];
        if (platformVersion >= 16)
            list.push("x86");
        if (platformVersion >= 21)
            list.push("arm64-v8a", "x86_64");
        return list;
    }

    property string armMode: abi && abi.startsWith("armeabi")
            ? (qbs.buildVariant === "debug" ? "arm" : "thumb")
            : undefined;
    PropertyOptions {
        name: "armMode"
        description: "Determines the instruction set for armeabi configurations."
        allowedValues: ["arm", "thumb"]
    }

    validate: {
        if (!ndkDir) {
            throw ModUtils.ModuleError("Could not find an Android NDK at any of the following "
                                       + "locations:\n\t" + ndkProbe.candidatePaths.join("\n\t")
                                       + "\nInstall the Android NDK to one of the above locations, "
                                       + "or set the Android.ndk.ndkDir property or "
                                       + "ANDROID_NDK_ROOT environment variable to a valid "
                                       + "Android NDK location.");
        }

        if (Utilities.versionCompare(version, "19") < 0)
            throw ModUtils.ModuleError("Unsupported NDK version "
                                       + Android.ndk.version
                                       + ", please upgrade your NDK to r19+");

        if (product.aggregate && !product.multiplexConfigurationId)
            return;
        var validator = new ModUtils.PropertyValidator("Android.ndk");
        validator.setRequiredProperty("abi", abi);
        validator.setRequiredProperty("appStl", appStl);
        validator.setRequiredProperty("hostArch", hostArch);
        validator.setRequiredProperty("platform", platform);
        return validator.validate();
    }
}
