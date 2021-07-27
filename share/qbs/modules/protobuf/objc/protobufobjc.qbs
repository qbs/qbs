import qbs.File
import qbs.FileInfo
import qbs.Probes
import "../protobufbase.qbs" as ProtobufBase
import "../protobuf.js" as HelperFunctions

ProtobufBase {
    property string includePath: includeProbe.path
    property string libraryPath: libraryProbe.path
    property string frameworkPath: frameworkProbe.path

    Depends { name: "cpp" }

    cpp.includePaths: [outputDir].concat(!frameworkPath && includePath ? [includePath] : [])
    cpp.libraryPaths: !frameworkPath && libraryPath ? [libraryPath] : []
    cpp.dynamicLibraries: !frameworkPath && libraryPath ? ["ProtocolBuffers"] : []
    cpp.frameworkPaths: frameworkPath ? [frameworkPath] : []
    cpp.frameworks: ["Foundation"].concat(frameworkPath ? ["Protobuf"] : [])
    cpp.defines: frameworkPath ? ["GPB_USE_PROTOBUF_FRAMEWORK_IMPORTS"] : []

    Rule {
        inputs: ["protobuf.input"]
        outputFileTags: ["hpp", "protobuf.hpp", "objc"]
        outputArtifacts: {
            var outputDir = HelperFunctions.getOutputDir(input.protobuf.objc, input);
            return [
                HelperFunctions.objcArtifact(outputDir, input, ["hpp", "protobuf.hpp"],
                                             ".pbobjc.h"),
                HelperFunctions.objcArtifact(outputDir, input, ["objc"], ".pbobjc.m")
            ];
        }

        prepare: HelperFunctions.doPrepare(input.protobuf.objc, product, input, outputs, "objc")
    }

    Probes.IncludeProbe {
        id: includeProbe
        names: "GPBMessage.h"
    }

    Probes.LibraryProbe {
        id: libraryProbe
        names: "ProtocolBuffers"
    }

    Probes.FrameworkProbe {
        id: frameworkProbe
        names: ["Protobuf"]
    }

    validate: {
        HelperFunctions.validateCompiler(compilerName, compilerPath);
        if (!HelperFunctions.checkPath(frameworkPath)) {
            if (!HelperFunctions.checkPath(includePath)) {
                throw "Can't find objective-c protobuf include files. Please set the includePath "
                        + "or frameworkPath property.";
            }
            if (!HelperFunctions.checkPath(libraryPath)) {
                throw "Can't find objective-c protobuf library. Please set the libraryPath or "
                        + "frameworkPath property.";
            }
        }
    }
}
