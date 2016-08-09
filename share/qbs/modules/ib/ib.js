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

var BundleTools = loadExtension("qbs.BundleTools");
var DarwinTools = loadExtension("qbs.DarwinTools");
var File = loadExtension("qbs.File");
var FileInfo = loadExtension("qbs.FileInfo");
var ModUtils = loadExtension("qbs.ModUtils");
var Process = loadExtension("qbs.Process");
var PropertyList = loadExtension("qbs.PropertyList");

function artifactsFromInputs(inputs) {
    var artifacts = [];
    for (var tag in inputs) {
        artifacts = artifacts.concat(inputs[tag]);
    }
    return artifacts;
}

function ibtooldArguments(product, inputs, outputs, overrideOutput) {
    var i;
    var args = [];
    var allInputs = artifactsFromInputs(inputs);

    var outputFormat = ModUtils.moduleProperty(product, "outputFormat");
    if (outputFormat) {
        if (!["binary1", "xml1", "human-readable-text"].contains(outputFormat))
            throw("Invalid ibtoold output format: " + outputFormat + ". " +
                  "Must be in [binary1, xml1, human-readable-text].");

        args.push("--output-format", outputFormat);
    }

    var debugFlags = ["warnings", "errors", "notices"];
    for (var j in debugFlags) {
        var flag = debugFlags[j];
        if (ModUtils.modulePropertyFromArtifacts(product, allInputs, product.moduleName, flag)) {
            args.push("--" + flag);
        }
    }

    if (inputs.assetcatalog) {
        args.push("--platform", DarwinTools.applePlatformName(
                      product.moduleProperty("qbs", "targetOS"),
                      product.moduleProperty("xcode", "platformType")));

        var appIconName = ModUtils.modulePropertyFromArtifacts(product, inputs.assetcatalog, product.moduleName, "appIconName");
        if (appIconName)
            args.push("--app-icon", appIconName);

        var launchImageName = ModUtils.modulePropertyFromArtifacts(product, inputs.assetcatalog, product.moduleName, "launchImageName");
        if (launchImageName)
            args.push("--launch-image", launchImageName);

        // Undocumented but used by Xcode (only for iOS?), probably runs pngcrush or equivalent
        if (ModUtils.modulePropertyFromArtifacts(product, inputs.assetcatalog, product.moduleName, "compressPngs"))
            args.push("--compress-pngs");
    } else {
        var sysroot = product.moduleProperty("qbs", "sysroot");
        if (sysroot)
            args.push("--sdk", sysroot);

        args.push("--flatten", ModUtils.modulePropertyFromArtifacts(product, allInputs, product.moduleName, "flatten") ? 'YES' : 'NO');

        // --module and --auto-activate-custom-fonts were introduced in Xcode 6.0
        if (ModUtils.moduleProperty(product, "ibtoolVersionMajor") >= 6) {
            var module = ModUtils.moduleProperty(product, "module");
            if (module)
                args.push("--module", module);

            if (ModUtils.modulePropertyFromArtifacts(product, allInputs, product.moduleName, "autoActivateCustomFonts"))
                args.push("--auto-activate-custom-fonts");
        }
    }

    // --minimum-deployment-target was introduced in Xcode 5.0
    var minimumDarwinVersion = product.moduleProperty("cpp", "minimumDarwinVersion");
    if (minimumDarwinVersion && ModUtils.moduleProperty(product, "ibtoolVersionMajor") >= 5)
        args.push("--minimum-deployment-target", minimumDarwinVersion);

    // --target-device and -output-partial-info-plist were introduced in Xcode 6.0 for ibtool
    if (ModUtils.moduleProperty(product, "ibtoolVersionMajor") >= 6 || inputs.assetcatalog) {
        args.push("--output-partial-info-plist", (outputs && outputs.partial_infoplist)
                  ? outputs.partial_infoplist[0].filePath
                  : "/dev/null");

        // For iOS, we'd normally only output the devices specified in TARGETED_DEVICE_FAMILY
        // We can't get this info from Info.plist keys due to dependency order, so use the qbs prop
        var targetDevices = ModUtils.moduleProperty(product, "targetDevices");
        for (i in targetDevices) {
            args.push("--target-device", targetDevices[i]);
        }
    }

    args = args.concat(ModUtils.modulePropertiesFromArtifacts(product, allInputs,
                                                              product.moduleName, "flags"));

    if (overrideOutput) {
        args.push("--compile", overrideOutput);
    } else {
        if (outputs.compiled_ibdoc_main)
            args.push("--compile", outputs.compiled_ibdoc_main[0].filePath);

        if (outputs.compiled_assetcatalog)
            args.push("--compile",
                      BundleTools.destinationDirectoryForResource(product, inputs.assetcatalog[0]));
    }

    for (i in allInputs)
        args.push(allInputs[i].filePath);

    return args;
}

function ibtoolFileTaggers(fileTags) {
    var ext;
    if (fileTags.contains("nib") && !fileTags.contains("storyboard"))
        ext = "nib";
    if (fileTags.contains("storyboard") && !fileTags.contains("nib"))
        ext = "storyboard";

    if (!ext)
        throw "unknown ibtool input file tags: " + fileTags;

    var t = "compiled_ibdoc";
    return {
        ".nib": [t, "compiled_" + ext + (ext !== "nib" ? "_nib" : "")],
        ".plist": [t, "compiled_" + ext + "_infoplist"],
        ".storyboard": [t, "compiled_" + ext]
    };
}

function ibtoolOutputArtifacts(product, inputs, input) {
    var suffix = input.completeBaseName;
    if (input.fileTags.contains("nib"))
        suffix += ModUtils.moduleProperty(product, "compiledNibSuffix");
    else if (input.fileTags.contains("storyboard"))
        suffix += ModUtils.moduleProperty(product, "compiledStoryboardSuffix");

    var tracker = new ModUtils.BlackboxOutputArtifactTracker();
    tracker.hostOS = product.moduleProperty("qbs", "hostOS");
    tracker.shellPath = product.moduleProperty("qbs", "shellPath");
    tracker.fileTaggers = ibtoolFileTaggers(input.fileTags);
    tracker.command = ModUtils.moduleProperty(product, "ibtoolPath");
    tracker.commandArgsFunction = function (outputDirectory) {
        // Last --output-format argument overrides any previous ones
        // Append the name of the base output since it can be either a file or a directory
        // in the case of XIB compilations
        return ibtooldArguments(product, inputs,
                                undefined, FileInfo.joinPaths(outputDirectory, suffix))
            .concat(["--output-format", "xml1"]);
    };

    var artifacts = tracker.artifacts(
                FileInfo.joinPaths(BundleTools.destinationDirectoryForResource(product, input)));

    if (product.moduleProperty("ib", "ibtoolVersionMajor") >= 6) {
        var prefix = input.fileTags.contains("storyboard") ? "SB" : "";
        var path = FileInfo.joinPaths(product.destinationDirectory, input.completeBaseName +
                                      "-" + prefix + "PartialInfo.plist");
        artifacts.push({ filePath: path, fileTags: ["partial_infoplist"] });
    }

    // Tag the "main" output
    // This can be either a file or directory so the artifact might already exist in the output list
    var main = FileInfo.joinPaths(BundleTools.destinationDirectoryForResource(product, input),
                                  suffix);
    var mainTags = ["compiled_ibdoc", "compiled_ibdoc_main"];
    var mainIndex = -1;
    for (var i = 0; i < artifacts.length; ++i) {
        if (artifacts[i].filePath === main) {
            mainIndex = i;
            break;
        }
    }

    if (mainIndex === -1) {
        // artifact not in list - the output was a directory (unflatted nib or storyboard)
        artifacts.splice(0, 0, {
            filePath: main,
            fileTags: mainTags
        });
    } else {
        // artifact in list - the output was a file (flattened nib)
        artifacts[mainIndex].fileTags = mainTags.uniqueConcat(artifacts[mainIndex].fileTags);
    }

    return artifacts;
}

function actoolOutputArtifacts(product, inputs) {
    // actool has no --dry-run option (rdar://21786925),
    // so compile to a fake temporary directory in order to extract the list of output files
    var tracker = new ModUtils.BlackboxOutputArtifactTracker();
    tracker.hostOS = product.moduleProperty("qbs", "hostOS");
    tracker.shellPath = product.moduleProperty("qbs", "shellPath");
    tracker.command = ModUtils.moduleProperty(product, "actoolPath");
    tracker.commandArgsFunction = function (outputDirectory) {
        // Last --output-format argument overrides any previous ones
        return ibtooldArguments(product, inputs,
                                undefined, outputDirectory).concat(["--output-format", "xml1"]);
    };
    tracker.processStdOutFunction = parseActoolOutput;
    var artifacts = tracker.artifacts(BundleTools.destinationDirectoryForResource(product,
            inputs.assetcatalog[0]));

    // Newer versions of actool don't generate *anything* if there's no input;
    // in that case a partial Info.plist would not have been generated either
    if (artifacts && artifacts.length > 0) {
        artifacts.push({
            filePath: FileInfo.joinPaths(product.destinationDirectory,
                                         "assetcatalog_generated_info.plist"),
            fileTags: ["partial_infoplist"]
        });
    }

    return artifacts;
}

function parseActoolOutput(output) {
    var propertyList = new PropertyList();
    try {
        propertyList.readFromString(output);

        var plist = propertyList.toObject();
        if (plist)
            plist = plist["com.apple.actool.compilation-results"];
        if (plist) {
            var artifacts = [];
            files = plist["output-files"];
            for (var i in files) {
                if (files[i] === "/dev/null")
                    continue;
                var tags = files[i].endsWith(".plist")
                        ? ["partial_infoplist"]
                        : ["compiled_assetcatalog"];
                artifacts.push({
                    filePath: files[i],
                    fileTags: tags
                });
            }

            return artifacts;
        }
    } finally {
        propertyList.clear();
    }
}

function ibtoolVersion(ibtool) {
    var process;
    var version;
    try {
        process = new Process();
        if (process.exec(ibtool, ["--version", "--output-format", "xml1"], true) !== 0)
            console.error(process.readStdErr());

        var propertyList = new PropertyList();
        try {
            propertyList.readFromString(process.readStdOut());

            var plist = propertyList.toObject();
            if (plist)
                plist = plist["com.apple.ibtool.version"];
            if (plist)
                version = plist["short-bundle-version"];
        } finally {
            propertyList.clear();
        }
    } finally {
        process.close();
    }
    return version;
}
