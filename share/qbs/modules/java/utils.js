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
var ModUtils = loadExtension("qbs.ModUtils");
var Process = loadExtension("qbs.Process");

function findJdkPath(hostOS, arch, environmentPaths, searchPaths) {
    var i;
    for (var key in environmentPaths) {
        if (environmentPaths[key]) {
            return environmentPaths[key];
        }
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
            var status = p.exec("/usr/libexec/java_home", args.concat(["--failfast"]));
            return status === 0 ? p.readStdOut().trim() : undefined;
        } finally {
            p.close();
        }
    }

    if (hostOS.contains("unix")) {
        var requiredTools = ["javac", "java", "jar"];
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

function findJdkVersion(compilerFilePath) {
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

function supportsGeneratedNativeHeaderFiles(product) {
    var compilerVersionMajor = ModUtils.moduleProperty(product, "compilerVersionMajor");
    if (compilerVersionMajor === 1) {
        if (ModUtils.moduleProperty(product, "compilerVersionMinor") >= 8) {
            return true;
        }
    }

    return compilerVersionMajor > 1;
}

function javacArguments(product, inputs, overrides) {
    function getModuleProperty(product, propertyName, overrides) {
        if (overrides && overrides[propertyName])
            return overrides[propertyName];
        return ModUtils.moduleProperty(product, propertyName);
    }

    function getModuleProperties(product, propertyName, overrides) {
        if (overrides && overrides[propertyName])
            return overrides[propertyName];
        return ModUtils.moduleProperties(product, propertyName);
    }

    var i;
    var outputDir = getModuleProperty(product, "classFilesDir", overrides);
    var classPaths = [outputDir];
    var additionalClassPaths = getModuleProperty(product, "additionalClassPaths", overrides);
    if (additionalClassPaths)
        classPaths = classPaths.concat(additionalClassPaths);
    for (i in inputs["java.jar"])
        classPaths.push(inputs["java.jar"][i].filePath);
    var debugArg = product.moduleProperty("qbs", "buildVariant") === "debug"
            ? "-g" : "-g:none";
    var pathListSeparator = product.moduleProperty("qbs", "pathListSeparator");
    var args = [
            "-classpath", classPaths.join(pathListSeparator),
            "-s", product.buildDirectory,
            debugArg, "-d", outputDir
        ];
    if (supportsGeneratedNativeHeaderFiles(product))
        args.push("-h", product.buildDirectory);
    var runtimeVersion = getModuleProperty(product, "runtimeVersion", overrides);
    if (runtimeVersion)
        args.push("-target", runtimeVersion);
    var languageVersion = getModuleProperty(product, "languageVersion", overrides);
    if (languageVersion)
        args.push("-source", languageVersion);
    var bootClassPaths = getModuleProperties(product, "bootClassPaths", overrides);
    if (bootClassPaths && bootClassPaths.length > 0)
        args.push("-bootclasspath", bootClassPaths.join(pathListSeparator));
    if (!getModuleProperty(product, "enableWarnings", overrides))
        args.push("-nowarn");
    if (getModuleProperty(product, "warningsAsErrors", overrides))
        args.push("-Werror");
    var otherFlags = getModuleProperty(product, "additionalCompilerFlags", overrides);
    if (otherFlags)
        args = args.concat(otherFlags);
    for (i in inputs["java.java"])
        args.push(inputs["java.java"][i].filePath);
    for (i in inputs["java.java-internal"])
        args.push(inputs["java.java-internal"][i].filePath);
    return args;
}

/**
  * Returns a list of fully qualified Java class names for the compiler helper tool.
  *
  * @param type @c java to return names of sources, @c to return names of compiled classes
  */
function helperFullyQualifiedNames(type) {
    var names = [
        "io/qt/qbs/Artifact",
        "io/qt/qbs/ArtifactListJsonWriter",
        "io/qt/qbs/ArtifactListTextWriter",
        "io/qt/qbs/ArtifactListWriter",
        "io/qt/qbs/ArtifactListXmlWriter",
        "io/qt/qbs/tools/JavaCompilerScannerTool",
        "io/qt/qbs/tools/utils/JavaCompilerOptions",
        "io/qt/qbs/tools/utils/JavaCompilerScanner",
        "io/qt/qbs/tools/utils/JavaCompilerScanner$1",
        "io/qt/qbs/tools/utils/NullFileObject",
        "io/qt/qbs/tools/utils/NullFileObject$1",
        "io/qt/qbs/tools/utils/NullFileObject$2",
        "io/qt/qbs/tools/utils/NullFileObject$3",
        "io/qt/qbs/tools/utils/NullFileObject$4"
    ];
    if (type === "java") {
        return names.filter(function (name) {
            return !name.contains("$");
        });
    } else if (type === "class") {
        return names;
    }
}

function helperOutputArtifacts(product) {
    return helperFullyQualifiedNames("class").map(function (name) {
        return {
            filePath: FileInfo.joinPaths(ModUtils.moduleProperty(product, "internalClassFilesDir"),
                                         name + ".class"),
            fileTags: ["java.class-internal"]
        };
    });
}

function helperOverrideArgs(product, tool) {
    var overrides = {};
    if (tool === "javac") {
        // Build the helper tool with the same source and target version as the JDK it's being
        // compiled with. Both are irrelevant here since the resulting tool will only be run
        // with the same JDK as it was built with, and we know in advance the source is
        // compatible with all Java language versions from 1.6 and above.
        var jdkVersion = [ModUtils.moduleProperty(product, "compilerVersionMajor"),
                          ModUtils.moduleProperty(product, "compilerVersionMinor")].join(".");
        overrides["languageVersion"] = jdkVersion;
        overrides["runtimeVersion"] = jdkVersion;

        // Build the helper tool's class files separately from the actual product's class files
        overrides["classFilesDir"] = ModUtils.moduleProperty(product, "internalClassFilesDir");
    }

    // Inject the current JDK's runtime classes into the boot class path when building/running the
    // dependency scanner. This is normally not necessary but is important for Android platforms
    // where android.jar is the only JAR on the boot classpath and JSR 199 is unavailable.
    overrides["bootClassPaths"] = [ModUtils.moduleProperty(product, "runtimeJarPath")].concat(
                ModUtils.moduleProperties(product, "bootClassPaths"));
    return overrides;
}

function outputArtifacts(product, inputs) {
    // Handle the case where a product depends on Java but has no Java sources
    if (!inputs["java.java"] || inputs["java.java"].length === 0)
        return [];

    // We need to ensure that the output directory is created first, because the Java compiler
    // internally checks that it is present before performing any actions
    File.makePath(ModUtils.moduleProperty(product, "classFilesDir"));

    var process;
    try {
        process = new Process();
        process.setWorkingDirectory(
                    FileInfo.joinPaths(ModUtils.moduleProperty(product, "internalClassFilesDir")));
        process.exec(ModUtils.moduleProperty(product, "interpreterFilePath"),
                     ["io/qt/qbs/tools/JavaCompilerScannerTool", "--output-format", "json"]
                     .concat(javacArguments(product, inputs, helperOverrideArgs(product))), true);
        return JSON.parse(process.readStdOut());
    } finally {
        if (process)
            process.close();
    }
}

function manifestContents(filePath) {
    if (filePath === undefined)
        return undefined;

    var contents, file;
    try {
        file = new TextFile(filePath);
        contents = file.readAll();
    } finally {
        if (file) {
            file.close();
        }
    }

    if (contents) {
        var dict = {};
        var lines = contents.split(/[\r\n]/g);
        for (var i in lines) {
            var kv = lines[i].split(":");
            if (kv.length !== 2)
                return undefined;
            dict[kv[0]] = kv[1];
        }

        return dict;
    }
}
