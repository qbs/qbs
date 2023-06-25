import qbs.File
import qbs.FileInfo
import qbs.Probes
import qbs.ModUtils
import "../protobufbase.qbs" as ProtobufBase
import "../protobuf.js" as HelperFunctions

ProtobufBase {
    property string includePath: includeProbe.found ? includeProbe.path : undefined
    property string libraryPath: libraryProbe.found ? libraryProbe.path : undefined
    property string pluginPath: pluginProbe.filePath
    property string pluginName: "protoc-gen-nanopb"
    readonly property string _plugin: "protoc-gen-nanopb=" + pluginPath
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

    cpp.libraryPaths: {
        var result = [];
        if (libraryProbe.found)
            result.push(libraryProbe.path);
        return result;
    }
    cpp.dynamicLibraries: {
        var result = [];
        if (_libraryName)
            result.push(_libraryName);
        return result;
    }
    cpp.includePaths: {
        var result = [outputDir];
        if (includeProbe.found)
            result.push(includePath);
        return result;
    }
    cpp.cxxLanguageVersion: qbs.targetOS.contains("darwin") ? ["c++17"] : ["c++11"]

    Rule {
        inputs: ["protobuf.input"]
        outputFileTags: ["hpp", "protobuf.hpp", "cpp"]
        outputArtifacts: {
            var outputDir = HelperFunctions.getOutputDir(input.protobuf.nanopb, input);
            var result = [
                        HelperFunctions.cppArtifact(outputDir, input, ["hpp", "protobuf.hpp"],
                                                    ".pb.h"),
                        HelperFunctions.cppArtifact(outputDir, input, ["cpp"], ".pb.c")
                    ];

            return result;
        }

        prepare: {
            var options = input.protobuf.nanopb.importPaths.map(function (path) {
                return "-I" + path;
            })

            var result = HelperFunctions.doPrepare(
                        input.protobuf.nanopb, product, input, outputs, "nanopb",
                        input.protobuf.nanopb._plugin, options);
            return result;
        }
    }

    Probes.IncludeProbe {
        id: includeProbe
        names: ["pb.h", "pb_encode.h", "pb_decode.h", "pb_common.h"]
        platformSearchPaths: includePath ? [] : base
        searchPaths: includePath ? [includePath] : []
    }

    Probes.LibraryProbe {
        id: libraryProbe
        names: [
            "protobuf-nanopb",
            "protobuf-nanopbd",
        ]
        platformSearchPaths: libraryPath ? [] : base
        searchPaths: libraryPath ? [libraryPath] : []
    }

    Probes.BinaryProbe {
        id: pluginProbe
        names: pluginName
    }

    validate: {
        HelperFunctions.validateCompiler(compilerName, compilerPath);

        if (!includeProbe.found)
            throw "Can't find nanopb protobuf include files. Please set the includePath property.";
        if (!libraryProbe.found)
            throw "Can't find nanopb protobuf library. Please set the libraryPath property.";
        if (!HelperFunctions.checkPath(pluginPath))
            throw "Can't find nanopb protobuf plugin. Please set the pluginPath property.";
    }
}
