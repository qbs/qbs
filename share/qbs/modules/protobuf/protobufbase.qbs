import qbs.File
import qbs.FileInfo
import qbs.Probes
import "protobuf.js" as HelperFunctions

Module {
    property string compilerName: "protoc"
    property string compilerPath: compilerProbe.filePath
    property var _searchPaths

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
}
