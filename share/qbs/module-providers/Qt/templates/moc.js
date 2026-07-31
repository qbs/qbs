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
var TextFile = require("qbs.TextFile");
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

function mocInformation(input, product) {
    var info = input.qbsScanners && input.qbsScanners["Qt.core.moc"];
    if (!info)
        return { hasQObjectMacro: false, hasPluginMetaDataMacro: false, mustCompile: false };
    var hasQObjectMacro = !!info.hasQObjectMacro;
    var hasPluginMetaDataMacro = !!info.hasPluginMetaDataMacro;
    var module = info.partOfModule;
    var mustCompile = !!module; // Module units always need their companion compiled.
    if (!mustCompile && hasQObjectMacro && input.fileTags.includes("hpp")) {
        var included = Utilities.includedMocCppBaseNames(input, product);
        mustCompile = included.indexOf(input.completeBaseName) === -1;
    }
    return {
        hasQObjectMacro: hasQObjectMacro,
        hasPluginMetaDataMacro: hasPluginMetaDataMacro,
        mustCompile: mustCompile,
        module: module,
        includes: info.includes || [],
    };
}

function outputArtifacts(project, product, inputs, input)
{
    var mocInfo = mocInformation(input, product);
    if (!mocInfo.hasQObjectMacro)
        return [];
    var artifact = { fileTags: ["unmocable"] };
    var artifacts = [artifact];
    if (mocInfo.hasPluginMetaDataMacro)
        artifact.explicitlyDependsOn = ["qt_plugin_metadata"];
    if (input.fileTags.contains("hpp") || mocInfo.module) {
        artifact.filePath = input.Qt.core.generatedHeadersDir
                + "/moc_" + input.completeBaseName + ".cpp";
        var amalgamate = input.Qt.core.combineMocOutput;
        artifact.fileTags.push(mocInfo.mustCompile ? (amalgamate ? "moc_cpp" : "cpp") : "hpp");
    } else {
        artifact.filePath = input.Qt.core.generatedHeadersDir + '/'
                + input.completeBaseName + ".moc";
        artifact.fileTags.push("hpp");
    }
    if (product.Qt.core._generateMetaTypesFile)
        artifacts.push({filePath: artifact.filePath + ".json", fileTags: "qt.core.metatypes.in"});
    return artifacts;
}

// For moc < 6.13, we inject the module boilerplate ourselves.
function backportModuleFragmentCommand(input, output, moduleName, moduleIncludes)
{
    var cmd = new JavaScriptCommand();
    cmd.description = "patching module fragment into " + output.fileName;
    cmd.highlight = "codegen";
    cmd.moduleName = moduleName;
    cmd.moduleIncludes = moduleIncludes;
    cmd.sourceCode = function() {
        var file = new TextFile(output.filePath, TextFile.ReadWrite);
        var content = file.readAll();

        var lines = content.split('\n');

        // moc's actual content starts right after its banner comment.
        var bannerCloseIndex = -1;
        for (var i = 0; i < lines.length; ++i) {
            if (/^\*+\/$/.test(lines[i])) {
                bannerCloseIndex = i;
                break;
            }
        }
        if (bannerCloseIndex === -1) {
            throw "Failed to patch a module fragment into moc's output for '"
                    + input.filePath + "': could not find the expected banner comment in '"
                    + output.filePath + "'.";
        }
        // Skip the banner's closing line and the blank line that always follows it.
        var contentStartIndex = bannerCloseIndex + 2;

        var revisionGuardLine = "#if !defined(Q_MOC_OUTPUT_REVISION)";
        var revisionGuardIndex = lines.indexOf(revisionGuardLine, contentStartIndex);
        if (revisionGuardIndex === -1) {
            throw "Failed to patch a module fragment into moc's output for '"
                    + input.filePath + "': could not find the expected '"
                    + revisionGuardLine + "' line in '" + output.filePath + "'.";
        }

        // Depending on the input file's extension, old moc may or may not have written a
        // plain self-include of the original file (it decides this the same way it would for
        // an ordinary, non-module .h/.cpp file, having no notion of modules at all); either
        // way, that doesn't work for a .cppm, so strip it if present.
        var selfInclude = '#include "' + input.filePath + '"';
        var selfIncludeIndex = lines.indexOf(selfInclude, contentStartIndex);
        if (selfIncludeIndex !== -1 && selfIncludeIndex < revisionGuardIndex) {
            lines.splice(selfIncludeIndex, 1);
            revisionGuardIndex -= 1;
        }

        var preamble = ["module;", ""];
        var includes = moduleIncludes || [];
        for (i = 0; i < includes.length; ++i)
            preamble.push('#include "' + includes[i] + '"');
        lines.splice.apply(lines, [contentStartIndex, 0].concat(preamble));
        revisionGuardIndex += preamble.length;

        var moduleAndPartition = moduleName.split(':');
        var moduleDecl = ["", "module " + moduleAndPartition[0] + ";"];
        if (moduleAndPartition.length > 1)
            moduleDecl.push("import :" + moduleAndPartition[1] + ";");
        moduleDecl.push("");
        lines.splice.apply(lines, [revisionGuardIndex, 0].concat(moduleDecl));

        file.truncate();
        file.write(lines.join('\n'));
        file.close();
    };
    return cmd;
}

function commands(project, product, inputs, outputs, input, output)
{
    var cmd = new Command(fullPath(product), args(product, input, outputs));
    cmd.description = 'moc ' + input.fileName;
    cmd.highlight = 'codegen';
    cmd.responseFileUsagePrefix = "@";

    var cmds = [cmd];

    if (Utilities.versionCompare(product.Qt.core.version, "6.13") < 0) {
        var mocInfo = mocInformation(input, product);
        if (mocInfo.module) {
            cmds.push(backportModuleFragmentCommand(input, output, mocInfo.module,
                                                    mocInfo.includes));
        }
    }

    return cmds;
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
