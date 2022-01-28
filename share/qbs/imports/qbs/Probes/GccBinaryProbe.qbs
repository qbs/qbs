import qbs.Environment
import qbs.FileInfo
import qbs.Host
import "path-probe.js" as PathProbeConfigure

BinaryProbe {
    nameSuffixes: undefined // _compilerName already contains ".exe" suffix on Windows
    // Inputs
    property string _compilerName
    property string _toolchainPrefix

    // Outputs
    property string tcPrefix

    platformSearchPaths: {
        var paths = base;
        if (qbs.targetOS.contains("windows") && Host.os().contains("windows"))
            paths.push(FileInfo.joinPaths(
                           Environment.getEnv("SystemDrive"), "MinGW", "bin"));
        return paths;
    }

    names: {
        var prefixes = [];
        if (_toolchainPrefix) {
            prefixes.push(_toolchainPrefix);
        } else {
            var arch = qbs.architecture;
            if (qbs.targetOS.contains("windows")) {
                if (!arch || arch === "x86") {
                    prefixes.push("mingw32-",
                                  "i686-w64-mingw32-",
                                  "i686-w64-mingw32.shared-",
                                  "i686-w64-mingw32.static-",
                                  "i686-mingw32-",
                                  "i586-mingw32msvc-");
                }
                if (!arch || arch === "x86_64") {
                    prefixes.push("x86_64-w64-mingw32-",
                                  "x86_64-w64-mingw32.shared-",
                                  "x86_64-w64-mingw32.static-",
                                  "amd64-mingw32msvc-");
                }
            }
        }
        return prefixes.map(function(prefix) {
            return prefix + _compilerName;
        }).concat([_compilerName]);
    }

    configure: {
        var selectors;
        var results = PathProbeConfigure.configure(
                    selectors, names, nameSuffixes, nameFilter, candidateFilter, searchPaths,
                    pathSuffixes, platformSearchPaths, environmentPaths, platformEnvironmentPaths);

        found = results.found;
        if (!found)
            return;

        var resultsMapper = function(result) {
            (nameSuffixes || [""]).forEach(function(suffix) {
                var end = _compilerName + suffix;
                if (result.fileName.endsWith(end))
                    result.tcPrefix = result.fileName.slice(0, -end.length);
            });
            return result;
        };
        results.files = results.files.map(resultsMapper);
        allResults = results.files;
        var result = results.files[0];
        candidatePaths = result.candidatePaths;
        path = result.path;
        filePath = result.filePath;
        fileName = result.fileName;
        tcPrefix = result.tcPrefix;
    }
}
