import qbs
import qbs.File
import qbs.FileInfo
import qbs.Probes
import "protobuf.js" as HelperFunctions

Module {
    property string compilerName: "protoc"
    property string compilerPath: compilerProbe.filePath

    property string protocBinary
    PropertyOptions {
        name: "protocBinary"
        description: "The path to the protoc binary. Deprecated, use compilerPath instead."
        removalVersion: "1.18"
    }
    readonly property string _compilerPath: protocBinary ? protocBinary : compilerPath
    property pathList importPaths: []

    property string _outputDir: product.buildDirectory + "/protobuf"

    FileTagger {
        patterns: ["*.proto"]
        fileTags: ["protobuf.input"];
    }

    Probes.BinaryProbe {
        id: compilerProbe
        names: [compilerName]
    }
}
