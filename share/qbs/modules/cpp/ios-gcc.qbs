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

import qbs 1.0
import qbs.DarwinTools
import qbs.File
import qbs.ModUtils

DarwinGCC {
    condition: qbs.hostOS.contains('osx') &&
               qbs.targetOS.contains('ios') &&
               qbs.toolchain && qbs.toolchain.contains('gcc')

    // Setting a minimum is especially important for Simulator or CC/LD thinks the target is OS X
    minimumIosVersion: xcodeSdkVersion || (cxxStandardLibrary === "libc++" ? "5.0" : undefined)

    platformObjcFlags: base.concat(simulatorObjcFlags)
    platformObjcxxFlags: base.concat(simulatorObjcFlags)

    // Private properties
    readonly property stringList simulatorObjcFlags: {
        // default in Xcode and also required for building 32-bit Simulator binaries with ARC
        // since the default ABI version is 0 for 32-bit targets
        return qbs.targetOS.contains("ios-simulator")
                ? ["-fobjc-abi-version=2", "-fobjc-legacy-dispatch"]
                : [];
    }

    Rule {
        condition: !product.moduleProperty("qbs", "targetOS").contains("ios-simulator")
        multiplex: true
        inputs: ["qbs"]

        Artifact {
            filePath: product.destinationDirectory + "/"
                    + product.moduleProperty("bundle", "contentsFolderPath")
                    + "/ResourceRules.plist"
            fileTags: ["resourcerules"]
        }

        prepare: {
            var cmd = new JavaScriptCommand();
            cmd.description = "generating ResourceRules";
            cmd.highlight = "codegen";
            cmd.sysroot = product.moduleProperty("qbs","sysroot");
            cmd.sourceCode = function() {
                File.copy(sysroot + "/ResourceRules.plist", outputs.resourcerules[0].filePath);
            }
            return cmd;
        }
    }

    Rule {
        condition: product.moduleProperty("cpp", "buildIpa")
        multiplex: true
        inputs: ["application", "infoplist", "pkginfo", "resourcerules", "compiled_nib"]

        Artifact {
            filePath: product.destinationDirectory + "/" + product.targetName + ".ipa"
            fileTags: ["ipa"]
        }

        prepare: {
            var signingIdentity = product.moduleProperty("cpp", "signingIdentity");
            if (!signingIdentity)
                throw "The name of a valid signing identity must be set using " +
                        "cpp.signingIdentity in order to build an IPA package.";

            var provisioningProfile = product.moduleProperty("cpp", "provisioningProfile");
            if (!provisioningProfile)
                throw "The path to a provisioning profile must be set using " +
                        "cpp.provisioningProfile in order to build an IPA package.";

            var args = ["-sdk", product.moduleProperty("cpp", "xcodeSdkName"), "PackageApplication",
                        "-v", product.buildDirectory + "/" + product.moduleProperty("bundle", "bundleName"),
                        "-o", outputs.ipa[0].filePath, "--sign", signingIdentity,
                        "--embed", provisioningProfile];

            var command = "/usr/bin/xcrun";
            var cmd = new Command(command, args)
            cmd.description = "creating ipa, signing with " + signingIdentity;
            cmd.highlight = "codegen";
            cmd.workingDirectory = product.buildDirectory;
            return cmd;
        }
    }
}
