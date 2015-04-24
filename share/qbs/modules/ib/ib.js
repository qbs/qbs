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

var DarwinTools = loadExtension("qbs.DarwinTools");
var FileInfo = loadExtension("qbs.FileInfo");
var ModUtils = loadExtension("qbs.ModUtils");
var Process = loadExtension("qbs.Process");
var PropertyList = loadExtension("qbs.PropertyList");

function ibtooldArguments(product, input, outputs) {
    var args = [];

    var outputFormat = ModUtils.moduleProperty(input, "outputFormat");
    if (outputFormat) {
        if (!["binary1", "xml1", "human-readable-text"].contains(outputFormat))
            throw("Invalid ibtoold output format: " + outputFormat + ". " +
                  "Must be in [binary1, xml1, human-readable-text].");

        args.push("--output-format", outputFormat);
    }

    if (ModUtils.moduleProperty(input, "warnings"))
        args.push("--warnings");

    if (ModUtils.moduleProperty(input, "errors"))
        args.push("--errors");

    if (ModUtils.moduleProperty(input, "notices"))
        args.push("--notices");

    if (input.fileTags.contains("assetcatalog")) {
        args.push("--platform", DarwinTools.applePlatformName(product.moduleProperty("qbs", "targetOS")));

        var appIconName = ModUtils.moduleProperty(input, "appIconName");
        if (appIconName)
            args.push("--app-icon", appIconName);

        var launchImageName = ModUtils.moduleProperty(input, "launchImageName");
        if (launchImageName)
            args.push("--launch-image", launchImageName);

        // Undocumented but used by Xcode (only for iOS?), probably runs pngcrush or equivalent
        if (ModUtils.moduleProperty(input, "compressPngs"))
            args.push("--compress-pngs");
    } else {
        var sysroot = product.moduleProperty("qbs", "sysroot");
        if (sysroot)
            args.push("--sdk", sysroot);

        args.push("--flatten", ModUtils.moduleProperty(input, "flatten") ? 'YES' : 'NO');

        // --module and --auto-activate-custom-fonts were introduced in Xcode 6.0
        if (ModUtils.moduleProperty(input, "ibtoolVersionMajor") >= 6) {
            var module = ModUtils.moduleProperty(input, "module");
            if (module)
                args.push("--module", module);

            if (ModUtils.moduleProperty(input, "autoActivateCustomFonts"))
                args.push("--auto-activate-custom-fonts");
        }
    }

    // --minimum-deployment-target was introduced in Xcode 5.0
    if (ModUtils.moduleProperty(input, "ibtoolVersionMajor") >= 5) {
        if (product.moduleProperty("cpp", "minimumOsxVersion")) {
            args.push("--minimum-deployment-target");
            args.push(product.moduleProperty("cpp", "minimumOsxVersion"));
        }

        if (product.moduleProperty("cpp", "minimumIosVersion")) {
            args.push("--minimum-deployment-target");
            args.push(product.moduleProperty("cpp", "minimumIosVersion"));
        }
    }

    // --target-device and -output-partial-info-plist were introduced in Xcode 6.0 for ibtool
    if (ModUtils.moduleProperty(input, "ibtoolVersionMajor") >= 6 || input.fileTags.contains("assetcatalog")) {
        args.push("--output-partial-info-plist", outputs.partial_infoplist[0].filePath);

        if (product.moduleProperty("qbs", "targetOS").contains("osx"))
            args.push("--target-device", "mac");

        if (product.moduleProperty("qbs", "targetOS").contains("ios")) {
            // TODO: Only output the devices specified in TARGET_DEVICE_FAMILY
            // We can't get this info from Info.plist keys due to dependency order
            args.push("--target-device", "iphone");
            args.push("--target-device", "ipad");
        }
    }

    var flags = ModUtils.moduleProperty(input, "flags");
    if (flags)
        args = args.concat(flags);

    if (outputs.compiled_ibdoc)
        args.push("--compile", outputs.compiled_ibdoc[0].filePath);

    if (outputs.compiled_assetcatalog)
        args.push("--compile", FileInfo.path(outputs.compiled_assetcatalog[0].filePath));

    args.push(input.filePath);

    return args;
}

function runActool(actool, args) {
    var process;
    var outputFilePaths;
    try {
        process = new Process();

        // Last --output-format argument overrides any previous ones
        if (process.exec(actool, args.concat(["--output-format", "xml1"]), true) !== 0)
            print(process.readStdErr());

        var propertyList = new PropertyList();
        try {
            propertyList.readFromString(process.readStdOut());

            var plist = propertyList.toObject();
            if (plist)
                plist = plist["com.apple.actool.compilation-results"];
            if (plist)
                outputFilePaths = plist["output-files"];
        } finally {
            propertyList.clear();
        }
    } finally {
        process.close();
    }
    return outputFilePaths;
}

function ibtoolVersion(ibtool) {
    var process;
    var version;
    try {
        process = new Process();
        if (process.exec(ibtool, ["--version", "--output-format", "xml1"], true) !== 0)
            print(process.readStdErr());

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
