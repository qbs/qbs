/****************************************************************************
**
** Copyright (C) 2015 The Qt Company Ltd.
** Contact: http://www.qt.io/licensing
**
** This file is part of the Qt Build Suite.
**
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms and
** conditions see http://www.qt.io/terms-conditions. For further information
** use the contact form at http://www.qt.io/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 2.1 or version 3 as published by the Free
** Software Foundation and appearing in the file LICENSE.LGPLv21 and
** LICENSE.LGPLv3 included in the packaging of this file.  Please review the
** following information to ensure the GNU Lesser General Public License
** requirements will be met: https://www.gnu.org/licenses/lgpl.html and
** http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html.
**
** In addition, as a special exception, The Qt Company gives you certain additional
** rights.  These rights are described in The Qt Company LGPL Exception
** version 1.1, included in the file LGPL_EXCEPTION.txt in this package.
**
****************************************************************************/

import qbs
import qbs.File
import qbs.FileInfo
import qbs.ModUtils
import qbs.Probes
import qbs.Process
import "typescript.js" as TypeScript

Module {
    Depends { name: "nodejs" }

    additionalProductTypes: ["compiled_typescript"]

    // QBS-833 workaround
    Probes.NodeJsProbe { id: nodejsProbe; pathPrefixes: [nodejsProbe.toolchainInstallPath] }
    nodejs.toolchainInstallPath: nodejsProbe.path
    nodejs.interpreterFileName: nodejsProbe.fileName
    nodejs.interpreterFilePath: nodejsProbe.filePath

    Probes.TypeScriptProbe {
        id: tsc
        pathPrefixes: [toolchainInstallPath]
        nodejsToolchainInstallPath: nodejs.toolchainInstallPath
    }

    property path toolchainInstallPath: tsc.path
    property string version: tsc.version ? tsc.version[2] : undefined
    property var versionParts: version ? version.split('.').map(function(item) { return parseInt(item, 10); }) : []
    property int versionMajor: versionParts[0]
    property int versionMinor: versionParts[1]
    property int versionPatch: versionParts[2]
    property int versionBuild: versionParts[3]
    property string versionSuffix: tsc.version ? tsc.version[3] : undefined

    property string compilerName: tsc.fileName
    property string compilerPath: tsc.filePath

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
        validator.setRequiredProperty("toolchainInstallPath", toolchainInstallPath);
        validator.setRequiredProperty("compilerName", compilerName);
        validator.setRequiredProperty("compilerPath", compilerPath);
        validator.setRequiredProperty("version", version);
        validator.setRequiredProperty("versionParts", versionParts);
        validator.setRequiredProperty("versionMajor", versionMajor);
        validator.setRequiredProperty("versionMinor", versionMinor);
        validator.setRequiredProperty("versionPatch", versionPatch);
        validator.addVersionValidator("version", version, 3, 4);
        validator.addRangeValidator("versionMajor", versionMajor, 1);
        validator.addRangeValidator("versionMinor", versionMinor, 0);
        validator.addRangeValidator("versionPatch", versionPatch, 0);

        if (versionParts && versionParts.length >= 4) {
            validator.setRequiredProperty("versionBuild", versionBuild);
            validator.addRangeValidator("versionBuild", versionBuild, 0);
        }

        validator.validate();
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

    Group {
        name: "io.qt.qbs.internal.typescript-helper"
        files: [
            FileInfo.joinPaths(path, "qbs-tsc-scan", "qbs-tsc-scan.ts"),
            FileInfo.joinPaths(typescript.toolchainInstallPath, "typescript.d.ts")
        ]
        fileTags: ["typescript.typescript-internal"]
    }

    Rule {
        multiplex: true
        inputs: ["typescript.typescript-internal"]

        outputFileTags: ["typescript.compiled_typescript-internal"]
        outputArtifacts: {
            if (!TypeScript.supportsModernFeatures(product))
                return [];
            return [{
                filePath: FileInfo.joinPaths(product.buildDirectory,
                                             ".io.qt.qbs.internal.typescript", "qbs-tsc-scan.js"),
                fileTags: ["typescript.compiled_typescript-internal"]
            }];
        }

        prepare: {
            var inputPaths = inputs["typescript.typescript-internal"].map(function (input) {
                return input.filePath;
            });

            var args = ["--module", "commonjs",
                        "--outDir", FileInfo.path(output.filePath)].concat(inputPaths);
            var cmd = new Command(ModUtils.moduleProperty(product, "compilerPath"), args);
            cmd.silent = true;
            return [cmd];
        }
    }

    Rule {
        id: typescriptCompiler
        multiplex: true
        inputs: ["typescript"]
        inputsFromDependencies: ["typescript_declaration"]
        explicitlyDependsOn: ["typescript.compiled_typescript-internal"]

        outputArtifacts: TypeScript.outputArtifacts(product, inputs)

        outputFileTags: ["application_js", "compiled_typescript", "js", "source_map", "typescript_declaration"]

        prepare: {
            var cmd, cmds = [];

            cmd = new Command(ModUtils.moduleProperty(product, "compilerPath"),
                              TypeScript.tscArguments(product, inputs));
            cmd.description = "compiling " + (ModUtils.moduleProperty(product, "singleFile")
                                                ? outputs.compiled_typescript[0].fileName
                                                : inputs.typescript.map(function(obj) {
                                                                return obj.fileName; }).join(", "));
            cmd.highlight = "compiler";
            cmds.push(cmd);

            // QBS-5
            // Move the compiled JavaScript files compiled by TypeScript to an intermediate
            // directory so that the nodejs module can perform any necessary postprocessing
            // on the result. The nodejs module will move the files back to their original
            // locations after postprocessing.
            cmd = new JavaScriptCommand();
            cmd.silent = true;
            cmd.outDir = product.buildDirectory;
            cmd.sourceCode = function() {
                for (var i = 0; i < outputs.compiled_typescript.length; ++i) {
                    var output = outputs.compiled_typescript[i];
                    var intermediatePath = FileInfo.path(FileInfo.relativePath(product.moduleProperty("nodejs", "compiledIntermediateDir"), output.filePath));
                    var originalFilePath = FileInfo.joinPaths(outDir, intermediatePath, output.fileName);
                    File.copy(originalFilePath, output.filePath);
                    File.remove(originalFilePath);
                }
            };
            cmds.push(cmd);

            return cmds;
        }
    }
}
