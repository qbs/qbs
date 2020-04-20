import qbs.Utilities

import "helpers.js" as Helpers

Project {
    minimumQbsVersion: "1.8"
    condition: xcodeVersion
    property string xcodeVersion

    CppApplication {
        Depends { name: "singlelib" }
        Depends { name: "bundle" }
        property bool isShallow: {
            console.info("isShallow: " + bundle.isShallow);
            return bundle.isShallow;
        }
        name: "singleapp"
        targetName: "singleapp"
        files: ["app.c"]
        cpp.rpaths: [cpp.rpathOrigin + "/../../../"]
        cpp.minimumMacosVersion: "10.6"
        cpp.minimumIosVersion: "8.0"

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
        cpp.minimumIosVersion: "8.0"

        // Force aggregation when not needed
        aggregate: true
        qbs.architectures: [Helpers.getNewArch(qbs)]
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
        cpp.minimumIosVersion: "8.0"

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
        cpp.minimumIosVersion: "8.0"
        qbs.architectures: Helpers.getArchitectures(qbs, project.xcodeVersion)
        qbs.architecture: Helpers.getNewArch(qbs)
        multiplexByQbsProperties: Helpers.enableOldArch(qbs, project.xcodeVersion)
                                  ? ["architectures", "buildVariants"]
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
        cpp.minimumIosVersion: "8.0"
        qbs.architectures: Helpers.getArchitectures(qbs, project.xcodeVersion)
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
        cpp.minimumIosVersion: "8.0"
        cpp.sonamePrefix: qbs.targetOS.contains("darwin") ? "@rpath" : undefined
        cpp.defines: ["VARIANT=" + Utilities.cStringQuote(qbs.buildVariant)]
        qbs.architectures: Helpers.getArchitectures(qbs, project.xcodeVersion)
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
        cpp.minimumIosVersion: "8.0"
        cpp.sonamePrefix: qbs.targetOS.contains("darwin") ? "@rpath" : undefined
        cpp.defines: ["VARIANT=" + Utilities.cStringQuote(qbs.buildVariant)]
        qbs.architectures: Helpers.getArchitectures(qbs, project.xcodeVersion)
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
        cpp.minimumIosVersion: "8.0"
        cpp.sonamePrefix: "@rpath"
        cpp.defines: ["VARIANT=" + Utilities.cStringQuote(qbs.buildVariant)]
        qbs.architectures: Helpers.getArchitectures(qbs, project.xcodeVersion)
        qbs.buildVariants: ["debug", "profile"]
        install: true
        installDir: ""
    }
    DynamicLibrary {
        Depends { name: "cpp" }
        Depends { name: "bundle" }
        name: "multilibB"
        files: ["lib.c"]
        cpp.minimumIosVersion: "8.0"
        cpp.sonamePrefix: "@rpath"
        cpp.defines: ["VARIANT=" + Utilities.cStringQuote(qbs.buildVariant)]
        qbs.architectures: Helpers.getArchitectures(qbs, project.xcodeVersion)
        qbs.buildVariants: ["debug", "profile"]
        install: true
        installDir: ""
    }
}
