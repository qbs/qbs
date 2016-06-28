import qbs

CLIModule {
    condition: qbs.toolchain && qbs.toolchain.contains("mono")

    debugInfoSuffix: ".mdb"
    csharpCompilerName: "mcs"
    vbCompilerName: "vbnc"
    fsharpCompilerName: "fsharpc"

    toolchainInstallPath: {
        if (qbs.hostOS.contains("macos"))
            return "/Library/Frameworks/Mono.framework/Commands";
    }
}
