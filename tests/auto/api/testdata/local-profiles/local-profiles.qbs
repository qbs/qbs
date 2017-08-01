import qbs

Project {
    property string windowsProfile: "windowsProfile"
    property bool enableProfiles
    property stringList mingwToolchain: ["mingw", "gcc"]
    Profile {
        name: windowsProfile
        qbs.targetOS: ["windows"]
    }

    Profile {
        name: "mingwProfile"
        condition: enableProfiles
        baseProfile: project.windowsProfile
        qbs.toolchain: project.mingwToolchain
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

        property stringList clangToolchain: ["clang", "llvm", "gcc"]
        property string clangProfileName: "clangProfile"

        Profile {
            name: product.clangProfileName
            condition: project.enableProfiles
            qbs.targetOS: ["linux", "unix"]
            qbs.toolchain: product.clangToolchain
        }

        multiplexByQbsProperties: ["profiles"]
        qbs.profiles: ["mingwProfile", "clangProfile"]
    }
}
