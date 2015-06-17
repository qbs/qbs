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
import qbs.FileInfo
import qbs.ModUtils
import qbs.Process

import "utils.js" as JavaUtils

Module {
    property stringList additionalClassPaths
    property stringList additionalCompilerFlags
    property stringList additionalJarFlags
    property stringList bootClassPaths
    property string compilerFilePath: FileInfo.joinPaths(jdkPath, compilerName)
    property string compilerName: "javac"
    property bool enableWarnings: true
    property string interpreterFilePath : FileInfo.joinPaths(jdkPath, interpreterName)
    property string interpreterName: "java"
    property string jarFilePath: FileInfo.joinPaths(jdkPath, jarName)
    property string jarName: "jar"
    property path jdkPath

    property string compilerVersion: rawCompilerVersion ? rawCompilerVersion[1] : undefined
    property var compilerVersionParts: compilerVersion ? compilerVersion.split(/[\._]/).map(function(item) { return parseInt(item, 10); }) : []
    property int compilerVersionMajor: compilerVersionParts[0]
    property int compilerVersionMinor: compilerVersionParts[1]
    property int compilerVersionPatch: compilerVersionParts[2]
    property int compilerVersionUpdate: compilerVersionParts[3]

    property string languageVersion
    PropertyOptions {
        name: "languageVersion"
        description: "Java language version to interpret source code as"
    }

    property string runtimeVersion
    PropertyOptions {
        name: "runtimeVersion"
        description: "version of the Java runtime to generate compatible bytecode for"
    }

    property bool warningsAsErrors: false

    // Internal properties
    property path classFilesDir: FileInfo.joinPaths(product.buildDirectory, "classFiles")

    // private properties
    readonly property var rawCompilerVersion: {
        var p = new Process();
        try {
            p.exec(compilerFilePath, ["-version"]);
            var re = /^javac (([0-9]+(?:\.[0-9]+){2,2})_([0-9]+))$/m;
            var match = p.readStdErr().trim().match(re);
            if (match !== null)
                return match;
        } finally {
            p.close();
        }
    }

    validate: {
        var validator = new ModUtils.PropertyValidator("java");
        validator.setRequiredProperty("compilerVersion", compilerVersion);
        validator.setRequiredProperty("compilerVersionParts", compilerVersionParts);
        validator.setRequiredProperty("compilerVersionMajor", compilerVersionMajor);
        validator.setRequiredProperty("compilerVersionMinor", compilerVersionMinor);
        validator.setRequiredProperty("compilerVersionUpdate", compilerVersionUpdate);
        validator.addVersionValidator("compilerVersion", compilerVersion
                                      ? compilerVersion.replace("_", ".") : undefined, 4, 4);
        validator.addRangeValidator("compilerVersionMajor", compilerVersionMajor, 1);
        validator.addRangeValidator("compilerVersionMinor", compilerVersionMinor, 0);
        validator.addRangeValidator("compilerVersionPatch", compilerVersionPatch, 0);
        validator.addRangeValidator("compilerVersionUpdate", compilerVersionUpdate, 0);
        validator.validate();
    }

    FileTagger {
        patterns: "*.java"
        fileTags: ["java.java"]
    }

    Rule {
        multiplex: true
        inputs: ["java.java"]
        inputsFromDependencies: ["java.jar"]
        outputFileTags: ["java.class", "hpp"] // Annotations can produce additional java source files. Ignored for now.
        outputArtifacts: {
            return JavaUtils.outputArtifacts(product, inputs);
        }
        prepare: {
            var cmd = new Command(ModUtils.moduleProperty(product, "compilerFilePath"),
                                  JavaUtils.javacArguments(product, inputs));
            cmd.description = "Compiling Java sources";
            cmd.highlight = "compiler";
            return [cmd];
        }
    }

    Rule {
        inputs: ["java.class"]
        multiplex: true

        Artifact {
            fileTags: ["java.jar"]
            filePath: FileInfo.joinPaths(product.destinationDirectory, product.targetName + ".jar")
        }

        prepare: {
            var i;
            var flags = "cf";
            var args = [output.filePath];

            var manifestFile = ModUtils.moduleProperty(product, "manifest");
            if (manifestFile) {
                flags += "m";
                args.push(manifestFile);
            }

            var entryPoint = ModUtils.moduleProperty(product, "entryPoint");
            var entryPoint = product.entryPoint;
            if (entryPoint) {
                flags += "e";
                args.push(entryPoint);
            }

            args.unshift(flags);

            var otherFlags = ModUtils.moduleProperty(product, "additionalJarFlags");
            if (otherFlags)
                args = args.concat(otherFlags);

            for (i in inputs["java.class"])
                args.push(FileInfo.relativePath(ModUtils.moduleProperty(product, "classFilesDir"),
                                                inputs["java.class"][i].filePath));
            var cmd = new Command(ModUtils.moduleProperty(product, "jarFilePath"), args);
            cmd.workingDirectory = ModUtils.moduleProperty(product, "classFilesDir");
            cmd.description = "building " + FileInfo.fileName(output.fileName);
            cmd.highlight = "linker";
            return cmd;
        }
    }
}
