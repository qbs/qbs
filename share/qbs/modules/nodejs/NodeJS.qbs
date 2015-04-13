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

Module {
    // JavaScript files which have been "processed" - currently this simply means "copied to output
    // directory" but might later include minification and obfuscation processing
    additionalProductTypes: ["nodejs_processed_js"].concat(applicationFile ? ["application"] : [])

    property path applicationFile
    PropertyOptions {
        name: "applicationFile"
        description: "file whose corresponding output will be executed when running the Node.js app"
    }

    setupRunEnvironment: {
        var v = new ModUtils.EnvironmentVariable("NODE_PATH", qbs.pathListSeparator, qbs.hostOS.contains("windows"));
        v.prepend(FileInfo.path(getEnv("QBS_RUN_FILE_PATH")));
        v.set();
    }

    FileTagger {
        patterns: ["*.js"]
        fileTags: ["js"]
    }

    Rule {
        inputs: ["js"]

        outputArtifacts: {
            var tags = ["nodejs_processed_js"];
            if (input.fileTags.contains("application_js") ||
                product.moduleProperty("nodejs", "applicationFile") === input.filePath)
                tags.push("application");

            return [{
                filePath: product.destinationDirectory + '/' + input.fileName,
                fileTags: tags
            }];
        }

        outputFileTags: ["nodejs_processed_js", "application"]

        prepare: {
            var cmd = new JavaScriptCommand();
            cmd.description = "copying " + input.fileName;
            cmd.sourceCode = function() {
                File.copy(input.filePath, output.filePath);
            };
            return cmd;
        }
    }
}
