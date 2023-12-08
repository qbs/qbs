import qbs.File
import qbs.FileInfo
import qbs.Probes
import qbs.ModUtils
import "../protobufbase.qbs" as ProtobufBase
import "../protobuf.js" as HelperFunctions

ProtobufBase {
    property bool useGrpc: false

    property bool _linkLibraries: true
    readonly property bool _hasModules: protobuflib.present && (!useGrpc || grpcpp.present)

    property string _cxxLanguageVersion: "c++17"

    cpp.includePaths: outputDir

    Depends { name: "cpp" }
    Depends {
        name: "protobuflib";
        condition: _linkLibraries;
        required: false
    }
    Depends {
        name: "grpc++";
        id: grpcpp
        condition: _linkLibraries && useGrpc;
        required: false
    }

    property path grpcPluginPath: grpcPluginProbe.filePath

    Probes.BinaryProbe {
        condition: useGrpc
        id: grpcPluginProbe
        names: "grpc_cpp_plugin"
    }

    cpp.cxxLanguageVersion: _cxxLanguageVersion

    Rule {
        inputs: ["protobuf.input", "protobuf.grpc"]
        outputFileTags: ["hpp", "protobuf.hpp", "cpp"]
        outputArtifacts: {
            var outputDir = HelperFunctions.getOutputDir(input.protobuf.cpp, input);
            var result = [
                        HelperFunctions.cppArtifact(outputDir, input, ["hpp", "protobuf.hpp"],
                                                    ".pb.h"),
                        HelperFunctions.cppArtifact(outputDir, input, "cpp", ".pb.cc")
                    ];
            if (input.fileTags.includes("protobuf.grpc")) {
                result.push(
                        HelperFunctions.cppArtifact(outputDir, input, ["hpp", "protobuf.hpp"],
                                                    ".grpc.pb.h"),
                        HelperFunctions.cppArtifact(outputDir, input, ["cpp"], ".grpc.pb.cc"));
            }

            return result;
        }

        prepare: {
            var result = HelperFunctions.doPrepare(
                        input.protobuf.cpp, product, input, outputs, "cpp");
            if (input.fileTags.includes("protobuf.grpc")) {
                result = ModUtils.concatAll(result, HelperFunctions.doPrepare(
                                input.protobuf.cpp, product, input, outputs, "grpc",
                                "protoc-gen-grpc=" + input.protobuf.cpp.grpcPluginPath));
            }
            return result;
        }
    }

    validate: {
        HelperFunctions.validateCompiler(compilerName, compilerPath);

        if (_linkLibraries && ! _hasModules) {
            throw "Can't find cpp protobuf runtime. Make sure .pc files are present";
        }

        if (useGrpc) {
            if (!File.exists(grpcPluginPath))
                throw "Can't find grpc_cpp_plugin plugin. Please set the grpcPluginPath property.";
        }
    }
}
