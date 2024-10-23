import qbs.FileInfo
import qbs.ModUtils
import qbs.TextFile
import qbs.Utilities
import "core.js" as Core
import "moc.js" as Moc
import "qdoc.js" as Qdoc
import "rcc.js" as Rcc

Module {
    condition: (qbs.targetPlatform === targetPlatform || isCombinedUIKitBuild)
               && (!qbs.architecture
                   || architectures.length === 0
                   || architectures.contains(qbs.architecture))

    readonly property bool isCombinedUIKitBuild: ["ios", "tvos", "watchos"].contains(targetPlatform)
        && (!qbs.architecture || ["x86", "x86_64"].contains(qbs.architecture))
        && qbs.targetPlatform === targetPlatform + "-simulator"

    Depends { name: "cpp" }
    Depends { name: "Sanitizers.address"; condition: config.contains("sanitize_address") }

    Group {
        condition: qbs.targetOS.contains("android")
        Depends { name: "Qt.android_support" }
        product.Qt.android_support._qtBinaryDir: FileInfo.path(binPath)
        product.Qt.android_support._qtInstallDir: FileInfo.path(installPath)
        product.Qt.android_support.version: version
        product.Qt.android_support.rccFilePath: Rcc.fullPath(product)
    }

    // qmlImportScanner is required by androiddeployqt even if the project doesn't
    // depend on qml. That's why the scannerName must be defined here and not in the
    // qml module
    property string qmlImportScannerName: "qmlimportscanner"
    property string qmlImportScannerFilePath: qmlLibExecPath + '/' + qmlImportScannerName

    version: @version@
    property stringList architectures: @archs@
    property string targetPlatform: @targetPlatform@
    property string libInfix: @libInfix@
    property stringList config: @config@
    property stringList qtConfig: @qtConfig@
    readonly property stringList enabledFeatures: @enabledFeatures@
    readonly property stringList disabledFeatures: @disabledFeatures@
    property path binPath: @binPath@
    property path installPath: @installPath@
    property path incPath: @incPath@
    property path libPath: @libPath@
    property path installPrefixPath: @installPrefixPath@
    property path libExecPath: @libExecPath@
    property path qmlLibExecPath: @qmlLibExecPath@
    property path pluginPath: @pluginPath@
    property string mkspecName: @mkspecName@
    property path mkspecPath: @mkspecPath@
    property string mocName: "moc"
    property stringList mocFlags: []
    property string lreleaseName: "lrelease"
    property string rccName: "rcc"
    property string qdocName: versionMajor >= 5 ? "qdoc" : "qdoc3"
    property stringList qdocEnvironment
    property path docPath: @docPath@
    property path translationsPath: @translationsPath@
    property string helpGeneratorLibExecPath: @helpGeneratorLibExecPath@
    property stringList helpGeneratorArgs: versionMajor >= 5 ? ["-platform", "minimal"] : []
    property var versionParts: version ? version.split('.').map(function(item) { return parseInt(item, 10); }) : []
    property int versionMajor: versionParts[0]
    property int versionMinor: versionParts[1]
    property int versionPatch: versionParts[2]
    property bool frameworkBuild: @frameworkBuild@
    property bool staticBuild: @staticBuild@
    property bool multiThreading: @multiThreading@
    property stringList pluginMetaData: []
    property bool enableKeywords: true
    property bool generateMetaTypesFile
    readonly property bool _generateMetaTypesFile: generateMetaTypesFile
        && Utilities.versionCompare(version, "5.15") >= 0
    property string metaTypesInstallDir

    property stringList availableBuildVariants: @availableBuildVariants@
    property string qtBuildVariant: {
        if (availableBuildVariants.contains(qbs.buildVariant))
            return qbs.buildVariant;
        if (qbs.buildVariant === "profiling" && availableBuildVariants.contains("release"))
            return "release";
        return availableBuildVariants.length > 0 ? availableBuildVariants[0] : "";
    }

    property stringList staticLibsDebug: @staticLibsDebug@
    property stringList staticLibsRelease: @staticLibsRelease@
    property stringList dynamicLibsDebug: @dynamicLibsDebug@
    property stringList dynamicLibsRelease: @dynamicLibsRelease@
    property stringList staticLibs: qtBuildVariant === "debug"
                                    ? staticLibsDebug : staticLibsRelease
    property stringList dynamicLibs: qtBuildVariant === "debug"
                                    ? dynamicLibsDebug : dynamicLibsRelease
    property stringList linkerFlagsDebug: @linkerFlagsDebug@
    property stringList linkerFlagsRelease: @linkerFlagsRelease@
    property stringList coreLinkerFlags: qtBuildVariant === "debug"
                                    ? linkerFlagsDebug : linkerFlagsRelease
    property stringList frameworksDebug: @frameworksDebug@
    property stringList frameworksRelease: @frameworksRelease@
    property stringList coreFrameworks: qtBuildVariant === "debug"
            ? frameworksDebug : frameworksRelease
    property stringList frameworkPathsDebug: @frameworkPathsDebug@
    property stringList frameworkPathsRelease: @frameworkPathsRelease@
    property stringList coreFrameworkPaths: qtBuildVariant === "debug"
            ? frameworkPathsDebug : frameworkPathsRelease
    property string libNameForLinkerDebug: @libNameForLinkerDebug@
    property string libNameForLinkerRelease: @libNameForLinkerRelease@
    property string libNameForLinker: qtBuildVariant === "debug"
                                      ? libNameForLinkerDebug : libNameForLinkerRelease
    property string libFilePathDebug: @libFilePathDebug@
    property string libFilePathRelease: @libFilePathRelease@
    property string libFilePath: qtBuildVariant === "debug"
                                      ? libFilePathDebug : libFilePathRelease
    property bool useRPaths: qbs.targetOS.contains("linux") && !qbs.targetOS.contains("android")

    property stringList coreLibPaths: @libraryPaths@
    property bool hasLibrary: true

    // These are deliberately not path types
    // We don't want to resolve them against the source directory
    property string generatedHeadersDir: product.buildDirectory + "/qt.headers"
    property string qdocOutputDir: product.buildDirectory + "/qdoc_html"
    property string qmDir: product.destinationDirectory
    property string qmBaseName: product.targetName
    property bool lreleaseMultiplexMode: false

    property stringList moduleConfig: @moduleConfig@

    Properties {
        condition: moduleConfig.contains("use_gold_linker")
        cpp.linkerVariant: "gold"
    }
    Properties {
        condition: Utilities.versionCompare(version, "6") >= 0
        cpp.cxxLanguageVersion: "c++17"
    }
    Properties {
        condition: Utilities.versionCompare(version, "6") < 0
                   && Utilities.versionCompare(version, "5.7.0") >= 0
        cpp.cxxLanguageVersion: "c++11"
    }

    cpp.enableCompilerDefinesByLanguage: ["cpp"].concat(
        qbs.targetOS.contains("darwin") ? ["objcpp"] : [])
    cpp.defines: {
        var defines = @defines@;
        // ### QT_NO_DEBUG must be added if the current build variant is not derived
        //     from the build variant "debug"
        if (!qbs.enableDebugCode)
            defines.push("QT_NO_DEBUG");
        if (!enableKeywords)
            defines.push("QT_NO_KEYWORDS");
        if (qbs.targetOS.containsAny(["ios", "tvos"])) {
            defines = defines.concat(["DARWIN_NO_CARBON", "QT_NO_CORESERVICES", "QT_NO_PRINTER",
                            "QT_NO_PRINTDIALOG"]);
            if (Utilities.versionCompare(version, "5.6.0") < 0)
                defines.push("main=qtmn");
        }
        if (qbs.toolchain.contains("msvc"))
            defines.push("_ENABLE_EXTENDED_ALIGNED_STORAGE");
        return defines;
    }
    cpp.driverFlags: {
        var flags = [];
        if (qbs.toolchain.contains("gcc")) {
            if (config.contains("sanitize_undefined"))
                flags.push("-fsanitize=undefined");
            if (config.contains("sanitize_thread"))
                flags.push("-fsanitize=thread");
            if (config.contains("sanitize_memory"))
                flags.push("-fsanitize=memory");
        }
        return flags;
    }
    cpp.includePaths: generatedHeadersDir
    cpp.systemIncludePaths: @includes@.concat(mkspecPath)
    cpp.libraryPaths: {
        var libPaths = [libPath];
        if (staticBuild && pluginPath)
            libPaths.push(pluginPath + "/platforms");
        libPaths = libPaths.concat(coreLibPaths);
        return libPaths;
    }
    cpp.staticLibraries: {
        var libs = [];
        if (qbs.targetOS.contains('windows') && !product.consoleApplication) {
            libs = libs.concat(qtBuildVariant === "debug"
                               ? @entryPointLibsDebug@ : @entryPointLibsRelease@);
        }
        libs = libs.concat(staticLibs);
        return libs;
    }
    cpp.dynamicLibraries: dynamicLibs
    cpp.linkerFlags: coreLinkerFlags
    cpp.systemFrameworkPaths: coreFrameworkPaths.concat(frameworkBuild ? [libPath] : [])
    cpp.frameworks: {
        var frameworks = coreFrameworks
        if (frameworkBuild)
            frameworks.push(libNameForLinker);
        if (qbs.targetOS.contains('ios') && staticBuild)
            frameworks = frameworks.concat(["Foundation", "CoreFoundation"]);
        if (frameworks.length === 0)
            return undefined;
        return frameworks;
    }
    cpp.rpaths: useRPaths ? libPath : undefined
    Properties {
        condition: qbs.toolchain.contains("msvc") && config.contains("static_runtime")
        cpp.runtimeLibrary: "static"
    }
    Properties {
        condition: qbs.toolchain.contains("msvc") && !config.contains("static_runtime")
        cpp.runtimeLibrary: "dynamic"
    }
    Properties {
        condition: versionMajor >= 5
        cpp.positionIndependentCode: true
    }
    cpp.cxxFlags: {
        var flags = [];
        if (qbs.toolchain.contains('msvc')) {
            if (versionMajor < 5)
                flags.push('/Zc:wchar_t-');
            if (Utilities.versionCompare(version, "6.3") >= 0
                && Utilities.versionCompare(cpp.compilerVersion, "19.10") >= 0) {
                flags.push("/permissive-");
            }
        }
        if (qbs.toolchain.includes("emscripten") && multiThreading)
            flags.push("-pthread");
        return flags;
    }
    Properties {
        condition: qbs.targetOS.contains('darwin') && qbs.toolchain.contains('clang')
                   && config.contains('c++11')
        cpp.cxxStandardLibrary: "libc++"
    }

    Properties {
        condition: qbs.toolchain.includes("emscripten")
        cpp.driverLinkerFlags: {
            var flags = [
                "-sMAX_WEBGL_VERSION=2",
                "-sFETCH=1",
                "-sWASM_BIGINT=1",
                "-sMODULARIZE",
                "-sEXPORT_NAME=createQtAppInstance",
                "-sALLOW_MEMORY_GROWTH",
                '-sASYNCIFY_IMPORTS=["qt_asyncify_suspend_js", "qt_asyncify_resume_js"]',
                '-sEXPORTED_RUNTIME_METHODS=["UTF16ToString", "stringToUTF16", "JSEvents", "specialHTMLTargets", "FS"]',
                "-lembind"
            ];
            if (multiThreading)
                flags.push("-pthread")
            return flags;
        }
    }

    cpp.minimumWindowsVersion: @minWinVersion_optional@
    cpp.minimumMacosVersion: @minMacVersion_optional@
    cpp.minimumIosVersion: @minIosVersion_optional@
    cpp.minimumTvosVersion: @minTvosVersion_optional@
    cpp.minimumWatchosVersion: @minWatchosVersion_optional@
    cpp.minimumAndroidVersion: @minAndroidVersion_optional@

    // Universal Windows Platform support
    cpp.windowsApiFamily: mkspecName.startsWith("winrt-") ? "pc" : undefined
    cpp.windowsApiAdditionalPartitions: mkspecPath.startsWith("winrt-") ? ["phone"] : undefined
    cpp.requireAppContainer: mkspecName.startsWith("winrt-")

    additionalProductTypes: ["qm", "qt.core.metatypes"]

    validate: {
        var validator = new ModUtils.PropertyValidator("Qt.core");
        validator.setRequiredProperty("binPath", binPath);
        validator.setRequiredProperty("incPath", incPath);
        validator.setRequiredProperty("libPath", libPath);
        validator.setRequiredProperty("mkspecPath", mkspecPath);
        validator.setRequiredProperty("version", version);
        validator.setRequiredProperty("config", config);
        validator.setRequiredProperty("qtConfig", qtConfig);
        validator.setRequiredProperty("versionMajor", versionMajor);
        validator.setRequiredProperty("versionMinor", versionMinor);
        validator.setRequiredProperty("versionPatch", versionPatch);

        if (!staticBuild)
            validator.setRequiredProperty("pluginPath", pluginPath);

        // Allow custom version suffix since some distributions might want to do this,
        // but otherwise the version must start with a valid 3-component string
        validator.addVersionValidator("version", version, 3, 3, true);
        validator.addRangeValidator("versionMajor", versionMajor, 1);
        validator.addRangeValidator("versionMinor", versionMinor, 0);
        validator.addRangeValidator("versionPatch", versionPatch, 0);

        validator.addCustomValidator("availableBuildVariants", availableBuildVariants, function (v) {
            return v.length > 0;
        }, "the Qt installation supports no build variants");

        validator.addCustomValidator("qtBuildVariant", qtBuildVariant, function (variant) {
            return availableBuildVariants.contains(variant);
        }, "'" + qtBuildVariant + "' is not supported by this Qt installation");

        validator.addCustomValidator("qtBuildVariant", qtBuildVariant, function (variant) {
            return variant === qbs.buildVariant || !qbs.toolchain.contains("msvc");
        }, " is '" + qtBuildVariant + "', but qbs.buildVariant is '" + qbs.buildVariant
            + "', which is not allowed when using MSVC");

        validator.addFileNameValidator("resourceFileBaseName", resourceFileBaseName);

        validator.validate();
    }

    FileTagger {
        patterns: ["*.qrc"]
        fileTags: ["qrc"]
    }

    FileTagger {
        patterns: ["*.ts"]
        fileTags: ["ts"]
    }

    FileTagger {
        patterns: ["*.qdoc", "*.qdocinc"]
        fileTags: ["qdoc"]
    }

    FileTagger {
        patterns: ["*.qdocconf"]
        fileTags: ["qdocconf"]
    }

    FileTagger {
        patterns: ["*.qhp"]
        fileTags: ["qhp"]
    }

    property bool combineMocOutput: cpp.combineCxxSources
    property bool enableBigResources: false
    // Product should not moc in the aggregate when multiplexing.
    property bool enableMoc: !(product.multiplexed || product.aggregate)
                             || product.multiplexConfigurationId

    Group {
        condition: enableMoc
        Rule {
            name: "QtCoreMocRuleCpp"
            property string cppInput: cpp.combineCxxSources ? "cpp.combine" : "cpp"
            property string objcppInput: cpp.combineObjcxxSources ? "objcpp.combine" : "objcpp"
            inputs: [objcppInput, cppInput]
            auxiliaryInputs: "qt_plugin_metadata"
            excludedInputs: "unmocable"
            outputFileTags: ["hpp", "unmocable", "qt.core.metatypes.in"]
            outputArtifacts: Moc.outputArtifacts.apply(Moc, arguments)
            prepare: Moc.commands.apply(Moc, arguments)
        }
        Rule {
            name: "QtCoreMocRuleHpp"
            inputs: "hpp"
            auxiliaryInputs: ["qt_plugin_metadata", "cpp", "objcpp"];
            excludedInputs: "unmocable"
            outputFileTags: ["hpp", "cpp", "moc_cpp", "unmocable", "qt.core.metatypes.in"]
            outputArtifacts: Moc.outputArtifacts.apply(Moc, arguments)
            prepare: Moc.commands.apply(Moc, arguments)
        }
    }

    Rule {
        multiplex: true
        inputs: ["moc_cpp"]
        Artifact {
            filePath: "amalgamated_moc_" + product.targetName + ".cpp"
            fileTags: ["cpp", "unmocable"]
        }
        prepare: Moc.generateMocCppCommands(inputs, output)
    }

    Rule {
        multiplex: true
        inputs: "qt.core.metatypes.in"
        Artifact {
            filePath: product.targetName.toLowerCase() + "_metatypes.json"
            fileTags: "qt.core.metatypes"
            qbs.install: product.Qt.core.metaTypesInstallDir
            qbs.installDir: product.Qt.core.metaTypesInstallDir
        }
        prepare: Moc.generateMetaTypesCommands(inputs, output)
    }

    property path resourceSourceBase
    property string resourcePrefix: "/"
    property string resourceFileBaseName: product.targetName
    Rule {
        multiplex: true
        inputs: ["qt.core.resource_data"]
        Artifact {
            filePath: product.Qt.core.resourceFileBaseName + ".qrc"
            fileTags: ["qrc"]
        }
        prepare: Rcc.generateQrcFileCommands.apply(Rcc, arguments)
    }

    Rule {
        inputs: ["qrc"]
        outputFileTags: ["cpp", "cpp_intermediate_object"]
        outputArtifacts: Rcc.rccOutputArtifacts(input)
        prepare: Rcc.rccCommands(product, input, output)
    }

    Rule {
        inputs: ["intermediate_obj"]
        Artifact {
            filePath: input.completeBaseName + ".2" + input.cpp.objectSuffix
            fileTags: ["obj"]
        }
        prepare: Rcc.rccPass2Commands(product, input, output)
    }

    Rule {
        inputs: ["ts"]
        multiplex: lreleaseMultiplexMode

        Artifact {
            filePath: FileInfo.joinPaths(product.Qt.core.qmDir,
                    (product.Qt.core.lreleaseMultiplexMode
                     ? product.Qt.core.qmBaseName
                     : input.baseName) + ".qm")
            fileTags: ["qm"]
        }

        prepare: Core.lreleaseCommands.apply(Core, arguments)
    }

    Rule {
        inputs: "qdocconf-main"
        explicitlyDependsOn: ["qdoc", "qdocconf"]
        outputFileTags: ModUtils.allFileTags(Qdoc.qdocFileTaggers())
        outputArtifacts: Qdoc.outputArtifacts(product, input)
        prepare: Qdoc.commands(product, input)
    }

    Rule {
        inputs: "qhp"
        auxiliaryInputs: ModUtils.allFileTags(Qdoc.qdocFileTaggers())
                .filter(function(tag) { return tag !== "qhp"; })

        Artifact {
            filePath: input.completeBaseName + ".qch"
            fileTags: ["qch"]
        }

        prepare: Core.qhelpGeneratorCommands(product, input, output)
    }

    Group
    {
        condition: qbs.toolchain.includes("emscripten") && (product.type.includes("application")
                                                            && product.type.includes("qthtml"))
        Rule {
            inputs: ["application"]
            multiplex: true

            Artifact {
                filePath : FileInfo.joinPaths(product.buildDirectory, product.targetName + ".html")
                fileTags : ["qthtml"]
            }

            prepare: Core.wasmQtHtmlCommands(product, output)
        }

        Group {
            fileTags: ["qthtml"]
            files: [
                module.pluginPath + "/platforms/qtloader.js",
                module.pluginPath + "/platforms/qtlogo.svg"
            ]
        }
    }

    @additionalContent@
}
