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

    property bool useGrpc: false

    property string grpcIncludePath: grpcIncludeProbe.path
    property string grpcLibraryPath: grpcLibraryProbe.path

    Depends { name: "cpp" }

    property path grpcPluginPath: grpcPluginProbe.filePath

    Probes.BinaryProbe {
        condition: useGrpc
        id: grpcPluginProbe
        names: "grpc_cpp_plugin"
    }

    cpp.libraryPaths: {
        var result = [];
        if (libraryPath)
            result.push(libraryPath);
        if (useGrpc && grpcLibraryPath)
            result.push(grpcLibraryPath);
        return result;
    }
    cpp.dynamicLibraries: {
        var result = ["protobuf"];
        if (qbs.targetOS.contains("unix"))
            result.push("pthread");
        if (useGrpc)
            result.push("grpc++");
        return result;
    }
    cpp.includePaths: {
        var result = [outputDir];
        if (includePath)
            result.push(includePath);
        if (useGrpc && grpcIncludePath)
            result.push(grpcIncludePath);
        return result;
    }

    Rule {
        inputs: ["protobuf.input", "protobuf.grpc"]
        outputFileTags: ["hpp", "cpp"]
        outputArtifacts: {
            var result = [
                        HelperFunctions.cppArtifact(input.protobuf.cpp, product, input, "hpp", ".pb.h"),
                        HelperFunctions.cppArtifact(input.protobuf.cpp, product, input, "cpp", ".pb.cc")
                    ];
            if (input.fileTags.contains("protobuf.grpc")) {
                result.push(
                        HelperFunctions.cppArtifact(input.protobuf.cpp, product, input, "hpp", ".grpc.pb.h"),
                        HelperFunctions.cppArtifact(input.protobuf.cpp, product, input, "cpp", ".grpc.pb.cc"));
            }

            return result;
        }

        prepare: {
            var result = HelperFunctions.doPrepare(
                        input.protobuf.cpp, product, input, outputs, "cpp");
            if (input.fileTags.contains("protobuf.grpc")) {
                result = ModUtils.concatAll(result, HelperFunctions.doPrepareGrpc(
                                input.protobuf.cpp, product, input, outputs, "cpp"));
            }
            return result;
        }
    }

    validateFunc: {
        return function() {
            base();
            if (!HelperFunctions.checkPath(includePath))
                throw "Can't find cpp protobuf include files. Please set the includePath property.";
            if (!HelperFunctions.checkPath(libraryPath))
                throw "Can't find cpp protobuf library. Please set the libraryPath property.";

            if (useGrpc) {
                if (!File.exists(grpcPluginPath))
                    throw "Can't find grpc_cpp_plugin plugin. Please set the grpcPluginPath property.";
                if (!HelperFunctions.checkPath(grpcIncludePath))
                    throw "Can't find grpc++ include files. Please set the grpcIncludePath property.";
                if (!HelperFunctions.checkPath(grpcLibraryPath))
                    throw "Can't find grpc++ library. Please set the grpcLibraryPath property.";
            }
        }
    }

    Probes.IncludeProbe {
        id: includeProbe
        names: "google/protobuf/message.h"
    }

    Probes.LibraryProbe {
        id: libraryProbe
        names: "protobuf"
    }

    Probes.IncludeProbe {
        id: grpcIncludeProbe
        pathSuffixes: "grpc++"
        names: "grpc++.h"
    }

    Probes.LibraryProbe {
        id: grpcLibraryProbe
        names: "grpc++"
    }
}
