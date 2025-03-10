import qbs.File
import qbs.FileInfo
import qbs.Probes
import "protobuf.js" as HelperFunctions

Module {
    property string compilerName: "protoc"
    property string compilerPath: compilerProbe.filePath
    property var _searchPaths

    property string pluginPath: pluginProbe.filePath
    property string pluginName
    property string pluginExecutableName: pluginName
    readonly property string _plugin: {
        if (pluginPath === undefined)
            return undefined;
        if (pluginName === undefined)
            return pluginPath;
        return pluginName + "=" + pluginPath;
    }

    property pathList importPaths: []

    readonly property string outputDir: product.buildDirectory + "/protobuf"

    FileTagger {
        patterns: ["*.proto"]
        fileTags: ["protobuf.input"]
    }

    Probes.BinaryProbe {
        id: compilerProbe
        names: [compilerName]
        searchPaths: _searchPaths
    }

    Probes.BinaryProbe {
        id: pluginProbe
        condition: pluginExecutableName !== undefined
        names: pluginExecutableName
    }
}
