CppApplication {
    name: "app"
    cpp._skipAllChecks: true
    Profile {
        name: "unixProfile"
        qbs.targetOS: ["unix"]
        qbs.toolchain: ["gcc"]
        //            qbs.targetPlatform: "unix"
        //            qbs.toolchainType: "gcc"
    }
    Profile {
        name: "gccProfile"
        qbs.targetOS: []
        qbs.toolchain: ["gcc"]
        //            qbs.targetPlatform: undefined
        //            qbs.toolchainType: "gcc"
    }
    multiplexByQbsProperties: ["profiles"]
    qbs.profiles: ["unixProfile", "gccProfile"]
}
