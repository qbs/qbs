import qbs.FileInfo

Project {
    property bool shouldInstallLibrary: true
    property bool lazy: false

    Application {
        condition: {
            var result = qbs.targetPlatform === qbs.hostPlatform;
            if (!result)
                console.info("targetPlatform differs from hostPlatform");
            return result;
        }
        Depends { name: "cpp" }
        Depends {
            name: "functions";
            cpp.symbolLinkMode: product.symbolLinkMode
            cpp.link: !(product.qbs.targetOS.contains("linux") && product.symbolLinkMode === "weak")
        }

        property string symbolLinkMode: project.lazy ? "lazy" : "weak"

        name: "driver"
        files: ["main.cpp"]
        consoleApplication: true
        property string installLib: "SHOULD_INSTALL_LIB=" + project.shouldInstallLibrary
        cpp.defines: {
            if (symbolLinkMode === "weak") {
                return qbs.targetOS.contains("darwin")
                        ? ["WEAK_IMPORT=__attribute__((weak_import))", installLib]
                        : ["WEAK_IMPORT=__attribute__((weak))", installLib];
            }
            return ["WEAK_IMPORT=", installLib];
        }
        cpp.cxxLanguageVersion: "c++11"
        cpp.minimumMacosVersion: "10.7"
        cpp.rpaths: [cpp.rpathOrigin + "/../lib"]

        Group {
            fileTagsFilter: product.type
            qbs.install: true
            qbs.installDir: "bin"
        }
    }

    DynamicLibrary {
        Depends { name: "cpp" }
        Depends { name: "indirect"; cpp.symbolLinkMode: "reexport" }

        Properties {
            condition: qbs.targetOS.contains("darwin")
            bundle.isBundle: false
        }
        name: "functions"
        files: ["lib.cpp"]
        cpp.cxxLanguageVersion: "c++11"
        cpp.minimumMacosVersion: "10.7"
        cpp.rpaths: [cpp.rpathOrigin]

        Properties {
            condition: qbs.targetOS.contains("darwin")
            cpp.sonamePrefix: "@rpath"
        }

        Group {
            condition: project.shouldInstallLibrary
            fileTagsFilter: product.type
            qbs.install: true
            qbs.installDir: "lib"
        }

        Export {
            // let the autotest pass on Linux where reexport is not supported
            Depends { name: "indirect"; condition: !qbs.targetOS.contains("darwin") }

            // on Linux, there is no LC_WEAK_LOAD_DYLIB equivalent (the library is simply omitted
            // from the list of load commands entirely), so use LD_PRELOAD to emulate
            qbs.commonRunEnvironment: {
                var env = original || {};
                if (project.shouldInstallLibrary) {
                    env["LD_PRELOAD"] = FileInfo.joinPaths(qbs.installRoot,
                                                           "lib", "libfunctions.so");
                }
                return env;
            }
        }
    }

    DynamicLibrary {
        Depends { name: "cpp" }

        Properties {
            condition: qbs.targetOS.contains("darwin")
            bundle.isBundle: false
        }
        name: "indirect"
        files: ["indirect.cpp"]
        cpp.cxxLanguageVersion: "c++11"
        cpp.minimumMacosVersion: "10.7"

        Properties {
            condition: qbs.targetOS.contains("darwin")
            // reexport is incompatible with rpath,
            // "ERROR: ld: file not found: @rpath/libindirect.dylib for architecture x86_64"
            cpp.sonamePrefix: qbs.installRoot + "/lib"
        }

        Group {
            fileTagsFilter: product.type
            qbs.install: true
            qbs.installDir: "lib"
        }
    }
}
