Project {
    property string windowsProfile: "windowsProfile"
    property bool enableProfiles
    property string mingwToolchain: "mingw"
    property string mingwProfile: "mingwProfile"
    Profile {
        name: windowsProfile
        qbs.targetPlatform: "windows"
    }

    Profile {
        name: project.mingwProfile
        condition: enableProfiles
        baseProfile: project.windowsProfile
        qbs.toolchainType: project.mingwToolchain
    }

    Application {
        name: "app"
        Depends { name: "cpp"; required: false }
        aggregate: false
        multiplexByQbsProperties: ["buildVariants"]
        qbs.buildVariants: ["debug", "release"]
        qbs.profile: project.mingwProfile
    }
    DynamicLibrary {
        name: "lib"

        Depends { name: "cpp"; required: false }

        property string clangToolchain: "clang"
        property string clangProfileName: "clangProfile"

        Profile {
            name: product.clangProfileName
            condition: project.enableProfiles
            qbs.targetPlatform: "linux"
            qbs.toolchainType: product.clangToolchain
        }

        multiplexByQbsProperties: ["profiles"]
        qbs.profiles: [project.mingwProfile, "clangProfile"]
    }
}
