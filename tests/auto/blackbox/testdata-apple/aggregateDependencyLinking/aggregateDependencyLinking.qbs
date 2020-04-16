import qbs

Project {
    minimumQbsVersion: "1.8"

    StaticLibrary {
        name: "multi_arch_lib"
        files: ["lib.c"]

        Depends { name: "cpp" }
        Depends { name: "bundle" }
        bundle.isBundle: false

        // This will generate 2 multiplex configs and an aggregate.
        qbs.architectures: ["x86", "x86_64"]
        qbs.buildVariant: "debug"
        cpp.minimumMacosVersion: "10.8"
    }

    CppApplication {
        name: "just_app"
        files: ["app.c"]

        // This should link only against the aggregate static library, and not against
        // the {debug, x86_64} variant, or worse - against both the single arch variant
        // and the lipo-ed one.
        Depends { name: "multi_arch_lib" }

        Depends { name: "bundle" }
        bundle.isBundle: false

        qbs.architecture: "x86_64"
        qbs.buildVariant: "debug"
        cpp.minimumMacosVersion: "10.8"
        multiplexByQbsProperties: []
    }
}
