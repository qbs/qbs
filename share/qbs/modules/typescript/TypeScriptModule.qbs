import qbs
import qbs.File
import qbs.FileInfo
import qbs.ModUtils
import qbs.Process

Module {
    Depends { name: "nodejs" }

    additionalProductTypes: ["compiled_typescript"]

    property path toolchainInstallPath
    property string version: {
        var p = new Process();
        p.exec(compilerPath, ["--version"]);
        var match = p.readStdOut().match(/^Version ([0-9]+(\.[0-9]+){1,3})\n$/);
        if (match !== null)
            return match[1];
    }

    property var versionParts: version ? version.split('.').map(function(item) { return parseInt(item, 10); }) : []
    property int versionMajor: versionParts[0]
    property int versionMinor: versionParts[1]
    property int versionPatch: versionParts[2]
    property int versionBuild: versionParts[3]

    property string compilerName: "tsc"
    property string compilerPath: FileInfo.joinPaths(toolchainInstallPath, compilerName)

    property string warningLevel: "normal"
    PropertyOptions {
        name: "warningLevel"
        description: "pedantic to warn on expressions and declarations with an implied 'any' type"
        allowedValues: ["normal", "pedantic"]
    }

    property string targetVersion
    PropertyOptions {
        name: "targetVersion"
        description: "ECMAScript target version"
        allowedValues: ["ES3", "ES5"]
    }

    property string moduleLoader
    PropertyOptions {
        name: "moduleLoader"
        allowedValues: ["commonjs", "amd"]
    }

    property bool stripComments: !qbs.debugInformation
    PropertyOptions {
        name: "stripComments"
        description: "whether to remove comments from the generated output"
    }

    property bool generateDeclarations: false
    PropertyOptions {
        name: "generateDeclarations"
        description: "whether to generate corresponding .d.ts files during compilation"
    }

    // In release mode, nodejs can/should default-enable minification and obfuscation,
    // making the source maps useless, so these default settings work out fine
    property bool generateSourceMaps: qbs.debugInformation
    PropertyOptions {
        name: "generateSourceMaps"
        description: "whether to generate corresponding .map files during compilation"
    }

    property stringList compilerFlags
    PropertyOptions {
        name: "compilerFlags"
        description: "additional flags for the TypeScript compiler"
    }

    property bool singleFile: false
    PropertyOptions {
        name: "singleFile"
        description: "whether to compile all source files to a single output file"
    }

    validate: {
        var validator = new ModUtils.PropertyValidator("typescript");
        validator.setRequiredProperty("version", version);
        validator.setRequiredProperty("versionMajor", versionMajor);
        validator.setRequiredProperty("versionMinor", versionMinor);
        validator.setRequiredProperty("versionPatch", versionPatch);
        validator.setRequiredProperty("versionBuild", versionBuild);
        validator.addVersionValidator("version", version, 4, 4);
        validator.addRangeValidator("versionMajor", versionMajor, 1);
        validator.addRangeValidator("versionMinor", versionMinor, 0);
        validator.addRangeValidator("versionPatch", versionPatch, 0);
        validator.addRangeValidator("versionBuild", versionBuild, 0);
        validator.validate();
    }

    setupBuildEnvironment: {
        if (toolchainInstallPath) {
            var v = new ModUtils.EnvironmentVariable("PATH", qbs.pathListSeparator, qbs.hostOS.contains("windows"));
            v.prepend(toolchainInstallPath);
            v.set();
        }
    }

    // TypeScript declaration files
    FileTagger {
        patterns: ["*.d.ts"]
        fileTags: ["typescript_declaration"]
    }

    // TypeScript source files
    FileTagger {
        patterns: ["*.ts"]
        fileTags: ["typescript"]
    }

    Rule {
        id: typescriptCompiler
        multiplex: true
        inputs: ["typescript"]
        usings: ["typescript_declaration"]

        outputArtifacts: {
            var artifacts = [];

            if (product.moduleProperty("typescript", "singleFile")) {
                var jsTags = ["js", "compiled_typescript"];

                // We could check
                // if (product.moduleProperty("nodejs", "applicationFile") === inputs.typescript[i].filePath)
                // but since we're compiling to a single file there's no need to state it explicitly
                jsTags.push("application_js");

                var filePath = FileInfo.joinPaths(product.destinationDirectory, product.targetName);

                artifacts.push({fileTags: jsTags,
                                filePath: FileInfo.joinPaths(".obj", product.targetName, "typescript", filePath + ".js")});
                artifacts.push({condition: product.moduleProperty("typescript", "generateDeclarations"), // ### QBS-412
                                fileTags: ["typescript_declaration"],
                                filePath: filePath + ".d.ts"});
                artifacts.push({condition: product.moduleProperty("typescript", "generateSourceMaps"), // ### QBS-412
                                fileTags: ["source_map"],
                                filePath: filePath + ".js.map"});
            } else {
                for (var i = 0; i < inputs.typescript.length; ++i) {
                    var jsTags = ["js", "compiled_typescript"];
                    if (product.moduleProperty("nodejs", "applicationFile") === inputs.typescript[i].filePath)
                        jsTags.push("application_js");

                    var filePath = FileInfo.joinPaths(product.destinationDirectory, FileInfo.baseName(inputs.typescript[i].filePath));

                    artifacts.push({fileTags: jsTags,
                                    filePath: FileInfo.joinPaths(".obj", product.targetName, "typescript", filePath + ".js")});
                    artifacts.push({condition: product.moduleProperty("typescript", "generateDeclarations"), // ### QBS-412
                                    fileTags: ["typescript_declaration"],
                                    filePath: filePath + ".d.ts"});
                    artifacts.push({condition: product.moduleProperty("typescript", "generateSourceMaps"), // ### QBS-412
                                    fileTags: ["source_map"],
                                    filePath: filePath + ".js.map"});
                }
            }

            return artifacts;
        }

        outputFileTags: {
            var fileTags = ["js", "compiled_typescript"];
            if (product.moduleProperty("nodejs", "applicationFile"))
                fileTags.push("application_js");
            if (product.moduleProperty("typescript", "generateDeclarations"))
                fileTags.push("typescript_declaration");
            if (product.moduleProperty("typescript", "generateSourceMaps"))
                fileTags.push("source_map");
            return fileTags;
        }

        prepare: {
            var i;
            var args = [];

            var primaryOutput = outputs.compiled_typescript[0];

            if (ModUtils.moduleProperty(product, "warningLevel") === "pedantic") {
                args.push("--noImplicitAny");
            }

            var targetVersion = ModUtils.moduleProperty(product, "targetVersion");
            if (targetVersion) {
                args.push("--target");
                args.push(targetVersion);
            }

            var moduleLoader = ModUtils.moduleProperty(product, "moduleLoader");
            if (moduleLoader) {
                if (ModUtils.moduleProperty(product, "singleFile")) {
                    throw("typescript.singleFile cannot be true when typescript.moduleLoader is set");
                }

                args.push("--module");
                args.push(moduleLoader);
            }

            if (ModUtils.moduleProperty(product, "stripComments")) {
                args.push("--removeComments");
            }

            if (ModUtils.moduleProperty(product, "generateDeclarations")) {
                args.push("--declaration");
            }

            if (ModUtils.moduleProperty(product, "generateSourceMaps")) {
                args.push("--sourcemap");
            }

            // User-supplied flags
            var flags = ModUtils.moduleProperty(product, "compilerFlags");
            for (i in flags) {
                args.push(flags[i]);
            }

            args.push("--outDir");
            args.push(product.buildDirectory);

            if (ModUtils.moduleProperty(product, "singleFile")) {
                args.push("--out");
                args.push(primaryOutput.filePath);
            }

            if (inputs.typescript_declaration) {
                for (i = 0; i < inputs.typescript_declaration.length; ++i) {
                    args.push(inputs.typescript_declaration[i].filePath);
                }
            }

            for (i = 0; i < inputs.typescript.length; ++i) {
                args.push(inputs.typescript[i].filePath);
            }

            var cmd, cmds = [];

            cmd = new Command(ModUtils.moduleProperty(product, "compilerPath"), args);
            cmd.description = "compiling " + (ModUtils.moduleProperty(product, "singleFile")
                                                ? primaryOutput.fileName
                                                : inputs.typescript.map(function(obj) {
                                                                return obj.fileName; }).join(", "));
            cmd.highlight = "compiler";
            cmds.push(cmd);

            // Move all the compiled TypeScript files to the proper intermediate directory
            cmd = new JavaScriptCommand();
            cmd.silent = true;
            cmd.outDir = product.buildDirectory;
            cmd.sourceCode = function() {
                for (i = 0; i < outputs.compiled_typescript.length; ++i) {
                    var fp = outputs.compiled_typescript[i].filePath;
                    var originalFilePath = FileInfo.joinPaths(outDir, FileInfo.fileName(fp));
                    File.copy(originalFilePath, fp);
                    File.remove(originalFilePath);
                }
            };
            cmds.push(cmd);

            return cmds;
        }
    }
}
