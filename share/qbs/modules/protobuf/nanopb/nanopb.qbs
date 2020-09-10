import qbs
import qbs.File
import qbs.FileInfo
import qbs.Probes
import qbs.ModUtils
import "../protobufbase.qbs" as ProtobufBase
import "../protobuf.js" as HelperFunctions

ProtobufBase {
    property string includePath: includeProbe.path
    property string libraryPath: libraryProbe.path
    property string pluginPath: pluginProbe.path
    property string _plugin: "protoc-gen-nanopb=" +
                             FileInfo.joinPaths(pluginPath, "protoc-gen-nanopb")

    Depends { name: "cpp" }

    cpp.libraryPaths: {
        var result = [];
        if (libraryPath)
            result.push(libraryPath);
        return result;
    }
    cpp.dynamicLibraries: "protobuf-nanopb"
    cpp.includePaths: {
        var result = [_outputDir];
        if (includePath)
            result.push(includePath);
        return result;
    }

    Rule {
        inputs: ["protobuf.input"]
        outputFileTags: ["hpp", "cpp"]
        outputArtifacts: {
            var outputDir = HelperFunctions.getOutputDir(input.protobuf.nanopb, input);
            var result = [
                        HelperFunctions.cppArtifact(outputDir, input, "hpp", ".pb.h"),
                        HelperFunctions.cppArtifact(outputDir, input, "cpp", ".pb.c")
                    ];

            return result;
        }

        prepare: {
            var result = HelperFunctions.doPrepare(
                        input.protobuf.nanopb, product, input, outputs, "nanopb",
                        input.protobuf.nanopb._plugin);
            return result;
        }
    }

    Probes.IncludeProbe {
        id: includeProbe
        names: ["pb.h", "pb_encode.h", "pb_decode.h", "pb_common.h"]
    }

    Probes.LibraryProbe {
        id: libraryProbe
        names: "protobuf-nanopb"
    }

    Probes.BinaryProbe {
        id: pluginProbe
        names: "protoc-gen-nanopb"
    }

    validate: {
        HelperFunctions.validateCompiler(compilerName, compilerPath);

        if (!HelperFunctions.checkPath(includePath))
            throw "Can't find nanopb protobuf include files. Please set the includePath property.";
        if (!HelperFunctions.checkPath(libraryPath))
            throw "Can't find nanopb protobuf library. Please set the libraryPath property.";
        if (!HelperFunctions.checkPath(pluginPath))
            throw "Can't find nanopb protobuf plugin. Please set the pluginPath property.";
    }
}
