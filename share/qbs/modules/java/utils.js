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

var File = loadExtension("qbs.File");
var FileInfo = loadExtension("qbs.FileInfo");
var Process = loadExtension("qbs.Process");

function findJdkPath(hostOS, arch) {
    var i;
    var env = qbs.getEnv("JAVA_HOME");
    if (env) {
        return env;
    }

    if (hostOS.contains("windows")) {
        var keys = [
            "HKEY_LOCAL_MACHINE\\SOFTWARE\\JavaSoft\\Java Development Kit",
            "HKEY_LOCAL_MACHINE\\SOFTWARE\\Wow6432Node\\JavaSoft\\Java Development Kit"
        ];

        for (i in keys) {
            var current =  qbs.getNativeSetting(keys[i], "CurrentVersion"); // 1.8 etc.
            if (current) {
                var home = qbs.getNativeSetting([keys[i], current].join("\\"), "JavaHome");
                if (home) {
                    return home;
                }
            }
        }

        return undefined;
    }

    if (hostOS.contains("osx")) {
        var p = new Process();
        try {
            // We filter by architecture here so that we'll get a compatible JVM for JNI use.
            var args = [];
            if (arch) {
                args.push("--arch", arch === "x86" ? "i386" : arch);
            }

            // --failfast doesn't print the default JVM if nothing matches the filter(s).
            p.exec("/usr/libexec/java_home", args.concat(["--failfast"]), true);
            return p.readStdOut().trim();
        } catch (e) {
            return undefined;
        } finally {
            p.close();
        }
    }

    if (hostOS.contains("unix")) {
        var requiredTools = ["javac", "java", "jar"];
        var searchPaths = [
            "/usr/lib/jvm/default-java", // Debian/Ubuntu
            "/etc/alternatives/java_sdk_openjdk", // Fedora
            "/usr/lib/jvm/default" // Arch
        ];
        for (i = 0; i < searchPaths.length; ++i) {
            function fullToolPath(tool) {
                return FileInfo.joinPaths(searchPaths[i], "bin", tool);
            }

            if (requiredTools.map(fullToolPath).every(File.exists)) {
                return searchPaths[i];
            }
        }

        return undefined;
    }
}

function supportsGeneratedNativeHeaderFiles(product) {
    var compilerVersionMajor = ModUtils.moduleProperty(product, "compilerVersionMajor");
    if (compilerVersionMajor === 1) {
        if (ModUtils.moduleProperty(product, "compilerVersionMinor") >= 8) {
            return true;
        }
    }

    return compilerVersionMajor > 1;
}

function javacArguments(product, inputs) {
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
    if (supportsGeneratedNativeHeaderFiles(product))
        args.push("-h", product.buildDirectory);
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
    return args;
}

function outputArtifacts(product, inputs) {
    if (!File.exists(FileInfo.joinPaths(product.moduleProperty("qbs", "libexecPath"),
                                        "qbs-javac-scan.jar"))) {
        throw "Qbs was built without Java support. Rebuild Qbs with CONFIG+=qbs_enable_java " +
                "(qmake) or project.enableJava:true (self hosted build)";
    }

    // We need to ensure that the output directory is created first, because the Java compiler
    // internally checks that it is present before performing any actions
    File.makePath(ModUtils.moduleProperty(product, "classFilesDir"));

    var process;
    try {
        process = new Process();
        process.exec(product.moduleProperty("java", "interpreterFilePath"), ["-jar",
                              FileInfo.joinPaths(product.moduleProperty("qbs", "libexecPath"),
                                                 "qbs-javac-scan.jar"),
                              "--output-format", "json"]
                     .concat(javacArguments(product, inputs)), true);
        return JSON.parse(process.readStdOut());
    } finally {
        process.close();
    }
}
