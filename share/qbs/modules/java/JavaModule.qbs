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

    FileTagger {
        patterns: "*.java"
        fileTags: ["java.java"]
    }

    Rule {
        multiplex: true
        inputs: ["java.java"]
        inputsFromDependencies: ["java.jar"]
        outputFileTags: ["java.class"] // Annotations can produce additional java source files. Ignored for now.
        outputArtifacts: {
            // Note: We'd have to duplicate some javac functionality to catch all outputs
            //       (e.g. private classes), so ignore these for now.
            var oFilePaths = [];
            // Extract package name to find out where the class name will be
            for (var i = 0; i < inputs["java.java"].length; ++i) {
                var inp = inputs["java.java"][i];
                var packageName = JavaUtils.extractPackageName(inp.filePath);
                var oFileName = inp.completeBaseName + ".class";
                var oFilePath = FileInfo.joinPaths(ModUtils.moduleProperty(product, "classFilesDir"),
                                                   packageName.split('.').join('/'), oFileName);
                oFilePaths.push({ filePath: oFilePath, fileTags: ["java.class"] });
            }
            return oFilePaths;
        }
        prepare: {
            var i;
            var outputDir = ModUtils.moduleProperty(product, "classFilesDir");
            var classPaths = [outputDir];
            var additionalClassPaths = ModUtils.moduleProperties(product, "additionalClassPaths");
            if (additionalClassPaths)
                classPaths = classPaths.concat(additionalClassPaths);
            for (i in inputs["java.jar"])
                classPaths.push(inputs["java.jar"][i].filePath);
            var debugArg = product.moduleProperty("qbs", "buildVariant") === "debug"
                    ? "-g" : "-g:none";
            var args = [
                    "-classpath", classPaths.join(product.moduleProperty("qbs", "pathListSeparator")),
                    "-s", product.buildDirectory,
                    debugArg, "-d", outputDir
                ];
            var runtimeVersion = ModUtils.moduleProperty(product, "runtimeVersion");
            if (runtimeVersion)
                args.push("-target", runtimeVersion);
            var languageVersion = ModUtils.moduleProperty(product, "languageVersion");
            if (languageVersion)
                args.push("-source", languageVersion);
            var bootClassPaths = ModUtils.moduleProperties(product, "bootClassPaths");
            if (bootClassPaths && bootClassPaths.length > 0)
                args.push("-bootclasspath", bootClassPaths.join(';'));
            if (!ModUtils.moduleProperty(product, "enableWarnings"))
                args.push("-nowarn");
            if (ModUtils.moduleProperty(product, "warningsAsErrors"))
                args.push("-Werror");
            var otherFlags = ModUtils.moduleProperty(product, "additionalCompilerFlags")
            if (otherFlags)
                args = args.concat(otherFlags);
            for (i = 0; i < inputs["java.java"].length; ++i)
                args.push(inputs["java.java"][i].filePath);
            var cmd = new Command(ModUtils.moduleProperty(product, "compilerFilePath"), args);
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
