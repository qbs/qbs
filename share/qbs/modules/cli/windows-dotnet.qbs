import qbs

CLIModule {
    condition: qbs.toolchain.contains("dotnet")

    debugInfoSuffix: ".pdb"
    csharpCompilerName: "csc"
    vbCompilerName: "vbc"
    fsharpCompilerName: "fsc"
}
