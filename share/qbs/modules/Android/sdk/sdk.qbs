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

import qbs.Environment
import qbs.File
import qbs.FileInfo
import qbs.ModUtils
import qbs.Probes
import qbs.TextFile
import qbs.Utilities
import qbs.Xml
import "utils.js" as SdkUtils

Module {
    Probes.AndroidSdkProbe {
        id: sdkProbe
        environmentPaths: (sdkDir ? [sdkDir] : []).concat(base)
    }

    Probes.AndroidNdkProbe {
        id: ndkProbe
        sdkPath: sdkProbe.path
        environmentPaths: (ndkDir ? [ndkDir] : []).concat(base)
    }

    Probes.PathProbe {
        id: bundletoolProbe
        platformSearchPaths: [sdkDir]
        names: ["bundletool-all"]
        nameSuffixes: ["-0.11.0.jar", "-0.12.0.jar", "-0.13.0.jar", "-0.13.3.jar", "-0.13.4.jar",
            "-0.14.0.jar", "-0.15.0.jar", "-1.0.0.jar", "-1.1.0.jar", "-1.2.0.jar", "-1.3.0.jar",
            "-1.4.0.jar", "-1.5.0.jar", "-1.6.0.jar", "-1.6.1.jar", "-1.7.0.jar", "-1.7.1.jar",
            "-1.8.0.jar"]
    }

    property path sdkDir: sdkProbe.path
    property path ndkDir: ndkProbe.path
    property path ndkSamplesDir: ndkProbe.samplesDir
    property string buildToolsVersion: sdkProbe.buildToolsVersion
    property var buildToolsVersionParts: buildToolsVersion ? buildToolsVersion.split('.').map(function(item) { return parseInt(item, 10); }) : []
    property int buildToolsVersionMajor: buildToolsVersionParts[0]
    property int buildToolsVersionMinor: buildToolsVersionParts[1]
    property int buildToolsVersionPatch: buildToolsVersionParts[2]
    property string platform: sdkProbe.platform
    property string minimumVersion: "21"
    property string targetVersion: platformVersion.toString()

    property path bundletoolFilePath: bundletoolProbe.filePath

    // Product-specific properties and files
    property string packageName: {
        var idx = product.name.indexOf(".")
        if (idx > 0 && idx < product.name.length)
            return product.name;
        return "com.example." + product.name;
    }
    property int versionCode
    property string versionName
    property string apkBaseName: packageName
    property bool automaticSources: true
    property bool legacyLayout: false
    property path sourceSetDir: legacyLayout
                                ? product.sourceDirectory
                                : FileInfo.joinPaths(product.sourceDirectory, "src/main")
    property path resourcesDir: FileInfo.joinPaths(sourceSetDir, "res")
    property path assetsDir: FileInfo.joinPaths(sourceSetDir, "assets")
    property path sourcesDir: FileInfo.joinPaths(sourceSetDir, legacyLayout ? "src" : "java")
    property string manifestFile: defaultManifestFile
    readonly property string defaultManifestFile: FileInfo.joinPaths(sourceSetDir,
                                                                   "AndroidManifest.xml")

    property bool _enableRules: !product.multiplexConfigurationId && !!packageName

    property bool _bundledInAssets: true

    Group {
        name: "java sources"
        condition: product.Android.sdk.automaticSources
        prefix: FileInfo.resolvePath(product.sourceDirectory, product.Android.sdk.sourcesDir + '/')
        files: "**/*.java"
    }

    Group {
        name: "android resources"
        condition: product.Android.sdk.automaticSources
        fileTags: ["android.resources"]
        prefix: FileInfo.resolvePath(product.sourceDirectory, product.Android.sdk.resourcesDir + '/')
        files: "**/*"
    }

    Group {
        name: "android assets"
        condition: product.Android.sdk.automaticSources
        fileTags: ["android.assets"]
        prefix: FileInfo.resolvePath(product.sourceDirectory, product.Android.sdk.assetsDir + '/')
        files: "**/*"
    }

    Group {
        name: "manifest"
        condition: product.Android.sdk.automaticSources
        fileTags: ["android.manifest"]
        files: product.Android.sdk.manifestFile
               && product.Android.sdk.manifestFile !== product.Android.sdk.defaultManifestFile
               ? [product.Android.sdk.manifestFile]
               : (File.exists(product.Android.sdk.defaultManifestFile)
                  ? [product.Android.sdk.defaultManifestFile] : [])
    }


    // Internal properties.
    property int platformVersion: {
        if (platform) {
            var match = platform.match(/^android-([0-9]+)$/);
            if (match !== null) {
                return parseInt(match[1], 10);
            }
        }
    }

    property string platformJavaVersion: {
        if (platformVersion >= 21)
            return "1.7";
        return "1.5";
    }

    property path buildToolsDir: FileInfo.joinPaths(sdkDir, "build-tools", buildToolsVersion)
    property string aaptName: "aapt2"
    PropertyOptions {
        name: "aaptName"
        allowedValues: ["aapt", "aapt2"]
    }
    property path aaptFilePath: FileInfo.joinPaths(buildToolsDir, aaptName)
    readonly property bool _enableAapt2: aaptName === "aapt2"
    property string packageType: "apk"
    PropertyOptions {
        name: "packageType"
        allowedValues: ["apk", "aab"]
    }
    readonly property bool _generateAab: packageType == "aab"

    property path apksignerFilePath: FileInfo.joinPaths(buildToolsDir, "apksigner")
    property path aidlFilePath: FileInfo.joinPaths(buildToolsDir, "aidl")
    property path dxFilePath: FileInfo.joinPaths(buildToolsDir, "dx")
    property path d8FilePath: FileInfo.joinPaths(buildToolsDir, "d8")
    property string dexCompilerName: "d8"
    PropertyOptions {
        name: "dexCompilerName"
        allowedValues: ["dx", "d8"]
    }
    readonly property bool _useD8: dexCompilerName === "d8"
    property path zipalignFilePath: FileInfo.joinPaths(buildToolsDir, "zipalign")
    property path androidJarFilePath: FileInfo.joinPaths(sdkDir, "platforms", platform,
                                                         "android.jar")
    property path frameworkAidlFilePath: FileInfo.joinPaths(sdkDir, "platforms", platform,
                                                            "framework.aidl")
    property path generatedJavaFilesBaseDir: FileInfo.joinPaths(product.buildDirectory, "gen")
    property path generatedJavaFilesDir: FileInfo.joinPaths(generatedJavaFilesBaseDir,
                                         (packageName || "").split('.').join('/'))
    property path compiledResourcesDir: FileInfo.joinPaths(product.buildDirectory,
                                                           "compiled_resources")
    property string packageContentsDir: FileInfo.joinPaths(product.buildDirectory, packageType)
    property stringList aidlSearchPaths

    Depends { name: "java"; condition: _enableRules }
    Depends { name: "codesign"; condition: _enableRules }
    Properties {
        condition: _enableRules
        java.languageVersion: platformJavaVersion
        java.runtimeVersion: platformJavaVersion
        java.bootClassPaths: androidJarFilePath
        codesign.apksignerFilePath: apksignerFilePath
        codesign._packageName: apkBaseName + (_generateAab ? ".aab" : ".apk")
        codesign.useApksigner: !_generateAab
    }

    validate: {
        if (!sdkDir) {
            throw ModUtils.ModuleError("Could not find an Android SDK at any of the following "
                                       + "locations:\n\t" + sdkProbe.candidatePaths.join("\n\t")
                                       + "\nInstall the Android SDK to one of the above locations, "
                                       + "or set the Android.sdk.sdkDir property or "
                                       + "ANDROID_HOME environment variable to a valid "
                                       + "Android SDK location.");
        }
        if (!bundletoolFilePath && _generateAab) {
            throw ModUtils.ModuleError("Could not find Android bundletool at the following "
                                       + "location:\n\t" + Android.sdk.sdkDir
                                       + "\nInstall the Android bundletool to the above location, "
                                       + "or set the Android.sdk.bundletoolFilePath property.\n"
                                       + "Android bundletool can be downloaded from "
                                       + "https://github.com/google/bundletool");
        }
        if (Utilities.versionCompare(buildToolsVersion, "24.0.3") < 0) {
            throw ModUtils.ModuleError("Version of Android SDK build tools too old. This version "
                                       + "is " + buildToolsVersion + " and minimum version is "
                                       + "24.0.3. Please update the Android SDK.")
        }
    }

    FileTagger {
        patterns: ["AndroidManifest.xml"]
        fileTags: ["android.manifest"]
    }

    FileTagger {
        patterns: ["*.aidl"]
        fileTags: ["android.aidl"]
    }

    Parameter {
        property bool embedJar: true
    }

    Rule {
        condition: _enableRules
        inputs: ["android.aidl"]
        Artifact {
            filePath: FileInfo.joinPaths(Utilities.getHash(input.filePath),
                                         input.completeBaseName + ".java")
            fileTags: ["java.java"]
        }

        prepare: {
            var aidl = product.Android.sdk.aidlFilePath;
            var args = ["-p" + product.Android.sdk.frameworkAidlFilePath];
            var aidlSearchPaths = input.Android.sdk.aidlSearchPaths;
            for (var i = 0; i < (aidlSearchPaths ? aidlSearchPaths.length : 0); ++i)
                args.push("-I" + aidlSearchPaths[i]);
            args.push(input.filePath, output.filePath);
            var cmd = new Command(aidl, args);
            cmd.description = "processing " + input.fileName;
            return [cmd];
        }
    }

    property bool customManifestProcessing: false
    Group {
        condition: !product.Android.sdk.customManifestProcessing
        fileTagsFilter: "android.manifest_processed"
        fileTags: "android.manifest_final"
    }
    Rule {
        condition: _enableRules
        inputs: "android.manifest"
        Artifact {
            filePath: FileInfo.joinPaths("processed_manifest", input.fileName)
            fileTags: "android.manifest_processed"
        }
        prepare: {
            var cmd = new JavaScriptCommand();
            cmd.description = "ensuring correct package name in Android manifest file";
            cmd.sourceCode = function() {
                var manifestData = new Xml.DomDocument();
                manifestData.load(input.filePath);
                var rootElem = manifestData.documentElement();
                if (!rootElem || !rootElem.isElement() || rootElem.tagName() != "manifest")
                    throw "No manifest tag found in '" + input.filePath + "'.";

                // Quick sanity check. Don't try to be fancy; let's not risk rejecting valid names.
                var packageName = product.Android.sdk.packageName;
                if (!packageName.match(/^[^.]+(?:\.[^.]+)+$/)) {
                    throw "Package name '" + packageName + "' is not valid. Please set "
                            + "Android.sdk.packageName to a name following the "
                            + "'com.mycompany.myproduct' pattern."
                }
                rootElem.setAttribute("package", packageName);
                if (product.Android.sdk.versionCode !== undefined)
                    rootElem.setAttribute("android:versionCode", product.Android.sdk.versionCode);
                if (product.Android.sdk.versionName !== undefined)
                    rootElem.setAttribute("android:versionName", product.Android.sdk.versionName);

                if (product.Android.sdk._bundledInAssets) {
                    // Remove <meta-data android:name="android.app.bundled_in_assets_resource_id"
                    // android:resource="@array/bundled_in_assets"/>
                    // custom AndroidManifest.xml because assets are in rcc files for qt >= 5.14
                    var appElem = rootElem.firstChild("application");
                    if (!appElem || !appElem.isElement() || appElem.tagName() != "application")
                         throw "No application tag found in '" + input.filePath + "'.";
                    var activityElem = appElem.firstChild("activity");
                    if (!activityElem || !activityElem.isElement() ||
                        activityElem.tagName() != "activity")
                         throw "No activity tag found in '" + input.filePath + "'.";
                    var metaDataElem = activityElem.firstChild("meta-data");
                    while (metaDataElem && metaDataElem.isElement()) {
                        if (SdkUtils.elementHasBundledAttributes(metaDataElem)) {
                            var elemToRemove = metaDataElem;
                            metaDataElem = metaDataElem.nextSibling("meta-data");
                            activityElem.removeChild(elemToRemove);
                        } else {
                            metaDataElem = metaDataElem.nextSibling("meta-data");
                        }
                    }
                }

                var usedSdkElem = rootElem.firstChild("uses-sdk");
                if (!usedSdkElem || !usedSdkElem.isElement()) {
                    usedSdkElem = manifestData.createElement("uses-sdk");
                    rootElem.appendChild(usedSdkElem);
                } else {
                    if (!usedSdkElem.isElement())
                        throw "Tag uses-sdk is not an element in '" + input.filePath + "'.";
                }
                usedSdkElem.setAttribute("android:minSdkVersion",
                                         product.Android.sdk.minimumVersion);
                usedSdkElem.setAttribute("android:targetSdkVersion",
                                         product.Android.sdk.targetVersion);

                rootElem.appendChild(usedSdkElem);
                manifestData.save(output.filePath, 4);
            }
            return cmd;
        }
    }

    Rule {
        condition: _enableRules && !_enableAapt2
        multiplex: true
        inputs: ["android.resources", "android.assets", "android.manifest_final"]

        outputFileTags: ["java.java"]
        outputArtifacts: {
            var artifacts = [];
            var resources = inputs["android.resources"];
            if (resources && resources.length) {
                artifacts.push({
                    filePath: FileInfo.joinPaths(product.Android.sdk.generatedJavaFilesDir,
                                                 "R.java"),
                    fileTags: ["java.java"]
                });
            }

            return artifacts;
        }

        prepare: SdkUtils.prepareAaptGenerate.apply(SdkUtils, arguments)
    }

    Rule {
        condition: _enableRules && _enableAapt2
        inputs: ["android.resources"]
        outputArtifacts: {
            var outputs = [];
            var resources = inputs["android.resources"];
            for (var i = 0; i < resources.length; ++i) {
                var filePath = resources[i].filePath;
                var resourceFileName = SdkUtils.generateAapt2ResourceFileName(filePath);
                var compiledName = FileInfo.joinPaths(product.Android.sdk.compiledResourcesDir,
                                                      resourceFileName);
                outputs.push({filePath: compiledName, fileTags: "android.resources_compiled"});
            }
            return outputs;
        }
        outputFileTags: ["android.resources_compiled"]

        prepare: SdkUtils.prepareAapt2CompileResource.apply(SdkUtils, arguments)
    }

    Rule {
        condition: _enableRules && _enableAapt2
        multiplex: true
        inputs: ["android.resources_compiled", "android.assets", "android.manifest_final"]
        outputFileTags: ["java.java", "android.apk_resources"]
        outputArtifacts: {
            var artifacts = [];
            artifacts.push({
                filePath: product.Android.sdk.apkBaseName + (product.Android.sdk._generateAab ?
                              ".apk_aab" : ".apk_apk"),
                fileTags: ["android.apk_resources"]
            });
            var resources = inputs["android.resources_compiled"];
            if (resources && resources.length) {
                artifacts.push({
                    filePath: FileInfo.joinPaths(product.Android.sdk.generatedJavaFilesDir,
                                                 "R.java"),
                    fileTags: ["java.java"]
                });
            }

            return artifacts;
        }
        prepare: SdkUtils.prepareAapt2Link.apply(SdkUtils, arguments)
    }

    Rule {
        condition: _enableRules
        multiplex: true

        Artifact {
            filePath: FileInfo.joinPaths(product.Android.sdk.generatedJavaFilesDir,
                                         "BuildConfig.java")
            fileTags: ["java.java"]
        }

        prepare: {
            var cmd = new JavaScriptCommand();
            cmd.description = "generating BuildConfig.java";
            cmd.sourceCode = function() {
                var debugValue = product.qbs.buildVariant === "debug" ? "true" : "false";
                var ofile = new TextFile(output.filePath, TextFile.WriteOnly);
                ofile.writeLine("package " + product.Android.sdk.packageName +  ";")
                ofile.writeLine("public final class BuildConfig {");
                ofile.writeLine("    public final static boolean DEBUG = " + debugValue + ";");
                ofile.writeLine("}");
                ofile.close();
            };
            return [cmd];
        }
    }

    Rule {
        condition: _enableRules
        multiplex: true
        inputs: ["java.class"]
        inputsFromDependencies: ["java.jar", "bundled_jar"]
        Artifact {
            filePath: product.Android.sdk._generateAab ?
                          FileInfo.joinPaths(product.Android.sdk.packageContentsDir, "dex",
                                             "classes.dex") :
                          FileInfo.joinPaths(product.Android.sdk.packageContentsDir, "classes.dex")
            fileTags: ["android.dex"]
        }
        prepare: SdkUtils.prepareDex.apply(SdkUtils, arguments)
    }

    Rule {
        condition: _enableRules
        property stringList inputTags: "android.nativelibrary"
        inputsFromDependencies: inputTags
        inputs: product.aggregate ? [] : inputTags
        Artifact {
            filePath: FileInfo.joinPaths(product.Android.sdk.packageContentsDir, "lib",
                                         input.Android.ndk.abi, product.Android.sdk._archInName ?
                                             input.baseName + "_" + input.Android.ndk.abi + ".so" :
                                             input.fileName)
            fileTags: "android.nativelibrary_deployed"
        }
        prepare: {
            var cmd = new JavaScriptCommand();
            cmd.description = "copying " + input.fileName + " for packaging";
            cmd.sourceCode = function() { File.copy(input.filePath, output.filePath); };
            return cmd;
        }
    }

    Rule {
        condition: _enableRules
        multiplex: true
        property stringList inputTags: "android.stl"
        inputsFromDependencies: inputTags
        inputs: product.aggregate ? [] : inputTags
        outputFileTags: "android.stl_deployed"
        outputArtifacts: {
            var deploymentData = SdkUtils.stlDeploymentData(product, inputs, "stl");
            var outputs = [];
            for (var i = 0; i < deploymentData.outputFilePaths.length; ++i) {
                outputs.push({filePath: deploymentData.outputFilePaths[i],
                              fileTags: "android.stl_deployed"});
            }
            return outputs;
        }
        prepare: {
            var cmds = [];
            var deploymentData = SdkUtils.stlDeploymentData(product, inputs);
            for (var i = 0; i < deploymentData.uniqueInputs.length; ++i) {
                var input = deploymentData.uniqueInputs[i];
                var stripArgs = ["--strip-all", "-o", deploymentData.outputFilePaths[i],
                                 input.filePath];
                var cmd = new Command(input.cpp.stripPath, stripArgs);
                cmd.description = "deploying " + input.fileName;
                cmds.push(cmd);
            }
            return cmds;
        }
    }

    Rule {
        condition: _enableRules && !_enableAapt2 && !_generateAab
        multiplex: true
        inputs: [
            "android.resources", "android.assets", "android.manifest_final",
            "android.dex", "android.stl_deployed",
            "android.nativelibrary_deployed"
        ]
        Artifact {
            filePath: product.Android.sdk.apkBaseName + ".apk_unsigned"
            fileTags: "android.package_unsigned"
        }
        prepare: SdkUtils.prepareAaptPackage.apply(SdkUtils, arguments)
    }

    Rule {
        condition: _enableRules && _enableAapt2 && !_generateAab
        multiplex: true
        inputs: [
            "android.apk_resources", "android.manifest_final",
            "android.dex", "android.stl_deployed",
            "android.nativelibrary_deployed"
        ]
        Artifact {
            filePath: product.Android.sdk.apkBaseName + ".apk_unsigned"
            fileTags: "android.package_unsigned"
        }
        prepare: SdkUtils.prepareApkPackage.apply(SdkUtils, arguments)
    }

    Rule {
        condition: _enableRules && _enableAapt2 && _generateAab
        multiplex: true
        inputs: [
            "android.apk_resources", "android.manifest_final",
            "android.dex", "android.stl_deployed",
            "android.nativelibrary_deployed"
        ]
        Artifact {
            filePath: product.Android.sdk.apkBaseName + ".aab_unsigned"
            fileTags: "android.package_unsigned"
        }
        prepare: SdkUtils.prepareBundletoolPackage.apply(SdkUtils, arguments)
    }
}
