/****************************************************************************
**
** Copyright (C) 2017 The Qt Company Ltd.
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

var FileInfo = require("qbs.FileInfo");
var Process = require("qbs.Process");
var Rcc = require("rcc.js");
var TextFile = require("qbs.TextFile");

function scanQrc(product, qrcFilePath) {
    var absInputDir = FileInfo.path(qrcFilePath);
    var result = [];
    var process = new Process();
    try {
        var rcc = FileInfo.joinPaths(Rcc.fullPath(product) + FileInfo.executableSuffix());
        var exitCode = process.exec(rcc, ["--list", qrcFilePath], true);
        for (;;) {
            var line = process.readLine();
            if (!line)
                break;
            line = line.trim();
            line = FileInfo.relativePath(absInputDir, line);
            result.push(line);
        }
    } finally {
        process.close();
    }
    return result;
}

function qtQuickCompilerOutputName(filePath) {
    var str = filePath.replace(/\//g, '_');
    var i = str.lastIndexOf('.');
    if (i != -1)
        str = str.substr(0, i) + '_' + str.substr(i + 1);
    str += ".cpp";
    return str;
}

function qtQuickResourceFileOutputName(fileName) {
    return fileName.replace(/\.qrc$/, "_qtquickcompiler.qrc");
}

function contentFromQrc(product, qrcFilePath) {
    var supportsFiltering = product.Qt.quick._supportsQmlJsFiltering;
    var filesInQrc = scanQrc(product, qrcFilePath);
    var qmlJsFiles = filesInQrc.filter(function (filePath) {
        return (/\.(mjs|js|qml)$/).test(filePath);
    } );
    var content = {};
    if (!supportsFiltering || filesInQrc.length - qmlJsFiles.length > 0) {
        content.newQrcFileName = qtQuickResourceFileOutputName(input.fileName);
    }
    content.qmlJsFiles = qmlJsFiles.map(function (filePath) {
        return {
            input: filePath,
            output: qtQuickCompilerOutputName(filePath)
        };
    });
    return content;
}

function generateCompilerInputCommands(product, input, output)
{
    var cmd = new JavaScriptCommand();
    cmd.silent = true;
    cmd.sourceCode = function() {
        var content = contentFromQrc(product, input.filePath);
        content.qrcFilePath = input.filePath;
        var tf = new TextFile(output.filePath, TextFile.WriteOnly);
        tf.write(JSON.stringify(content));
        tf.close();
    }
    return cmd;
}

function compilerOutputArtifacts(product, inputs)
{
    var infos = [];
    inputs["qt.quick.qrcinfo"].forEach(function (input) {
        var tf = new TextFile(input.filePath, TextFile.ReadOnly);
        infos.push(JSON.parse(tf.readAll()));
        tf.close();
    });

    var result = [];
    infos.forEach(function (info) {
        if (info.newQrcFileName) {
            result.push({
                filePath: info.newQrcFileName,
                fileTags: ["qrc"]
            });
        }
        info.qmlJsFiles.forEach(function (qmlJsFile) {
            result.push({
                filePath: qmlJsFile.output,
                fileTags: ["cpp"]
            });
        });
    });
    result.push({
        filePath: product.Qt.quick._generatedLoaderFileName,
        fileTags: ["cpp"]
    });
    return result;
}

function compilerCommands(project, product, inputs, outputs, input, output, explicitlyDependsOn)
{
    var infos = [];
    inputs["qt.quick.qrcinfo"].forEach(function (input) {
        var tf = new TextFile(input.filePath, TextFile.ReadOnly);
        infos.push(JSON.parse(tf.readAll()));
        tf.close();
    });

    var cmds = [];
    var qmlCompiler = product.Qt.quick.compilerFilePath;
    var useCacheGen = product.Qt.quick._compilerIsQmlCacheGen;
    var cmd;
    var loaderFlags = [];

    function findOutput(fileName) {
        for (var k in outputs) {
            for (var i in outputs[k]) {
                if (outputs[k][i].fileName === fileName)
                    return outputs[k][i];
            }
        }
        throw new Error("Qt Quick compiler rule: Cannot find output artifact "
                        + fileName + ".");
    }

    infos.forEach(function (info) {
        if (info.newQrcFileName) {
            loaderFlags.push("--resource-file-mapping="
                             + FileInfo.fileName(info.qrcFilePath)
                             + '=' + info.newQrcFileName);
            // Qt 5.15 doesn't really filter anyting but merely copies the qrc
            // content to the new location
            var args = ["--filter-resource-file",
                        info.qrcFilePath];
            if (useCacheGen)
                args.push("-o");
            args.push(findOutput(info.newQrcFileName).filePath);
            cmd = new Command(qmlCompiler, args);
            cmd.description = "generating " + info.newQrcFileName;
            cmds.push(cmd);
        } else {
            loaderFlags.push("--resource-file-mapping=" + info.qrcFilePath);
        }
        info.qmlJsFiles.forEach(function (qmlJsFile) {
            var args = ["--resource=" + info.qrcFilePath, qmlJsFile.input];
            if (useCacheGen)
                args.push("-o");
            args.push(findOutput(qmlJsFile.output).filePath);
            cmd = new Command(qmlCompiler, args);
            cmd.description = "generating " + qmlJsFile.output;
            cmd.workingDirectory = FileInfo.path(info.qrcFilePath);
            cmds.push(cmd);
        });
    });

    var args = loaderFlags.concat(infos.map(function (info) { return info.qrcFilePath; }));
    if (useCacheGen)
        args.push("-o");
    args.push(findOutput(product.Qt.quick._generatedLoaderFileName).filePath);
    cmd = new Command(qmlCompiler, args);
    cmd.description = "generating loader source";
    cmds.push(cmd);
    return cmds;
}
