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
import qbs.Process
import "typescript.js" as TypeScript

Module {
    Depends { name: "nodejs" }

    additionalProductTypes: ["compiled_typescript"]

    property path toolchainInstallPath
    property string version: rawVersion ? rawVersion[2] : undefined
    property var versionParts: version ? version.split('.').map(function(item) { return parseInt(item, 10); }) : []
    property int versionMajor: versionParts[0]
    property int versionMinor: versionParts[1]
    property int versionPatch: versionParts[2]
    property int versionBuild: versionParts[3]
    property string versionSuffix: rawVersion ? rawVersion[3] : undefined

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

    // private properties
    readonly property var rawVersion: {
        var p = new Process();
        try {
            p.exec(compilerPath, ["--version"]);
            var re = /^(?:message TS6029: )?Version (([0-9]+(?:\.[0-9]+){1,3})(?:-(.+?))?)$/m;
            var match = p.readStdOut().match(re);
            if (match !== null)
                return match;
        } finally {
            p.close();
        }
    }

    validate: {
        var validator = new ModUtils.PropertyValidator("typescript");
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
        inputsFromDependencies: ["typescript_declaration"]

        outputArtifacts: TypeScript.outputArtifacts(product, inputs)

        outputFileTags: ["application_js", "compiled_typescript", "js", "source_map", "typescript_declaration"]

        prepare: {
            var cmd, cmds = [];

            cmd = new Command(ModUtils.moduleProperty(product, "compilerPath"),
                              TypeScript.tscArguments(product, inputs, outputs));
            cmd.description = "compiling " + (ModUtils.moduleProperty(product, "singleFile")
                                                ? primaryOutput.fileName
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
