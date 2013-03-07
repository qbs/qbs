import qbs 1.0
import qbs.fileinfo as FileInfo
import "utils.js" as Utils

Probe {
    // Inputs
    property var names
    property pathList pathPrefixes
    property var pathSuffixes
    property pathList platformPaths: [ '/usr', '/usr/local' ]
    property var environmentPaths
    property var platformEnvironmentPaths

    // Output
    property string path
    property string filePath

    configure: {
        if (!names)
            throw '"names" must be specified';
        var _names = Utils.concatAll(names);
        // FIXME: Suggest how to obtain paths from system
        var _paths = Utils.concatAll(pathPrefixes, platformPaths);
        // FIXME: Add getenv support
        var envs = Utils.concatAll(platformEnvironmentPaths, environmentPaths);
        for (var i = 0; i < envs.length; ++i) {
            var value = qbs.getenv(envs[i]) || '';
            if (value.length > 0)
                _paths = _paths.concat(value.split(qbs.pathListSeparator));
        }
        var _suffixes = Utils.concatAll('', pathSuffixes);
        for (i = 0; i < _names.length; ++i) {
            for (var j = 0; j < _paths.length; ++j) {
                for (var k = 0; k < _suffixes.length; ++k) {
                    var fileName = FileInfo.joinPaths(_paths[j], _suffixes[k], _names[i]);
                    if (File.exists(fileName)) {
                        found = true;
                        path = FileInfo.joinPaths(_paths[j], _suffixes[k]);
                        filePath = fileName;
                        return;
                    }
                }
            }
        }
        found = false;
        path = undefined;
    }
}
