import qbs.FileInfo
import qbs.ModUtils
import qbs.Utilities
import qbs.Xml

import "android_support.js" as Impl

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

    readonly property string _qtAndroidJarFileName: Utilities.versionCompare(version, "6.0") >= 0 ?
                                                       "Qt6Android.jar" : "QtAndroid.jar"

    Group {
        condition: _enableSdkSupport
        name: "helper sources from qt"

        Depends { name: "Android.sdk" }
        Depends { name: "java" }

        product.Android.sdk.customManifestProcessing: true
        product.Android.sdk._bundledInAssets: _multiAbi
        product.java._tagJniHeaders: false // prevent rule cycle

        Properties {
            condition: Utilities.versionCompare(version, "5.15") >= 0
                       && Utilities.versionCompare(version, "6.0") < 0
            java.additionalClassPaths: [FileInfo.joinPaths(_qtInstallDir, "jar", "QtAndroid.jar")]
        }
        Properties {
            condition: Utilities.versionCompare(version, "6.0") >= 0
            java.additionalClassPaths: [FileInfo.joinPaths(_qtInstallDir, "jar", "Qt6Android.jar")]
            Android.sdk.minimumVersion: "23"
        }
        Properties {
            condition: Utilities.versionCompare(version, "6.0") < 0
            Android.sdk.minimumVersion: "21"
        }

        prefix: Qt.android_support._templatesBaseDir + "/java/"
        Android.sdk.aidlSearchPaths: prefix + "src"
        files: [
            "**/*.java",
            "**/*.aidl",
        ]

        Rule {
            multiplex: true
            property stringList inputTags: ["android.nativelibrary", "qrc"]
            inputsFromDependencies: inputTags
            inputs: product.aggregate ? [] : inputTags
            Artifact {
                filePath: "androiddeployqt.json"
                fileTags: "qt_androiddeployqt_input"
            }
            prepare: Impl.prepareDeployQtCommands.apply(Impl, arguments)
        }

        // We use the manifest template from the Qt installation if and only if the project
        // does not provide a manifest file.
        Rule {
            multiplex: true
            requiresInputs: false
            inputs: "android.manifest"
            excludedInputs: "qt.android_manifest"
            outputFileTags: ["android.manifest", "qt.android_manifest"]
            outputArtifacts: Impl.qtManifestOutputArtifacts(inputs)
            prepare: Impl.qtManifestCommands(product, output)
        }

        Rule {
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
            outputArtifacts: Impl.deployQtOutputArtifacts(product)
            prepare: Impl.deployQtCommands.apply(Impl, arguments)
        }
    }

    Group {
        condition: _enableNdkSupport

        Depends { name: "cpp" }
        Depends { name: "Android.ndk" }

        product.Android.ndk.appStl: qbs.toolchain.contains("clang") ? "c++_shared" : "gnustl_shared"
        Properties {
            condition: _multiAbi
            cpp.archSuffix: "_" + Android.ndk.abi
        }

        // "ANDROID_HAS_WSTRING" was removed from qtcore qstring.h in Qt 5.14.0
        Properties {
            condition: Utilities.versionCompare(version, "5.14.0") < 0 &&
                       (Android.ndk.abi === "armeabi-v7a" || Android.ndk.abi === "x86")
            cpp.defines: "ANDROID_HAS_WSTRING"
        }
    }

    validate: {
        if (Utilities.versionCompare(version, "5.12") < 0)
            throw ModUtils.ModuleError("Cannot use Qt " + version + " with Android. "
                + "Version 5.12 or later is required.");
    }
}
