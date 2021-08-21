Project {
    minimumQbsVersion: "1.8"

    StaticLibrary {
        name: "multi_arch_lib"
        files: ["lib.c"]

        Depends { name: "cpp" }
        Depends { name: "bundle" }
        property bool hasX86Mac: true // cannot use xcode.version in qbs.architectures
        property bool hasArmMac: false
        bundle.isBundle: false

        // This will generate 2 multiplex configs and an aggregate.
        qbs.architectures: {
            if (qbs.targetPlatform === "macos") {
                if (hasX86Mac)
                    return ["x86_64", "x86"];
                else if (hasArmMac)
                    return ["arm64", "x86_64"];
            } else if (qbs.targetPlatform === "ios") {
                return ["arm64", "armv7a"];
            }
            console.info("Cannot build fat binaries for this target platform ("
                         + qbs.targetPlatform + ")");
            return original;
        }

        qbs.buildVariant: "debug"
        cpp.minimumMacosVersion: "10.8"
    }

    CppApplication {
        name: "just_app"
        files: ["app.c"]

        // This should link only against the aggregate static library, and not against
        // the {debug, arm64} variant, or worse - against both the single arch variant
        // and the lipo-ed one.
        Depends { name: "multi_arch_lib" }

        Depends { name: "bundle" }
        bundle.isBundle: false

        qbs.architecture: {
            if (qbs.targetPlatform === "macos")
                return "x86_64";
            else if (qbs.targetPlatform === "ios")
                return "arm64";
            return original;
        }
        qbs.buildVariant: "debug"
        cpp.minimumMacosVersion: "10.8"
        multiplexByQbsProperties: []
    }
}
