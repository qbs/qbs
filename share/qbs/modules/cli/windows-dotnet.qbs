import qbs
import qbs.Utilities

CLIModule {
    condition: qbs.toolchain && qbs.toolchain.contains("dotnet")

    debugInfoSuffix: ".pdb"
    csharpCompilerName: "csc"
    vbCompilerName: "vbc"
    fsharpCompilerName: "fsc"

    toolchainInstallPath: Utilities.getNativeSetting(registryKey, "InstallPath")

    // private properties
    property string registryKey: {
        // TODO: Use a probe after QBS-833 is resolved
        // Querying the registry on-demand should be fast enough for now
        // https://msdn.microsoft.com/en-us/library/hh925568.aspx
        var keys = [
            "HKEY_LOCAL_MACHINE\\SOFTWARE\\Microsoft\\NET Framework Setup\\NDP",
            "HKEY_LOCAL_MACHINE\\SOFTWARE\\Wow6432Node\\Microsoft\\NET Framework Setup\\NDP"
        ];
        for (var i in keys) {
            var key = keys[i] + "\\v4\\Full";
            if (Utilities.getNativeSetting(key, "InstallPath"))
                return key;
        }
    }
}
