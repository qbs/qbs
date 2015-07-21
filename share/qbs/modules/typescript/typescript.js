/****************************************************************************
**
** Copyright (C) 2015 Jake Petroules.
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

var ModUtils = loadExtension("qbs.ModUtils");

function tscArguments(product, inputs, outputs) {
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

    return args;
}

function outputArtifacts(product, inputs) {
    var artifacts = [];

    if (!inputs.typescript) {
        return artifacts;
    }

    if (product.moduleProperty("typescript", "singleFile")) {
        var jsTags = ["js", "compiled_typescript"];

        // We could check
        // if (product.moduleProperty("nodejs", "applicationFile") === inputs.typescript[i].filePath)
        // but since we're compiling to a single file there's no need to state it explicitly
        jsTags.push("application_js");

        var filePath = FileInfo.joinPaths(product.destinationDirectory, product.targetName);

        artifacts.push({fileTags: jsTags,
                        filePath: FileInfo.joinPaths(
                                      product.moduleProperty("nodejs",
                                                             "compiledIntermediateDir"),
                                      product.targetName + ".js")});

        if (product.moduleProperty("typescript", "generateDeclarations")) {
            artifacts.push({fileTags: ["typescript_declaration"],
                            filePath: filePath + ".d.ts"});
        }

        if (product.moduleProperty("typescript", "generateSourceMaps")) {
            artifacts.push({fileTags: ["source_map"],
                            filePath: filePath + ".js.map"});
        }
    } else {
        for (var i = 0; i < inputs.typescript.length; ++i) {
            var jsTags = ["js", "compiled_typescript"];
            if (product.moduleProperty("nodejs", "applicationFile") === inputs.typescript[i].filePath)
                jsTags.push("application_js");

            var intermediatePath = FileInfo.path(FileInfo.relativePath(
                                                     product.sourceDirectory,
                                                     inputs.typescript[i].filePath));

            var baseName = FileInfo.baseName(inputs.typescript[i].fileName);
            var filePath = FileInfo.joinPaths(product.destinationDirectory,
                                              intermediatePath,
                                              baseName);

            artifacts.push({fileTags: jsTags,
                            filePath: FileInfo.joinPaths(
                                          product.moduleProperty("nodejs",
                                                                 "compiledIntermediateDir"),
                                          intermediatePath,
                                          baseName + ".js")});

            if (product.moduleProperty("typescript", "generateDeclarations")) {
                artifacts.push({fileTags: ["typescript_declaration"],
                                filePath: filePath + ".d.ts"});
            }

            if (product.moduleProperty("typescript", "generateSourceMaps")) {
                artifacts.push({fileTags: ["source_map"],
                                filePath: filePath + ".js.map"});
            }
        }
    }

    return artifacts;
}
