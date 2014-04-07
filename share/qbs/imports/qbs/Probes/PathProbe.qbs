import qbs 1.0
import qbs.File
import qbs.FileInfo
import qbs.ModUtils

Probe {
    // Inputs
    property stringList names
    property var nameFilter
    property pathList pathPrefixes
    property stringList pathSuffixes
    property pathList platformPaths: [ '/usr', '/usr/local' ]
    property pathList environmentPaths
    property pathList platformEnvironmentPaths

    // Output
    property string path
    property string filePath
    property string fileName

    configure: {
        if (!names)
            throw '"names" must be specified';
        var _names = ModUtils.concatAll(names);
        if (nameFilter)
            _names = _names.map(nameFilter);
        // FIXME: Suggest how to obtain paths from system
        var _paths = ModUtils.concatAll(pathPrefixes, platformPaths);
        // FIXME: Add getenv support
        var envs = ModUtils.concatAll(platformEnvironmentPaths, environmentPaths);
        for (var i = 0; i < envs.length; ++i) {
            var value = qbs.getEnv(envs[i]) || '';
            if (value.length > 0)
                _paths = _paths.concat(value.split(qbs.pathListSeparator));
        }
        var _suffixes = ModUtils.concatAll('', pathSuffixes);
        for (i = 0; i < _names.length; ++i) {
            for (var j = 0; j < _paths.length; ++j) {
                for (var k = 0; k < _suffixes.length; ++k) {
                    var _filePath = FileInfo.joinPaths(_paths[j], _suffixes[k], _names[i]);
                    if (File.exists(_filePath)) {
                        found = true;
                        path = FileInfo.joinPaths(_paths[j], _suffixes[k]);
                        filePath = _filePath;
                        fileName = _names[i];
                        return;
                    }
                }
            }
        }
        found = false;
        path = undefined;
        filePath = undefined;
        fileName = undefined;
    }
}
