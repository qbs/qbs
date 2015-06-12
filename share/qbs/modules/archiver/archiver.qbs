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

Module {
    property string type
    property string archiveBaseName: product.targetName
    property string workingDirectory
    property stringList flags: []
    property path outputDirectory: product.destinationDirectory
    property string archiveExtension: {
        if (type === "7zip")
            return "7z";
        if (type == "tar") {
            var extension = "tar";
            if (compressionType !== "none")
                extension += "." + compressionType;
            return extension;
        }
        return undefined;
    }
    property string command: {
        if (type === "7zip")
            return "7z";
        if (type === "tar")
            return "tar";
        return undefined;
    }

    property string compressionLevel
    property string compressionType: "gz"

    PropertyOptions {
        name: "type"
        description: "The type of archive to create."
        allowedValues: ["7zip", "tar"]
    }

    PropertyOptions {
        name: "compressionLevel"
        description: "How much effort to put into compression.\n"
            + "Higher numbers mean smaller archive files at the cost of taking more time.\n"
            + "This property is only used for the '7zip' type."
        allowedValues: [undefined, "0", "1", "3", "5", "7", "9"]
    }

    PropertyOptions {
        name: "compressionType"
        description: "The compression format to use. The respective tool needs to be present.\n"
            + "This property is only used for the 'tar' type."
        allowedValues: ["none", "gz", "bz2", "Z", "xz"]
    }

    Rule {
        inputs: ["archiver.input-list"]

        Artifact {
            filePath: FileInfo.joinPaths(product.moduleProperty("archiver", "outputDirectory"),
                              product.moduleProperty("archiver", "archiveBaseName") + '.'
                                         + product.moduleProperty("archiver", "archiveExtension"));
            fileTags: ["archiver.archive"]
        }

        prepare: {
            var binary = product.moduleProperty("archiver", "command");
            var args = [];
            var commands = [];
            var type = product.moduleProperty("archiver", "type");
            if (type === "7zip") {
                var rmCommand = new JavaScriptCommand();
                rmCommand.silent = true;
                rmCommand.sourceCode = function() {
                    if (File.exists(output.filePath))
                        File.remove(output.filePath);
                };
                commands.push(rmCommand);
                args.push("a", "-y", "-mmt=on");
                var compressionLevel = product.moduleProperty("archiver", "compressionLevel");
                if (compressionLevel)
                    args.push("-mx" + compressionLevel);
                args = args.concat(product.moduleProperty("archiver", "flags"));
                args.push(output.filePath);
                args.push("@" + input.filePath);
            } else if (type === "tar") {
                args.push("-c");
                var compression = product.moduleProperty("tar", "compressiontype");
                if (compression === "gz")
                    args.push("-z");
                else if (compression === "bz2")
                    args.push("-j");
                else if (compression === "Z")
                    args.push("-Z");
                else if (compression === "xz")
                    args.push("-J");
                args.push("-f", output.filePath, "-T", input.filePath);
                args = args.concat(product.moduleProperty("archiver", "flags"));
            }
            var archiverCommand = new Command(binary, args);
            archiverCommand.description = "Creating archive file " + output.fileName;
            archiverCommand.highlight = "linker";
            archiverCommand.workingDirectory
                    = product.moduleProperty("archiver", "workingDirectory");
            commands.push(archiverCommand);
            return commands;
        }
    }
}
