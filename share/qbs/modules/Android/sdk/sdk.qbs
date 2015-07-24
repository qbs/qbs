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

import qbs
import qbs.File
import qbs.FileInfo
import qbs.ModUtils
import qbs.Probes
import qbs.TextFile
import "utils.js" as SdkUtils

Module {
    Probes.AndroidSdkProbe {
        id: sdkProbe
        environmentPaths: [sdkDir].concat(base)
    }

    Probes.AndroidNdkProbe {
        id: ndkProbe
        environmentPaths: [ndkDir].concat(base)
    }

    property path sdkDir: sdkProbe.path
    property path ndkDir: ndkProbe.path
    property string buildToolsVersion: sdkProbe.buildToolsVersion
    property var buildToolsVersionParts: buildToolsVersion ? buildToolsVersion.split('.').map(function(item) { return parseInt(item, 10); }) : []
    property int buildToolsVersionMajor: buildToolsVersionParts[0]
    property int buildToolsVersionMinor: buildToolsVersionParts[1]
    property int buildToolsVersionPatch: buildToolsVersionParts[2]
    property string platform: sdkProbe.platform

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

    property path buildToolsDir: {
        var path = FileInfo.joinPaths(sdkDir, "build-tools", buildToolsVersion);
        if (buildToolsVersionMajor >= 23)
            return FileInfo.joinPaths(path, "bin");
        return path;
    }

    property path aaptFilePath: FileInfo.joinPaths(buildToolsDir, "aapt")
    property path aidlFilePath: FileInfo.joinPaths(buildToolsDir, "aidl")
    property path dxFilePath: FileInfo.joinPaths(buildToolsDir, "dx")
    property path zipalignFilePath: FileInfo.joinPaths(buildToolsDir, "zipalign")
    property path androidJarFilePath: FileInfo.joinPaths(sdkDir, "platforms", platform,
                                                         "android.jar")
    property path generatedJavaFilesBaseDir: FileInfo.joinPaths(product.buildDirectory, "gen")
    property path generatedJavaFilesDir: FileInfo.joinPaths(generatedJavaFilesBaseDir,
                                         product.packageName.split('.').join('/'))

    Depends { name: "java" }
    java.languageVersion: platformJavaVersion
    java.runtimeVersion: platformJavaVersion
    java.bootClassPaths: androidJarFilePath

    // QBS-833 workaround
    Probes.JdkProbe { id: jdk; environmentPaths: [java.jdkPath].concat(base) }
    java.jdkPath: jdk.path
    java.compilerVersion: jdk.version ? jdk.version[1] : undefined

    FileTagger {
        patterns: ["AndroidManifest.xml"]
        fileTags: ["android.manifest"]
    }

    FileTagger {
        patterns: ["*.aidl"]
        fileTags: ["android.aidl"]
    }


    Rule {
        inputs: ["android.aidl"]
        Artifact {
            filePath: FileInfo.joinPaths(qbs.getHash(input.filePath),
                                         input.completeBaseName + ".java")
            fileTags: ["java.java"]
        }

        prepare: {
            var aidl = ModUtils.moduleProperty(product, "aidlFilePath");
            cmd = new Command(aidl, [input.filePath, output.filePath]);
            cmd.description = "Processing " + input.fileName;
            return [cmd];
        }
    }

    Rule {
        multiplex: true
        inputs: ["android.resources", "android.assets", "android.manifest"]

        outputFileTags: ["android.ap_", "java.java"]
        outputArtifacts: {
            var artifacts = [{
                filePath: product.name + ".ap_",
                fileTags: ["android.ap_"]
            }];

            var resources = inputs["android.resources"];
            if (resources && resources.length) {
                artifacts.push({
                    filePath: FileInfo.joinPaths(
                                  ModUtils.moduleProperty(product, "generatedJavaFilesDir"),
                                  "R.java"),
                    fileTags: ["java.java"]
                });
            }

            return artifacts;
        }

        prepare: {
            var manifestFilePath = inputs["android.manifest"][0].filePath;
            var args = ["package", "-f", "-m", "--no-crunch",
                        "-M", manifestFilePath,
                        "-I", ModUtils.moduleProperty(product, "androidJarFilePath"),
                        "-F", outputs["android.ap_"][0].filePath, "--generate-dependencies"];
            var resources = inputs["android.resources"];
            if (resources && resources.length)
                args.push("-S", product.resourcesDir,
                          "-J", ModUtils.moduleProperty(product, "generatedJavaFilesBaseDir"));
            if (product.moduleProperty("qbs", "buildVariant") === "debug")
                args.push("--debug-mode");
            if (File.exists(product.assetsDir))
                args.push("-A", product.assetsDir);
            var cmd = new Command(ModUtils.moduleProperty(product, "aaptFilePath"), args);
            cmd.description = "Processing resources";
            return [cmd];
        }
    }

    Rule {
        inputs: ["android.manifest"] // FIXME: Workaround for the fact that rules need inputs
        Artifact {
            filePath: FileInfo.joinPaths(ModUtils.moduleProperty(product, "generatedJavaFilesDir"),
                                         "BuildConfig.java")
            fileTags: ["java.java"]
        }

        prepare: {
            var cmd = new JavaScriptCommand();
            cmd.description = "Generating BuildConfig.java";
            cmd.sourceCode = function() {
                var debugValue = product.moduleProperty("qbs", "buildVariant") === "debug"
                        ? "true" : "false";
                var ofile = new TextFile(output.filePath, TextFile.WriteOnly);
                ofile.writeLine("package " + product.packageName +  ";")
                ofile.writeLine("public final class BuildConfig {");
                ofile.writeLine("    public final static boolean DEBUG = " + debugValue + ";");
                ofile.writeLine("}");
                ofile.close();
            };
            return [cmd];
        }
    }

    Rule {
        multiplex: true
        inputs: ["java.class"]
        Artifact {
            filePath: "classes.dex"
            fileTags: ["android.dex"]
        }
        prepare: {
            var dxFilePath = ModUtils.moduleProperty(product, "dxFilePath");
            var args = ["--dex", "--output", output.filePath,
                        product.moduleProperty("java", "classFilesDir")];
            var cmd = new Command(dxFilePath, args);
            cmd.description = "Creating " + output.fileName;
            return [cmd];
        }
    }

    Rule {
        multiplex: true
        inputsFromDependencies: [
            "android.gdbserver-info", "android.stl-info", "android.nativelibrary"
        ]
        outputFileTags: ["android.gdbserver", "android.stl", "android.nativelibrary-deployed"]
        outputArtifacts: {
            var libArtifacts = [];
            if (inputs["android.nativelibrary"]) {
                for (var i = 0; i < inputs["android.nativelibrary"].length; ++i) {
                    var inp = inputs["android.nativelibrary"][i];
                    var destDir = FileInfo.joinPaths("lib",
                                                     inp.moduleProperty("Android.ndk", "abi"));
                    libArtifacts.push({
                            filePath: FileInfo.joinPaths(destDir, inp.fileName),
                            fileTags: ["android.nativelibrary-deployed"]
                    });
                }
            }
            var gdbServerArtifacts = SdkUtils.outputArtifactsFromInfoFiles(inputs,
                    product, "android.gdbserver-info", "android.gdbserver");
            var stlArtifacts = SdkUtils.outputArtifactsFromInfoFiles(inputs, product,
                    "android.stl-info", "android.deployed-stl");
            return libArtifacts.concat(gdbServerArtifacts).concat(stlArtifacts);
        }
        prepare: {
            var cmd = new JavaScriptCommand();
            cmd.description = "Pre-packaging native binaries";
            cmd.sourceCode = function() {
                if (inputs["android.nativelibrary"]) {
                    for (var i = 0; i < inputs["android.nativelibrary"].length; ++i) {
                        for (var j = 0; j < outputs["android.nativelibrary-deployed"].length; ++j) {
                            var inp = inputs["android.nativelibrary"][i];
                            var outp = outputs["android.nativelibrary-deployed"][j];
                            var inpAbi = inp.moduleProperty("Android.ndk", "abi");
                            var outpAbi = FileInfo.fileName(outp.baseDir);
                            if (inp.fileName === outp.fileName && inpAbi === outpAbi) {
                                File.copy(inp.filePath, outp.filePath);
                                break;
                            }
                        }
                    }
                }
                var pathsSpecs = SdkUtils.sourceAndTargetFilePathsFromInfoFiles(inputs, product,
                        "android.gdbserver-info");
                for (i = 0; i < pathsSpecs.sourcePaths.length; ++i)
                    File.copy(pathsSpecs.sourcePaths[i], pathsSpecs.targetPaths[i]);
                pathsSpecs = SdkUtils.sourceAndTargetFilePathsFromInfoFiles(inputs, product,
                        "android.stl-info");
                for (i = 0; i < pathsSpecs.sourcePaths.length; ++i)
                    File.copy(pathsSpecs.sourcePaths[i], pathsSpecs.targetPaths[i]);
            };
            return [cmd];
        }
    }

    // TODO: ApkBuilderMain is deprecated. Do we have to provide our own tool directly
    //       accessing com.android.sdklib.build.ApkBuilder or is there a simpler way?
    Rule {
        multiplex: true
        inputs: [
            "android.dex", "android.ap_", "android.gdbserver", "android.stl",
            "android.nativelibrary-deployed"
        ]
        Artifact {
            filePath: product.name + ".apk.unaligned"
            fileTags: ["android.apk.unaligned"]
        }
        prepare: {
            var args = ["-classpath", FileInfo.joinPaths(ModUtils.moduleProperty(product, "sdkDir"),
                                                         "tools/lib/sdklib.jar"),
                        "com.android.sdklib.build.ApkBuilderMain", output.filePath,
                        "-z", inputs["android.ap_"][0].filePath,
                        "-f", inputs["android.dex"][0].filePath];
            if (product.moduleProperty("qbs", "buildVariant") === "debug")
                args.push("-d");
            if (inputs["android.nativelibrary-deployed"])
                args.push("-nf", FileInfo.joinPaths(product.buildDirectory, "lib"));
            var cmd = new Command(product.moduleProperty("java", "interpreterFilePath"), args);
            cmd.description = "Generating " + output.fileName;
            return [cmd];
        }
    }

    Rule {
        multiplex: true
        inputs: ["android.apk.unaligned"]
        Artifact {
            filePath: product.name + ".apk"
            fileTags: ["android.apk"]
        }
        prepare: {
            var zipalign = ModUtils.moduleProperty(product, "zipalignFilePath");
            var args = ["-f", "4", inputs["android.apk.unaligned"][0].filePath, output.filePath];
            var cmd = new Command(zipalign, args);
            cmd.description = "Creating " + output.fileName;
            return [cmd];
        }
    }
}
