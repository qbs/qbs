import qbs.File
import qbs.Host
import qbs.Probes

CLIModule {
    condition: qbs.toolchain && qbs.toolchain.includes("mono")

    debugInfoSuffix: ".mdb"
    csharpCompilerName: "mcs"
    vbCompilerName: "vbnc"
    fsharpCompilerName: "fsharpc"

    Probes.PathProbe {
        id: monoProbe
        names: ["mono"]
        platformSearchPaths: {
            var paths = [];
            if (Host.os().includes("macos"))
                paths.push("/Library/Frameworks/Mono.framework/Commands");
            if (Host.os().includes("unix"))
                paths.push("/usr/bin");
            return paths;
        }
    }

    toolchainInstallPath: monoProbe.path
}
