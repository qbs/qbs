/****************************************************************************
**
** Copyright (C) 2015 The Qt Company Ltd.
** Contact: http://www.qt.io/licensing
**
** This file is part of Qbs.
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

var ModUtils = require("qbs.ModUtils");
var Utilities = require("qbs.Utilities");

function args(product, input, outputs)
{
    var defines = product.cpp.compilerDefinesByLanguage;
    if (input.fileTags.contains("objcpp"))
        defines = ModUtils.flattenDictionary(defines["objcpp"]) || [];
    else if (input.fileTags.contains("cpp") || input.fileTags.contains("hpp"))
        defines = ModUtils.flattenDictionary(defines["cpp"]) || [];
    else
        defines = [];
    defines = defines.uniqueConcat(product.cpp.platformDefines);
    defines = defines.uniqueConcat(input.cpp.defines);
    var includePaths = input.cpp.includePaths;
    includePaths = includePaths.uniqueConcat(input.cpp.systemIncludePaths);
    var useCompilerPaths = product.Qt.core.versionMajor >= 5;
    if (useCompilerPaths) {
        includePaths = includePaths.uniqueConcat(input.cpp.compilerIncludePaths);
    }
    var frameworkPaths = product.cpp.frameworkPaths;
    frameworkPaths = frameworkPaths.uniqueConcat(
                product.cpp.systemFrameworkPaths);
    if (useCompilerPaths) {
        frameworkPaths = frameworkPaths.uniqueConcat(
                    product.cpp.compilerFrameworkPaths);
    }
    var pluginMetaData = product.Qt.core.pluginMetaData;
    var args = [];
    if (product.Qt.core._generateMetaTypesFile)
        args.push("--output-json");
    var outputFileName;
    for (tag in outputs) {
        if (tag !== "qt.core.metatypes.in") {
            outputFileName = outputs[tag][0].filePath;
            break;
        }
    }
    args = args.concat(
                defines.map(function(item) { return '-D' + item; }),
                includePaths.map(function(item) { return '-I' + item; }),
                frameworkPaths.map(function(item) { return '-F' + item; }),
                pluginMetaData.map(function(item) { return '-M' + item; }),
                product.Qt.core.mocFlags,
                '-o', outputFileName,
                input.filePath);
    return args;
}

function fullPath(product)
{
    if (Utilities.versionCompare(product.Qt.core.version, "6.1") < 0)
        return product.Qt.core.binPath + '/' + product.Qt.core.mocName;
    return product.Qt.core.libExecPath + '/' + product.Qt.core.mocName;
}

function outputArtifacts(project, product, inputs, input)
{
    var mocInfo = QtMocScanner.apply(input);
    if (!mocInfo.hasQObjectMacro)
        return [];
    var artifact = { fileTags: ["unmocable"] };
    var artifacts = [artifact];
    if (mocInfo.hasPluginMetaDataMacro)
        artifact.explicitlyDependsOn = ["qt_plugin_metadata"];
    if (input.fileTags.contains("hpp")) {
        artifact.filePath = input.Qt.core.generatedHeadersDir
                + "/moc_" + input.completeBaseName + ".cpp";
        var amalgamate = input.Qt.core.combineMocOutput;
        artifact.fileTags.push(mocInfo.mustCompile ? (amalgamate ? "moc_cpp" : "cpp") : "hpp");
    } else {
        artifact.filePath = input.Qt.core.generatedHeadersDir + '/'
                + input.completeBaseName + ".moc";
        artifact.fileTags.push("hpp");

        // If modules might be involved, provide split moc header files for inclusion
        // before and after the module start, respectively.
        if (product.cpp.forceUseCxxModules) {
            var includer = Object.assign({}, artifact);
            includer.filePath += ".h";
            var data = Object.assign({}, artifact);
            data.filePath = artifact.filePath + ".data";
            artifacts.push(includer, data);
        }
    }
    if (product.Qt.core._generateMetaTypesFile)
        artifacts.push({filePath: artifact.filePath + ".json", fileTags: "qt.core.metatypes.in"});
    return artifacts;
}

function findOutputArtifact(outputs, suffix)
{
    for (var tag in outputs) {
        var artifacts = outputs[tag];
        for (var i = 0; i < artifacts.length; ++i) {
            if (artifacts[i].filePath.endsWith(suffix))
                return artifacts[i];
        }
    }
    return undefined;
}

function splitMocFile(outputs)
{
    var mocArtifact = findOutputArtifact(outputs, ".moc");
    var includerArtifact = findOutputArtifact(outputs, ".h");
    var dataArtifact = findOutputArtifact(outputs, ".data");
    if (!mocArtifact || !includerArtifact || !dataArtifact)
        throw "splitMocFile: could not find expected .moc/.h/.data output artifacts";

    var inFile = new TextFile(mocArtifact.filePath, TextFile.ReadOnly);
    var content = inFile.readAll();
    inFile.close();

    var lines = content.split('\n');
    var splitIndex = -1;
    var depth = 0;
    var inCompatCheck = false;
    for (var i = 0; i < lines.length; ++i) {
        var line = lines[i].trim();
        if (!inCompatCheck) {
            if (/^#if\s*!defined\(Q_MOC_OUTPUT_REVISION\)/.test(line)) {
                inCompatCheck = true;
                depth = 1;
            }
            continue;
        }
        if (/^#(if|ifdef|ifndef)\b/.test(line))
            ++depth;
        else if (/^#endif\b/.test(line) && --depth === 0) {
            splitIndex = i;
            break;
        }
    }
    if (splitIndex === -1)
        throw "Could not locate the Q_MOC_OUTPUT_REVISION compatibility check in '"
                + mocArtifact.filePath + "'";

    var firstPart = lines.slice(0, splitIndex + 1).join('\n') + '\n';
    var secondPart = lines.slice(splitIndex + 1).join('\n');

    var includerFile = new TextFile(includerArtifact.filePath, TextFile.WriteOnly);
    includerFile.write(firstPart);
    includerFile.close();

    var dataFile = new TextFile(dataArtifact.filePath, TextFile.WriteOnly);
    dataFile.write(secondPart);
    dataFile.close();
}

function commands(project, product, inputs, outputs, input, output)
{
    var cmd = new Command(fullPath(product), args(product, input, outputs));
    cmd.description = 'moc ' + input.fileName;
    cmd.highlight = 'codegen';
    cmd.responseFileUsagePrefix = "@";

    if (product.cpp.forceUseCxxModules) {
        var splitCmd = new JavaScriptCommand();
        splitCmd.description = "splitting " + input.fileName;
        splitCmd.highlight = "codegen";
        splitCmd.sourceCode = function() {
            splitMocFile(outputs);
        };
        return [cmd, splitCmd];
    }

    return cmd;
}

function generateMocCppCommands(inputs, output)
{
    var cmd = new JavaScriptCommand();
    cmd.description = "creating " + output.fileName;
    cmd.highlight = "codegen";
    cmd.sourceCode = function() {
        ModUtils.mergeCFiles(inputs["moc_cpp"], output.filePath);
    };
    return [cmd];
}

function generateMetaTypesCommands(inputs, output)
{
    var inputFilePaths = inputs["qt.core.metatypes.in"].map(function(a) {
        return a.filePath;
    });
    var cmd = new Command(fullPath(product),
        ["--collect-json", "-o", output.filePath].concat(inputFilePaths));
    cmd.description = "generating " + output.fileName;
    cmd.highlight = "codegen";
    cmd.responseFileUsagePrefix = "@";
    return cmd;
}
