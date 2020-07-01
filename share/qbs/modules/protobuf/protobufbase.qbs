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

    property string outputDir: product.buildDirectory + "/protobuf"

    property var validateFunc: {
        return function() {
            if (!File.exists(_compilerPath))
                throw "Can't find protoc binary. Please set the compilerPath property or make "
                        +  "sure the compiler is found in PATH";
        }
    }

    FileTagger {
        patterns: ["*.proto"]
        fileTags: ["protobuf.input"];
    }

    Probes.BinaryProbe {
        id: compilerProbe
        names: [compilerName]
    }

    validate: {
        validateFunc();
    }
}
