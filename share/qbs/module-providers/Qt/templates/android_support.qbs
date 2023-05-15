import qbs.File
import qbs.FileInfo
import qbs.ModUtils
import qbs.TextFile
import qbs.Utilities
import qbs.Process
import qbs.Xml

Module {
    version: @version@
    property string qmlRootDir: product.sourceDirectory
    property stringList extraPrefixDirs
    property stringList qmlImportPaths
    property stringList deploymentDependencies // qmake: ANDROID_DEPLOYMENT_DEPENDENCIES
    property stringList extraPlugins // qmake: ANDROID_EXTRA_PLUGINS
    property stringList extraLibs // qmake: ANDROID_EXTRA_LIBS
    property bool verboseAndroidDeployQt: false
    property string _androidDeployQtFilePath: FileInfo.joinPaths(_qtBinaryDir, "bin",
                                                                 "androiddeployqt")
    property string rccFilePath
    property string _qtBinaryDir
    property string _qtInstallDir
    property bool _enableSdkSupport: product.type && product.type.contains("android.package")
                                     && !product.consoleApplication
    property bool _enableNdkSupport: !product.aggregate || product.multiplexConfigurationId
    property string _templatesBaseDir: FileInfo.joinPaths(_qtInstallDir, "src", "android")
    property string _deployQtOutDir: FileInfo.joinPaths(product.buildDirectory, "deployqt_out")

    property bool _multiAbi: Utilities.versionCompare(version, "5.14") >= 0

    // QTBUG-87288: correct QtNetwork jar dependencies for 5.15.0 < Qt < 5.15.3
    property bool _correctQtNetworkDependencies: Utilities.versionCompare(version, "5.15.0") > 0 &&
                                                 Utilities.versionCompare(version, "5.15.3") < 0

    Depends { name: "Android.sdk"; condition: _enableSdkSupport }
    Depends { name: "Android.ndk"; condition: _enableNdkSupport }
    Depends { name: "java"; condition: _enableSdkSupport }
    Depends { name: "cpp"; condition: _enableNdkSupport }

    Properties {
        condition: _enableNdkSupport && qbs.toolchain.contains("clang")
        Android.ndk.appStl: "c++_shared"
    }
    Properties {
        condition: _enableNdkSupport && !qbs.toolchain.contains("clang")
        Android.ndk.appStl: "gnustl_shared"
    }
    Properties {
        condition: _enableSdkSupport
        Android.sdk.customManifestProcessing: true
        java._tagJniHeaders: false // prevent rule cycle
    }
    readonly property string _qtAndroidJarFileName: Utilities.versionCompare(version, "6.0") >= 0 ?
                                                       "Qt6Android.jar" : "QtAndroid.jar"
    Properties {
        condition: _enableSdkSupport && Utilities.versionCompare(version, "5.15") >= 0
                   && Utilities.versionCompare(version, "6.0") < 0
        java.additionalClassPaths: [FileInfo.joinPaths(_qtInstallDir, "jar", "QtAndroid.jar")]
    }
    Properties {
        condition: _enableSdkSupport && Utilities.versionCompare(version, "6.0") >= 0
        java.additionalClassPaths: [FileInfo.joinPaths(_qtInstallDir, "jar", "Qt6Android.jar")]
    }
    // "ANDROID_HAS_WSTRING" was removed from qtcore qstring.h in Qt 5.14.0
    Properties {
        condition: _enableNdkSupport && Utilities.versionCompare(version, "5.14.0") < 0 &&
                   (Android.ndk.abi === "armeabi-v7a" || Android.ndk.abi === "x86")
        cpp.defines: "ANDROID_HAS_WSTRING"
    }
    Properties {
        condition: _enableSdkSupport
        Android.sdk._bundledInAssets: _multiAbi
    }
    Properties {
        condition: _enableSdkSupport && Utilities.versionCompare(version, "6.0") < 0
        Android.sdk.minimumVersion: "21"
    }
    Properties {
        condition: _enableSdkSupport && Utilities.versionCompare(version, "6.0") >= 0
        Android.sdk.minimumVersion: "23"
    }

    Properties {
        condition: _enableNdkSupport
        cpp.archSuffix: _multiAbi ? "_" + Android.ndk.abi : ""
    }

    Rule {
        condition: _enableSdkSupport
        multiplex: true
        property stringList inputTags: ["android.nativelibrary", "qrc"]
        inputsFromDependencies: inputTags
        inputs: product.aggregate ? [] : inputTags
        Artifact {
            filePath: "androiddeployqt.json"
            fileTags: "qt_androiddeployqt_input"
        }
        prepare: {
            var cmd = new JavaScriptCommand();
            cmd.description = "creating " + output.fileName;
            cmd.sourceCode = function() {
                var theBinary;
                var nativeLibs = inputs["android.nativelibrary"];
                var architectures = [];
                var triples = [];
                var qtInstallDirectories = [];
                var hostArch;
                var targetArchitecture;
                if (nativeLibs.length === 1) {
                    theBinary = nativeLibs[0];
                    hostArch = theBinary.Android.ndk.hostArch;
                    targetArchitecture = theBinary.Android.ndk.abi;
                    if (product.Qt.android_support._multiAbi) {
                        architectures.push(theBinary.Android.ndk.abi);
                        triples.push(theBinary.cpp.toolchainTriple);
                        qtInstallDirectories.push(theBinary.Qt.android_support._qtInstallDir);
                    }
                } else {
                    for (i = 0; i < nativeLibs.length; ++i) {
                        var candidate = nativeLibs[i];
                        if (product.Qt.android_support._multiAbi) {
                           if (candidate.product.name === product.name) {
                               architectures.push(candidate.Android.ndk.abi);
                               triples.push(candidate.cpp.toolchainTriple);
                               qtInstallDirectories.push(
                                           candidate.Qt.android_support._qtInstallDir);
                               hostArch = candidate.Android.ndk.hostArch;
                               targetArchitecture = candidate.Android.ndk.abi;
                               theBinary = candidate;
                           }
                        } else {
                            if (!candidate.fileName.contains(candidate.product.targetName))
                                continue;
                            if (!theBinary) {
                                theBinary = candidate;
                                hostArch = theBinary.Android.ndk.hostArch;
                                targetArchitecture = theBinary.Android.ndk.abi;
                                continue;
                            }
                            if (candidate.product.name !== product.name) {
                                continue; // This is not going to be a match
                            }
                            if (candidate.product.name === product.name
                                    && theBinary.product.name !== product.name) {
                                theBinary = candidate; // The new candidate is a better match.
                                hostArch = theBinary.Android.ndk.hostArch;
                                targetArchitecture = theBinary.Android.ndk.abi;
                                continue;
                            }

                            throw "Qt applications for Android support only one native binary "
                                    + "per package.\n"
                                    + "In particular, you cannot build a Qt app for more than "
                                    + "one architecture at the same time.";
                        }
                    }
                }
                var f = new TextFile(output.filePath, TextFile.WriteOnly);
                f.writeLine("{");
                f.writeLine('"description": "This file was generated by qbs to be read by '
                            + 'androiddeployqt and should not be modified by hand.",');
                if (Utilities.versionCompare(product.Qt.core.version, "6.3.0") >= 0) {
                    var line = '"qt": {';
                    for (var i = 0; i < qtInstallDirectories.length && i < architectures.length;
                         ++i) {
                        line = line + '"' + architectures[i] + '":"' +
                                qtInstallDirectories[i] + '"';
                        if (i < qtInstallDirectories.length-1 || i < architectures.length-1)
                            line = line + ',';
                    }
                    line = line + "},";
                    f.writeLine(line);
                } else {
                    f.writeLine('"qt": "' + product.Qt.android_support._qtInstallDir + '",');
                }
                f.writeLine('"sdk": "' + product.Android.sdk.sdkDir + '",');
                f.writeLine('"sdkBuildToolsRevision": "' + product.Android.sdk.buildToolsVersion
                            + '",');
                f.writeLine('"ndk": "' + product.Android.sdk.ndkDir + '",');
                f.writeLine('"toolchain-prefix": "llvm",');
                f.writeLine('"tool-prefix": "llvm",');
                f.writeLine('"useLLVM": true,');
                f.writeLine('"ndk-host": "' + hostArch + '",');
                if (!product.Qt.android_support._multiAbi) {
                    f.writeLine('"target-architecture": "' + targetArchitecture + '",');
                }
                else {
                    var line = '"architectures": {';
                    for (var i in architectures) {
                        line = line + '"' + architectures[i] + '":"' + triples[i] + '"';
                        if (i < architectures.length-1)
                            line = line + ',';
                    }
                    line = line + "},";
                    f.writeLine(line);
                }
                f.writeLine('"qml-root-path": "' + product.Qt.android_support.qmlRootDir + '",');
                var deploymentDeps = product.Qt.android_support.deploymentDependencies;
                if (deploymentDeps && deploymentDeps.length > 0)
                    f.writeLine('"deployment-dependencies": "' + deploymentDeps.join() + '",');
                var extraPlugins = product.Qt.android_support.extraPlugins;
                if (extraPlugins && extraPlugins.length > 0)
                    f.writeLine('"android-extra-plugins": "' + extraPlugins.join() + '",');
                var extraLibs = product.Qt.android_support.extraLibs;
                if (extraLibs && extraLibs.length > 0) {
                    for (var i = 0; i < extraLibs.length; ++i) {
                        if (!FileInfo.isAbsolutePath(extraLibs[i])) {
                            extraLibs[i] = FileInfo.joinPaths(product.sourceDirectory, extraLibs[i]);
                        }
                    }
                    f.writeLine('"android-extra-libs": "' + extraLibs.join() + '",');
                }
                var prefixDirs = product.Qt.android_support.extraPrefixDirs;
                if (prefixDirs && prefixDirs.length > 0)
                    f.writeLine('"extraPrefixDirs": ' + JSON.stringify(prefixDirs) + ',');
                var qmlImportPaths = product.Qt.android_support.qmlImportPaths;
                if (qmlImportPaths && qmlImportPaths.length > 0)
                    f.writeLine('"qml-import-paths": "' + qmlImportPaths.join(',') + '",');
                else if ((product.qmlImportPaths instanceof Array) && product.qmlImportPaths.length > 0)
                    f.writeLine('"qml-import-paths": "' + product.qmlImportPaths.join(',') + '",');

                if (Utilities.versionCompare(product.Qt.android_support.version, "6.0") >= 0) {
                    f.writeLine('"qml-importscanner-binary": "'
                                + product.Qt.core.qmlImportScannerFilePath + FileInfo.executableSuffix()
                                + '",');
                    f.writeLine('"rcc-binary": "' + product.Qt.android_support.rccFilePath
                                + FileInfo.executableSuffix() + '",');

                    if (inputs["qrc"] && inputs["qrc"].length > 0) {
                        var qrcFiles = [];
                        var qrcInputs = inputs["qrc"];
                        for (i = 0; i < qrcInputs.length; ++i) {
                            qrcFiles.push(qrcInputs[i].filePath);
                        }
                        f.writeLine('"qrcFiles": "' + qrcFiles.join(',') + '",');
                    }
                }

                // QBS-1429
                if (!product.Qt.android_support._multiAbi) {
                    f.writeLine('"stdcpp-path": "' + (product.cpp.sharedStlFilePath
                            ? product.cpp.sharedStlFilePath : product.cpp.staticStlFilePath)
                            + '",');
                    f.writeLine('"application-binary": "' + theBinary.filePath + '"');
                } else {
                    f.writeLine('"stdcpp-path": "' + product.Android.sdk.ndkDir +
                                '/toolchains/llvm/prebuilt/' + hostArch + '/sysroot/usr/lib/",');
                    f.writeLine('"application-binary": "' + theBinary.product.targetName + '"');
                }

                f.writeLine("}");
                f.close();
            };
            return cmd;
        }
    }

    // We use the manifest template from the Qt installation if and only if the project
    // does not provide a manifest file.
    Rule {
        condition: _enableSdkSupport
        multiplex: true
        requiresInputs: false
        inputs: "android.manifest"
        excludedInputs: "qt.android_manifest"
        outputFileTags: ["android.manifest", "qt.android_manifest"]
        outputArtifacts: {
            if (inputs["android.manifest"])
                return [];
            return [{
                filePath: "qt_manifest/AndroidManifest.xml",
                fileTags: ["android.manifest", "qt.android_manifest"]
            }];
        }
        prepare: {
            var cmd = new JavaScriptCommand();
            cmd.description = "copying Qt Android manifest template";
            cmd.sourceCode = function() {
                File.copy(FileInfo.joinPaths(product.Qt.android_support._templatesBaseDir,
                          "templates", "AndroidManifest.xml"), output.filePath);
            };
            return cmd;
        }
    }

    Rule {
        condition: _enableSdkSupport
        multiplex: true
        property stringList defaultInputs: ["qt_androiddeployqt_input",
                                            "android.manifest_processed"]
        property stringList allInputs: ["qt_androiddeployqt_input", "android.manifest_processed",
                                        "android.nativelibrary"]
        inputsFromDependencies: "android.nativelibrary"
        inputs: product.aggregate ? defaultInputs : allInputs
        outputFileTags: [
            "android.manifest_final", "android.resources", "android.assets", "bundled_jar",
            "android.deployqt_list",
        ]
        outputArtifacts: [
            {
                filePath: "AndroidManifest.xml",
                fileTags: "android.manifest_final"
            },
            {
                filePath: product.Qt.android_support._deployQtOutDir + "/res/values/libs.xml",
                fileTags: "android.resources"
            },
            {
                filePath: product.Qt.android_support._deployQtOutDir
                          + "/res/values/strings.xml",
                fileTags: "android.resources"
            },
            {
                filePath: product.Qt.android_support._deployQtOutDir + "/assets/.dummy",
                fileTags: "android.assets"
            },
            {
                filePath: "deployqt.list",
                fileTags: "android.deployqt_list"
            },
            // androiddeployqt potentially copies more jar files but this one will always be there
            // since it comes with Qt.core
            {
                filePath: FileInfo.joinPaths(product.java.classFilesDir,
                                             product.Qt.android_support._qtAndroidJarFileName),
                fileTags: "bundled_jar"
            }
        ]
        prepare: {
            var cmds = [];
            var copyCmd = new JavaScriptCommand();
            copyCmd.description = "copying Qt resource templates";
            copyCmd.sourceCode = function() {
                File.copy(inputs["android.manifest_processed"][0].filePath,
                          product.Qt.android_support._deployQtOutDir + "/AndroidManifest.xml");
                File.copy(product.Qt.android_support._templatesBaseDir + "/java/res",
                          product.Qt.android_support._deployQtOutDir + "/res");
                File.copy(product.Qt.android_support._templatesBaseDir
                          + "/templates/res/values/libs.xml",
                          product.Qt.android_support._deployQtOutDir + "/res/values/libs.xml");
                if (!product.Qt.android_support._multiAbi) {
                    try {
                        File.remove(FileInfo.path(outputs["android.assets"][0].filePath));
                    } catch (e) {
                    }
                }
                else {
                    for (var i in inputs["android.nativelibrary"]) {
                        var input = inputs["android.nativelibrary"][i];
                        File.copy(input.filePath,
                                  FileInfo.joinPaths(product.Qt.android_support._deployQtOutDir,
                                                     "libs", input.Android.ndk.abi,
                                                     input.fileName));
                    }
                }
            };
            cmds.push(copyCmd);

            var androidDeployQtArgs = [
                "--output", product.Qt.android_support._deployQtOutDir,
                "--input", inputs["qt_androiddeployqt_input"][0].filePath, "--aux-mode",
                "--deployment", "bundled",
                "--android-platform", product.Android.sdk.platform,
            ];
            if (product.Qt.android_support.verboseAndroidDeployQt)
                androidDeployQtArgs.push("--verbose");
            var androidDeployQtCmd = new Command(
                        product.Qt.android_support._androidDeployQtFilePath, androidDeployQtArgs);
            androidDeployQtCmd.description = "running androiddeployqt";
            cmds.push(androidDeployQtCmd);

            // We do not want androiddeployqt to write directly into our APK base dir, so
            // we ran it on an isolated directory and now we move stuff over.
            // We remember the files for which we do that, so if the next invocation
            // of androiddeployqt creates fewer files, the other ones are removed from
            // the APK base dir.
            var moveCmd = new JavaScriptCommand();
            moveCmd.description = "processing androiddeployqt output";
            moveCmd.sourceCode = function() {
                File.makePath(product.java.classFilesDir);
                var libsDir = product.Qt.android_support._deployQtOutDir + "/libs";
                var libDir = product.Android.sdk.packageContentsDir + "/lib";
                var listFilePath = outputs["android.deployqt_list"][0].filePath;
                var oldLibs = [];
                try {
                    var listFile = new TextFile(listFilePath, TextFile.ReadOnly);
                    var listFileLine = listFile.readLine();
                    while (listFileLine) {
                        oldLibs.push(listFileLine);
                        listFileLine = listFile.readLine();
                    }
                    listFile.close();
                } catch (e) {
                }
                listFile = new TextFile(listFilePath, TextFile.WriteOnly);
                var newLibs = [];
                var moveLibFiles = function(prefix) {
                    var fullSrcPrefix = FileInfo.joinPaths(libsDir, prefix);
                    var files = File.directoryEntries(fullSrcPrefix, File.Files);
                    for (var i = 0; i < files.length; ++i) {
                        var file = files[i];
                        var srcFilePath = FileInfo.joinPaths(fullSrcPrefix, file);
                        var targetFilePath;
                        if (file.endsWith(".jar"))
                            targetFilePath = FileInfo.joinPaths(product.java.classFilesDir, file);
                        else
                            targetFilePath = FileInfo.joinPaths(libDir, prefix, file);
                        File.move(srcFilePath, targetFilePath);
                        listFile.writeLine(targetFilePath);
                        newLibs.push(targetFilePath);
                    }
                    var dirs = File.directoryEntries(fullSrcPrefix,
                                                     File.Dirs | File.NoDotAndDotDot);
                    for (i = 0; i < dirs.length; ++i)
                        moveLibFiles(FileInfo.joinPaths(prefix, dirs[i]));
                };
                moveLibFiles("");
                listFile.close();
                for (i = 0; i < oldLibs.length; ++i) {
                    if (!newLibs.contains(oldLibs[i]))
                        File.remove(oldLibs[i]);
                }
            };
            cmds.push(moveCmd);

            // androiddeployqt doesn't strip the deployed libraries anymore so it has to done here
            // but only for release build
            if (product.qbs.buildVariant == "release") {
                var stripLibsCmd = new JavaScriptCommand();
                stripLibsCmd.description = "stripping unneeded symbols from deployed qt libraries";
                stripLibsCmd.sourceCode = function() {
                    var stripArgs = ["--strip-all"];
                    var architectures = [];
                    for (var i in inputs["android.nativelibrary"])
                        architectures.push(inputs["android.nativelibrary"][i].Android.ndk.abi);
                    for (var i in architectures) {
                        var abiDirPath = FileInfo.joinPaths(product.Android.sdk.packageContentsDir,
                                                            "lib", architectures[i]);
                        var files = File.directoryEntries(abiDirPath, File.Files);
                        for (var i = 0; i < files.length; ++i) {
                            var filePath = FileInfo.joinPaths(abiDirPath, files[i]);
                            if (FileInfo.suffix(filePath) == "so") {
                                stripArgs.push(filePath);
                            }
                        }
                    }
                    var process = new Process();
                    process.exec(product.cpp.stripPath, stripArgs, false);
                }
                cmds.push(stripLibsCmd);
            }

            var correctingCmd = new JavaScriptCommand();
            if (product.Qt.android_support._correctQtNetworkDependencies) {
                correctingCmd.description = "correcting network jar dependency";
                correctingCmd.sourceCode = function() {
                    var findNetworkLib = function() {
                        var libsDir = product.Android.sdk.packageContentsDir + "/lib";
                        var dirList = File.directoryEntries(libsDir, File.Dirs |
                                                            File.NoDotAndDotDot);
                        for (var i = 0; i < dirList.length; ++i) {
                            var archDir = FileInfo.joinPaths(libsDir, dirList[i]);
                            var fileList = File.directoryEntries(archDir, File.Files);
                            if (fileList) {
                                for (var j = 0; j < fileList.length; ++j) {
                                    if (fileList[j].contains("libQt5Network")) {
                                        return true;
                                    }
                                }
                            }
                        }
                        return false;
                    }

                    if (findNetworkLib()) {
                        var manifestData = new Xml.DomDocument();
                        var manifestFilePath = product.Qt.android_support._deployQtOutDir +
                                "/AndroidManifest.xml"
                        manifestData.load(manifestFilePath);

                        var rootElem = manifestData.documentElement();
                        if (!rootElem || !rootElem.isElement() || rootElem.tagName() != "manifest")
                            throw "No manifest tag found in '" + manifestFilePath + "'.";
                        var appElem = rootElem.firstChild("application");
                        if (!appElem || !appElem.isElement() || appElem.tagName() != "application")
                            throw "No application tag found in '" + manifestFilePath + "'.";
                        var activityElem = appElem.firstChild("activity");
                        if (!activityElem || !activityElem.isElement() ||
                                activityElem.tagName() != "activity")
                            throw "No activity tag found in '" + manifestFilePath + "'.";
                        var metaDataElem = activityElem.firstChild("meta-data");
                        while (metaDataElem && metaDataElem.isElement()) {
                            if (metaDataElem.attribute("android:name") ==
                                    "android.app.load_local_jars" ) {
                                var value = metaDataElem.attribute("android:value");
                                var fileName = "QtAndroidNetwork.jar";
                                metaDataElem.setAttribute("android:value", value + ":jar/" +
                                                          fileName);
                                var jarFilePath = FileInfo.joinPaths(
                                            product.Qt.android_support._qtInstallDir, "jar",
                                            fileName);
                                var targetFilePath = FileInfo.joinPaths(product.java.classFilesDir,
                                                                        fileName);
                                File.copy(jarFilePath, targetFilePath);
                                break;
                            }
                            metaDataElem = metaDataElem.nextSibling("meta-data");
                        }
                        manifestData.save(outputs["android.manifest_final"][0].filePath, 4);
                    } else {
                        File.move(product.Qt.android_support._deployQtOutDir + "/AndroidManifest.xml",
                                  outputs["android.manifest_final"][0].filePath);
                    }
                };
            } else {
                correctingCmd.description = "copying manifest";
                correctingCmd.sourceCode = function() {
                    File.move(product.Qt.android_support._deployQtOutDir + "/AndroidManifest.xml",
                              outputs["android.manifest_final"][0].filePath);
                }
            }
            cmds.push(correctingCmd);

            return cmds;
        }
    }

    Group {
        condition: Qt.android_support._enableSdkSupport
        name: "helper sources from qt"
        prefix: Qt.android_support._templatesBaseDir + "/java/"
        Android.sdk.aidlSearchPaths: prefix + "src"
        files: [
            "**/*.java",
            "**/*.aidl",
        ]
    }

    validate: {
        if (Utilities.versionCompare(version, "5.12") < 0)
            throw ModUtils.ModuleError("Cannot use Qt " + version + " with Android. "
                + "Version 5.12 or later is required.");
    }
}
