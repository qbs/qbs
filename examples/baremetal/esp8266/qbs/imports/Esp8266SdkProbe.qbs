import qbs.Environment
import qbs.File
import qbs.FileInfo
import qbs.Probes

Probes.PathProbe {
    // Inputs.
    environmentPaths: Environment.getEnv("ESP8266_NON_OS_SDK_ROOT")
    // Outputs.
    property string includesPath;
    property string libsPath;
    property string linkerScriptsPath;
    configure: {
        for (var i in environmentPaths) {
            var rootPath = environmentPaths[i];
            if (!File.exists(rootPath))
                continue;
            includesPath = FileInfo.joinPaths(rootPath, "include");
            libsPath = FileInfo.joinPaths(rootPath, "lib");
            linkerScriptsPath = FileInfo.joinPaths(rootPath, "ld");
            found = true;
            return;
        }
    }
}
