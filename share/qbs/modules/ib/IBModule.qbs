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

import qbs.BundleTools
import qbs.DarwinTools
import qbs.File
import "IBModuleBase.qbs" as IBModuleBase
import qbs.Host
import qbs.FileInfo
import qbs.ModUtils
import qbs.Process
import 'ib.js' as Ib

IBModuleBase {
    Depends { name: "xcode"; required: false }

    Probe {
        id: ibProbe
        property string toolPath: ibtoolPath // input
        property string toolVersion // output
        configure: {
            toolVersion = Ib.ibtoolVersion(toolPath);
            found = true;
        }
    }

    priority: 0
    condition: Host.os().includes("darwin") && qbs.targetOS.includes("darwin")

    warnings: true
    errors: true
    notices: true

    flags: []

    // tiffutil specific
    tiffutilName: "tiffutil"
    tiffutilPath: FileInfo.joinPaths("/usr/bin", tiffutilName)
    combineHidpiImages: true

    // iconutil specific
    iconutilName: "iconutil"
    iconutilPath: FileInfo.joinPaths("/usr/bin", iconutilName)

    // XIB/NIB specific
    ibtoolName: "ibtool"
    ibtoolPath: FileInfo.joinPaths(xcode.developerPath, "/usr/bin", ibtoolName)
    flatten: true
    module: ""
    autoActivateCustomFonts: true

    // Asset catalog specific
    actoolName: xcode.present ? "actool" : "ictool"
    actoolPath: FileInfo.joinPaths(xcode.developerPath, "/usr/bin", actoolName)
    appIconName: ""
    launchImageName: ""
    compressPngs: true

    // private properties
    outputFormat: "human-readable-text"
    tiffSuffix: ".tiff"
    appleIconSuffix: ".icns"
    compiledAssetCatalogSuffix: ".car"
    compiledNibSuffix: ".nib"
    compiledStoryboardSuffix: ".storyboardc"

    version: ibtoolVersion
    ibtoolVersion: ibProbe.toolVersion
    ibtoolVersionParts: ibtoolVersion ? ibtoolVersion.split('.').map(function(item) { return parseInt(item, 10); }) : []
    ibtoolVersionMajor: ibtoolVersionParts[0]
    ibtoolVersionMinor: ibtoolVersionParts[1]
    ibtoolVersionPatch: ibtoolVersionParts[2]

    targetDevices: xcode.present
                   ? xcode.targetDevices
                   : DarwinTools.targetDevices(qbs.targetOS)

    validate: {
        var validator = new ModUtils.PropertyValidator("ib");
        validator.setRequiredProperty("ibtoolVersion", ibtoolVersion);
        validator.setRequiredProperty("ibtoolVersionMajor", ibtoolVersionMajor);
        validator.setRequiredProperty("ibtoolVersionMinor", ibtoolVersionMinor);
        validator.addVersionValidator("ibtoolVersion", ibtoolVersion, 2, 3);
        validator.addRangeValidator("ibtoolVersionMajor", ibtoolVersionMajor, 1);
        validator.addRangeValidator("ibtoolVersionMinor", ibtoolVersionMinor, 0);
        if (ibtoolVersionPatch !== undefined)
            validator.addRangeValidator("ibtoolVersionPatch", ibtoolVersionPatch, 0);
        validator.validate();
    }

    FileTagger {
        patterns: ["*.png"]
        fileTags: ["png"]
    }

    FileTagger {
        patterns: ["*.iconset"] // bundle
        fileTags: ["iconset"]
    }

    FileTagger {
        patterns: ["*.nib", "*.xib"]
        fileTags: ["nib"]
    }

    FileTagger {
        patterns: ["*.storyboard"]
        fileTags: ["storyboard"]
    }

    FileTagger {
        patterns: ["*.xcassets"] // bundle
        fileTags: ["assetcatalog"]
    }

    FileTagger {
        patterns: ["*.icon"] // bundle
        fileTags: ["iconcomposer"]
    }

    Rule {
        multiplex: true
        inputs: ["png"]

        outputFileTags: ["tiff"]
        outputArtifacts: Ib.tiffutilArtifacts(product, inputs)

        prepare: Ib.prepareTiffutil.apply(Ib, arguments)
    }

    Rule {
        inputs: ["iconset"]

        outputFileTags: ["icns", "bundle.input"]
        outputArtifacts: ([{
            filePath: FileInfo.joinPaths(product.destinationDirectory, input.completeBaseName +
                                         ModUtils.moduleProperty(product, "appleIconSuffix")),
            fileTags: ["icns", "bundle.input"],
            bundle: {
                _bundleFilePath: FileInfo.joinPaths(BundleTools.destinationDirectoryForResource(product, input),
                                                    input.completeBaseName +
                                                    ModUtils.moduleProperty(product, "appleIconSuffix"))
            }
        }])

        prepare: {
            var args = ["--convert", "icns", "--output", output.filePath, input.filePath];
            var cmd = new Command(ModUtils.moduleProperty(product, "iconutilPath"), args);
            cmd.description = "compiling " + input.fileName;
            return cmd;
        }
    }

    Rule {
        inputs: ["nib", "storyboard"]
        outputFileTags: {
            var tags = ["partial_infoplist"];
            for (var i = 0; i < inputs.length; ++i)
                tags = tags.uniqueConcat(ModUtils.allFileTags(Ib.ibtoolFileTaggers(inputs[i])));
            return tags;
        }
        outputArtifacts: Ib.ibtoolOutputArtifacts(product, inputs, input)
        prepare: Ib.ibtoolCommands.apply(Ib, arguments)
    }

    Rule {
        inputs: ["assetcatalog", "iconcomposer"]
        multiplex: true
        outputArtifacts: Ib.actoolOutputArtifacts(product, inputs)
        outputFileTags: ["bundle.input", "compiled_assetcatalog", "partial_infoplist"]
        prepare: Ib.compileAssetCatalogCommands.apply(Ib, arguments)
    }
}
