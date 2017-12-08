import qbs
import "path-probe.js" as PathProbeConfigure

BinaryProbe {
    // Inputs
    property string _compilerName
    property string _toolchainPrefix

    // Outputs
    property string tcPrefix

    names: (_toolchainPrefix || "") + _compilerName

    configure: {
        var result = PathProbeConfigure.configure(names, nameSuffixes, nameFilter, pathPrefixes,
                                                  pathSuffixes, platformPaths, environmentPaths,
                                                  platformEnvironmentPaths, pathListSeparator);
        found = result.found;
        candidatePaths = result.candidatePaths;
        path = result.path;
        filePath = result.filePath;
        fileName = result.fileName;
        (nameSuffixes || [""]).forEach(function(suffix) {
            var end = _compilerName + suffix;
            if (fileName.endsWith(end))
                tcPrefix = fileName.slice(0, -end.length);
        });
    }
}
