import qbs.File
import qbs.FileInfo
import qbs.Probes
import qbs.ModUtils
import "../protobufbase.qbs" as ProtobufBase
import "../protobuf.js" as HelperFunctions

ProtobufBase {
    property string includePath: includeProbe.found ? includeProbe.path : undefined
    property string libraryPath: libraryProbe.found ? libraryProbe.path : undefined

    property bool useGrpc: false

    property bool _linkLibraries: true
    readonly property bool _hasModules: protobuflib.present && (!useGrpc || grpcpp.present)

    property string grpcIncludePath: grpcIncludeProbe.found ? grpcIncludeProbe.path : undefined
    property string grpcLibraryPath: grpcLibraryProbe.found ? grpcLibraryProbe.path : undefined

    readonly property string _libraryName: {
        var libraryName;
        if (libraryProbe.found) {
            libraryName = FileInfo.baseName(libraryProbe.fileName);
            if (libraryName.startsWith("lib"))
                libraryName = libraryName.substring(3);
        }
        return libraryName;
    }

    Depends { name: "cpp" }
    Depends {
        name: "protobuflib";
        condition: _linkLibraries;
        required: false;
        enableFallback: false
    }
    Depends {
        name: "grpcpp";
        condition: _linkLibraries && useGrpc;
        required: false;
        enableFallback: false
    }

    property path grpcPluginPath: grpcPluginProbe.filePath

    Probes.BinaryProbe {
        condition: useGrpc
        id: grpcPluginProbe
        names: "grpc_cpp_plugin"
    }

    cpp.libraryPaths: {
        if (!_linkLibraries || _hasModules)
            return [];

        var result = [];
        if (libraryProbe.found)
            result.push(libraryProbe.path);
        if (useGrpc && grpcLibraryProbe.found)
            result.push(grpcLibraryPath);
        return result;
    }
    cpp.dynamicLibraries: {
        if (!_linkLibraries || _hasModules)
            return [];

        var result = [];
        if (_libraryName)
            result.push(_libraryName)
        if (qbs.targetOS.contains("unix"))
            result.push("pthread");
        if (useGrpc)
            result.push("grpc++");
        return result;
    }
    cpp.includePaths: {
        if (!_linkLibraries || _hasModules)
            return [outputDir];

        var result = [outputDir];
        if (includeProbe.found)
            result.push(includePath);
        if (useGrpc && grpcIncludeProbe.found)
            result.push(grpcIncludePath);
        return result;
    }

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
            if (input.fileTags.contains("protobuf.grpc")) {
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
            if (input.fileTags.contains("protobuf.grpc")) {
                result = ModUtils.concatAll(result, HelperFunctions.doPrepare(
                                input.protobuf.cpp, product, input, outputs, "grpc",
                                "protoc-gen-grpc=" + input.protobuf.cpp.grpcPluginPath));
            }
            return result;
        }
    }

    Probes.IncludeProbe {
        id: includeProbe
        names: "google/protobuf/message.h"
        platformSearchPaths: includePath ? [] : base
        searchPaths: includePath ? [includePath] : []
    }

    Probes.LibraryProbe {
        id: libraryProbe
        names: [
            "protobuf",
            "protobufd",
        ]
        platformSearchPaths: libraryPath ? [] : base
        searchPaths: libraryPath ? [libraryPath] : []
    }

    Probes.IncludeProbe {
        id: grpcIncludeProbe
        pathSuffixes: "grpc++"
        names: "grpc++.h"
        platformSearchPaths: grpcIncludePath ? [] : base
        searchPaths: grpcIncludePath ? [grpcIncludePath] : []
    }

    Probes.LibraryProbe {
        id: grpcLibraryProbe
        names: "grpc++"
        platformSearchPaths: grpcLibraryPath ? [] : base
        searchPaths: grpcLibraryPath ? [grpcLibraryPath] : []
    }

    validate: {
        HelperFunctions.validateCompiler(compilerName, compilerPath);

        if (_hasModules)
            return;

        if (_linkLibraries && !includeProbe.found)
            throw "Can't find cpp protobuf include files. Please set the includePath property.";
        if (_linkLibraries && !libraryProbe.found)
            throw "Can't find cpp protobuf library. Please set the libraryPath property.";

        if (useGrpc) {
            if (!File.exists(grpcPluginPath))
                throw "Can't find grpc_cpp_plugin plugin. Please set the grpcPluginPath property.";
            if (_linkLibraries && !grpcIncludeProbe.found)
                throw "Can't find grpc++ include files. Please set the grpcIncludePath property.";
            if (_linkLibraries && !grpcLibraryProbe.found)
                throw "Can't find grpc++ library. Please set the grpcLibraryPath property.";
        }
    }
}
