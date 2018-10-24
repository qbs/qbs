import qbs.Utilities

Project {
    minimumQbsVersion: "1.8"
    property bool enableX86

    CppApplication {
        Depends { name: "singlelib" }
        Depends { name: "bundle" }
        name: "singleapp"
        targetName: "singleapp"
        files: ["app.c"]
        cpp.rpaths: [cpp.rpathOrigin + "/../../../"]
        cpp.minimumMacosVersion: "10.6"

        // Turn off multiplexing
        aggregate: false
        multiplexByQbsProperties: []

        install: true
        installDir: ""
    }

    CppApplication {
        Depends { name: "singlelib" }
        Depends { name: "bundle" }
        name: "singleapp_agg"
        targetName: "singleapp_agg"
        files: ["app.c"]
        cpp.rpaths: [cpp.rpathOrigin + "/../../../"]
        cpp.minimumMacosVersion: "10.6"

        // Force aggregation when not needed
        aggregate: true
        qbs.architectures: ["x86_64"]
        qbs.buildVariants: ["release"]

        install: true
        installDir: ""
    }

    DynamicLibrary {
        Depends { name: "cpp" }
        Depends { name: "bundle" }
        name: "singlelib"
        targetName: "singlelib"
        files: ["lib.c"]
        cpp.sonamePrefix: qbs.targetOS.contains("darwin") ? "@rpath" : undefined
        cpp.defines: ["VARIANT=" + Utilities.cStringQuote(qbs.buildVariant)]

        // Turn off multiplexing
        aggregate: false
        multiplexByQbsProperties: []

        install: true
        installDir: ""
    }

    CppApplication {
        Depends { name: "multilib" }
        Depends { name: "bundle" }
        name: "multiapp"
        targetName: "multiapp"
        files: ["app.c"]
        cpp.rpaths: [cpp.rpathOrigin + "/../../../"]
        cpp.minimumMacosVersion: "10.6"

        install: true
        installDir: ""
    }

    CppApplication {
        Depends { name: "multilib" }
        Depends { name: "bundle" }
        name: "fatmultiapp"
        targetName: "fatmultiapp"
        files: ["app.c"]
        cpp.rpaths: [cpp.rpathOrigin + "/../../../"]
        cpp.minimumMacosVersion: "10.6"
        qbs.architectures: project.enableX86 ? ["x86", "x86_64"] : ["x86_64"]
        qbs.architecture: "x86_64"
        multiplexByQbsProperties: project.enableX86 ? ["architectures", "buildVariants"]
                                                    : ["buildVariants"]
        qbs.buildVariants: "debug"

        install: true
        installDir: ""
    }

    CppApplication {
        Depends { name: "multilib" }
        Depends { name: "bundle" }
        name: "fatmultiappmultivariant"
        targetName: "fatmultiappmultivariant"
        files: ["app.c"]
        cpp.rpaths: [cpp.rpathOrigin + "/../../../"]
        cpp.minimumMacosVersion: "10.6"
        qbs.architectures: project.enableX86 ? ["x86", "x86_64"] : ["x86_64"]
        qbs.buildVariants: ["debug", "profile"]

        install: true
        installDir: ""
    }

    DynamicLibrary {
        Depends { name: "cpp" }
        Depends { name: "bundle" }
        name: "multilib"
        targetName: "multilib"
        files: ["lib.c"]
        cpp.sonamePrefix: qbs.targetOS.contains("darwin") ? "@rpath" : undefined
        cpp.defines: ["VARIANT=" + Utilities.cStringQuote(qbs.buildVariant)]
        qbs.architectures: project.enableX86 ? ["x86", "x86_64"] : ["x86_64"]
        qbs.buildVariants: ["release", "debug", "profile"]

        install: true
        installDir: ""
    }

    DynamicLibrary {
        Depends { name: "cpp" }
        Depends { name: "bundle" }
        name: "multilib-no-release"
        targetName: "multilib-no-release"
        files: ["lib.c"]
        cpp.sonamePrefix: qbs.targetOS.contains("darwin") ? "@rpath" : undefined
        cpp.defines: ["VARIANT=" + Utilities.cStringQuote(qbs.buildVariant)]
        qbs.architectures: project.enableX86 ? ["x86", "x86_64"] : ["x86_64"]
        qbs.buildVariants: ["debug", "profile"]

        install: true
        installDir: ""
    }

    DynamicLibrary {
        Depends { name: "cpp" }
        Depends { name: "bundle" }
        Depends { name: "multilibB" }
        name: "multilibA"
        files: ["lib.c"]
        cpp.sonamePrefix: "@rpath"
        cpp.defines: ["VARIANT=" + Utilities.cStringQuote(qbs.buildVariant)]
        qbs.architectures: project.enableX86 ? ["x86", "x86_64"] : ["x86_64"]
        qbs.buildVariants: ["debug", "profile"]
        install: true
        installDir: ""
    }
    DynamicLibrary {
        Depends { name: "cpp" }
        Depends { name: "bundle" }
        name: "multilibB"
        files: ["lib.c"]
        cpp.sonamePrefix: "@rpath"
        cpp.defines: ["VARIANT=" + Utilities.cStringQuote(qbs.buildVariant)]
        qbs.architectures: project.enableX86 ? ["x86", "x86_64"] : ["x86_64"]
        qbs.buildVariants: ["debug", "profile"]
        install: true
        installDir: ""
    }
}
