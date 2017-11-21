import qbs

Project {
    property string windowsProfile: "windowsProfile"
    property bool enableProfiles
    property string mingwToolchain: "mingw"
    Profile {
        name: windowsProfile
        qbs.targetPlatform: "windows"
    }

    Profile {
        name: "mingwProfile"
        condition: enableProfiles
        baseProfile: project.windowsProfile
        qbs.toolchainType: project.mingwToolchain
    }

    Application {
        name: "app"
        Depends { name: "cpp"; required: false }
        aggregate: false
        multiplexByQbsProperties: ["buildVariants", "profiles"]
        qbs.buildVariants: ["debug", "release"]
        qbs.profiles: ["mingwProfile"]
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
        qbs.profiles: ["mingwProfile", "clangProfile"]
    }
}
