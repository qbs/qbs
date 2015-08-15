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
import qbs.FileInfo
import qbs.ModUtils

DarwinGCC {
    condition: qbs.hostOS.contains('osx') &&
               qbs.targetOS.contains('ios') &&
               qbs.toolchain && qbs.toolchain.contains('gcc')

    targetSystem: "ios" + (minimumIosVersion || "")

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
        inputsFromDependencies: ["bundle"]

        Artifact {
            filePath: FileInfo.joinPaths(product.destinationDirectory, product.targetName + ".ipa")
            fileTags: ["ipa"]
        }

        prepare: {
            var signingIdentity = product.moduleProperty("xcode", "actualSigningIdentity");
            var signingIdentityDisplay = product.moduleProperty("xcode",
                                                                "actualSigningIdentityDisplayName");
            if (!signingIdentity)
                throw "The name of a valid signing identity must be set using " +
                        "xcode.signingIdentity in order to build an IPA package.";

            var provisioningProfilePath = product.moduleProperty("xcode",
                                                                 "provisioningProfilePath");
            if (!provisioningProfilePath)
                throw "The path to a provisioning profile must be set using " +
                        "xcode.provisioningProfilePath in order to build an IPA package.";

            var args = [input.filePath,
                        "-o", output.filePath,
                        "--sign", signingIdentity,
                        "--embed", provisioningProfilePath];

            var cmd = new Command("PackageApplication", args);
            cmd.description = "creating ipa, signing with " + signingIdentityDisplay;
            cmd.highlight = "codegen";
            return cmd;
        }
    }
}
