import qbs
import qbs.Utilities

Project {
    minimumQbsVersion: "1.8"

    CppApplication {
        Depends { name: "singlelib" }
        Depends { name: "bundle" }
        name: "singleapp"
        targetName: "singleapp"
        files: ["app.c"]
        cpp.rpaths: qbs.targetOS.contains("darwin") ? ["@loader_path/../../../"] : []
        cpp.minimumMacosVersion: "10.5"

        // Turn off multiplexing
        aggregate: false
        multiplexByQbsProperties: []

        Group {
            fileTagsFilter: ["bundle.content"]
            qbs.install: true
            qbs.installSourceBase: product.buildDirectory
        }
    }

    CppApplication {
        Depends { name: "singlelib" }
        Depends { name: "bundle" }
        name: "singleapp_agg"
        targetName: "singleapp_agg"
        files: ["app.c"]
        cpp.rpaths: qbs.targetOS.contains("darwin") ? ["@loader_path/../../../"] : []
        cpp.minimumMacosVersion: "10.5"

        // Force aggregation when not needed
        aggregate: true
        qbs.architectures: ["x86_64"]
        qbs.buildVariants: ["release"]

        Group {
            fileTagsFilter: ["bundle.content"]
            qbs.install: true
            qbs.installSourceBase: product.buildDirectory
        }
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

        Group {
            fileTagsFilter: ["bundle.content"]
            qbs.install: true
            qbs.installSourceBase: product.buildDirectory
        }
    }

    CppApplication {
        Depends { name: "multilib" }
        Depends { name: "bundle" }
        name: "multiapp"
        targetName: "multiapp"
        files: ["app.c"]
        cpp.rpaths: qbs.targetOS.contains("darwin") ? ["@loader_path/../../../"] : []
        cpp.minimumMacosVersion: "10.5"

        Group {
            fileTagsFilter: ["bundle.content"]
            qbs.install: true
            qbs.installSourceBase: product.buildDirectory
        }
    }

    CppApplication {
        Depends { name: "multilib" }
        Depends { name: "bundle" }
        name: "fatmultiapp"
        targetName: "fatmultiapp"
        files: ["app.c"]
        cpp.rpaths: qbs.targetOS.contains("darwin") ? ["@loader_path/../../../"] : []
        cpp.minimumMacosVersion: "10.5"
        qbs.architectures: ["x86", "x86_64"]

        Group {
            fileTagsFilter: ["bundle.content"]
            qbs.install: true
            qbs.installSourceBase: product.buildDirectory
        }
    }

    DynamicLibrary {
        Depends { name: "cpp" }
        Depends { name: "bundle" }
        name: "multilib"
        targetName: "multilib"
        files: ["lib.c"]
        cpp.sonamePrefix: qbs.targetOS.contains("darwin") ? "@rpath" : undefined
        cpp.defines: ["VARIANT=" + Utilities.cStringQuote(qbs.buildVariant)]
        qbs.architectures: ["x86", "x86_64"]
        qbs.buildVariants: ["release", "debug", "profile"]

        Group {
            fileTagsFilter: ["bundle.content"]
            qbs.install: true
            qbs.installSourceBase: product.buildDirectory
        }
    }

    DynamicLibrary {
        Depends { name: "cpp" }
        Depends { name: "bundle" }
        name: "multilib-no-release"
        targetName: "multilib-no-release"
        files: ["lib.c"]
        cpp.sonamePrefix: qbs.targetOS.contains("darwin") ? "@rpath" : undefined
        cpp.defines: ["VARIANT=" + Utilities.cStringQuote(qbs.buildVariant)]
        qbs.architectures: ["x86", "x86_64"]
        qbs.buildVariants: ["debug", "profile"]

        Group {
            fileTagsFilter: ["bundle.content"]
            qbs.install: true
            qbs.installSourceBase: product.buildDirectory
        }
    }
}
