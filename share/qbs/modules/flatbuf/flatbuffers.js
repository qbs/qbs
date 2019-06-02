/****************************************************************************
**
** Copyright (C) 2024 Ivan Komissarov (abbapoh@gmail.com)
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

var File = require("qbs.File");

function validateCompiler(compilerName, compilerPath) {
    if (!File.exists(compilerPath)) {
        throw "Can't find '" + compilerName + "' binary. Please set the compilerPath property or "
                +  "make sure the compiler is found in PATH";
    }
}

function getOutputDir(module, input)  {
    var outputDir = module._outputDir;
    if (!module.keepPrefix)
        return outputDir;
    var importPaths = module.importPaths;
    if (importPaths !== undefined && importPaths.length !== 0) {
        var canonicalInput = File.canonicalFilePath(FileInfo.path(input.filePath));
        for (var i = 0; i < importPaths.length; ++i) {
            var path = File.canonicalFilePath(importPaths[i]);

            if (canonicalInput.startsWith(path)) {
                return outputDir + "/" + FileInfo.relativePath(path, canonicalInput);
            }
        }
    }
    return outputDir;
}

function artifactC(module, input, tag, suffix) {
    var outputDir = module._outputDir;
    return {
        fileTags: [tag],
        filePath: outputDir + "/" + FileInfo.baseName(input.fileName) + suffix,
        cpp: { warningLevel: "none"}
    };
}

function doPrepareC(module, input)
{
    var args = [];
    args.push("-a") // write all
    args.push("-o", input.flatbuf.c._outputDir) // output dir

    var importPaths = module.importPaths;
    importPaths.forEach(function(path) {
        args.push("-I", path);
    });

    args.push(input.filePath);

    var cmd = new Command(module.compilerPath, args);
    cmd.workingDirectory = FileInfo.path(module._outputDir)
    cmd.highlight = "codegen";
    cmd.description = "generating C files for " + input.fileName;
    return [cmd];
}

function artifact(module, input, tag, suffix) {
    var outputDir = getOutputDir(module, input);
    return {
        fileTags: [tag],
        filePath: outputDir + "/" + FileInfo.baseName(input.fileName) + suffix,
        cpp: { warningLevel: "none" }
    };
}

function doPrepareCpp(module, input, output)
{
    var outputDir = FileInfo.path(output.filePath);
    var result = [];

    var args = [];
    args.push("--cpp")

    var importPaths = module.importPaths;
    importPaths.forEach(function(path) {
        args.push("-I", path);
    });

    args.push("--filename-ext", module.filenameExtension);
    args.push("--filename-suffix", module.filenameSuffix);

    if (module.includePrefix)
        args.push("--include-prefix", module.includePrefix);

    if (module.keepPrefix)
        args.push("--keep-prefix");

    args.push(input.filePath);
    var cmd = new Command(input.flatbuf.cpp.compilerPath, args);
    cmd.workingDirectory = outputDir;
    cmd.highlight = "codegen";
    cmd.description = "generating C++ files for " + input.fileName;
    result.push(cmd);

    return result;
}
