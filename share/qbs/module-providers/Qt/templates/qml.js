var File = require("qbs.File");
var FileInfo = require("qbs.FileInfo");
var Process = require("qbs.Process");
var TextFile = require("qbs.TextFile");

function scannerData(scannerFilePath, qmlFiles, qmlPath, hostOS)
{
    var p;
    try {
        p = new Process();
        if (!hostOS.contains("windows")) {
            p.exec(scannerFilePath, ["-qmlFiles"].concat(qmlFiles).concat(["-importPath", qmlPath]),
                    true);
            return JSON.parse(p.readStdOut());
        }
        var data = [];
        var nextFileIndex = 0;
        while (nextFileIndex < qmlFiles.length) {
            var currentFileList = [];
            var currentFileListStringLength = 0;
            while (nextFileIndex < qmlFiles.length && currentFileListStringLength < 30000) {
                var currentFile = qmlFiles[nextFileIndex++];
                currentFileList.push(currentFile);
                currentFileListStringLength += currentFile.length;
            }
            p.exec(scannerFilePath, ["-qmlFiles"].concat(currentFileList)
                   .concat(["-importPath", qmlPath]), true);
            data = data.concat(JSON.parse(p.readStdOut()));
        }
        return data;
    } finally {
        if (p)
            p.close();
    }
}

function getPrlRhs(line)
{
    return line.split('=')[1].trim();
}

function getLibsForPlugin(pluginData, product)
{
    var targetOS = product.qbs.targetOS;
    var toolchain = product.qbs.toolchain;
    var buildVariant = product.Qt.core.qtBuildVariant;
    var qtLibDir = product.Qt.core.libPath;
    var qtPluginDir = product.Qt.core.pluginPath;
    var qtDir = product.Qt.core.installPrefixPath;
    var qtQmlPath = product.Qt.qml.qmlPath;

    if (!pluginData.path)
        return "";
    var prlFileName = "";
    if (!targetOS.contains("windows"))
        prlFileName += "lib";
    prlFileName += pluginData.plugin;
    if (buildVariant === "debug") {
        if (targetOS.contains("windows")) {
            prlFileName += "d";
        } else if (product.Qt.core.versionMajor >= 6 &&
                   (targetOS.contains("ios")
                    || targetOS.contains("tvos")
                    || targetOS.contains("watchos"))) {
            prlFileName += "_debug";
        }
    }
    prlFileName += ".prl";
    var prlFilePath = FileInfo.joinPaths(pluginData.path, prlFileName);
    if (!File.exists(prlFilePath)) {
        console.warn("prl file for QML plugin '" + pluginData.plugin + "' not present at '"
                     + prlFilePath + "'. Linking may fail.");
        return "";
    }
    var prlFile = new TextFile(prlFilePath, TextFile.ReadOnly);
    try {
        var pluginLib;
        var otherLibs = [];
        var line;
        while (!prlFile.atEof()) {
            line = prlFile.readLine().trim();
            if (!line)
                continue;
            if (line.startsWith("QMAKE_PRL_TARGET"))
                pluginLib = FileInfo.joinPaths(pluginData.path, getPrlRhs(line));
            if (line.startsWith("QMAKE_PRL_LIBS = ")) {
                var otherLibsLine = ' ' + getPrlRhs(line);
                if (toolchain.contains("msvc")) {
                    otherLibsLine = otherLibsLine.replace(/ -L/g, " /LIBPATH:");
                    otherLibsLine = otherLibsLine.replace(/-l([^ ]+)/g, "$1" + ".lib");
                }
                otherLibsLine = otherLibsLine.replace(/\$\$\[QT_INSTALL_LIBS\]/g, qtLibDir);
                otherLibsLine = otherLibsLine.replace(/\$\$\[QT_INSTALL_PLUGINS\]/g, qtPluginDir);
                otherLibsLine = otherLibsLine.replace(/\$\$\[QT_INSTALL_PREFIX\]/g, qtDir);
                otherLibsLine = otherLibsLine.replace(/\$\$\[QT_INSTALL_QML\]/g, qtQmlPath);
                otherLibs = otherLibs.concat(otherLibsLine.split(' ').map(FileInfo.cleanPath));
            }
        }
        if (!pluginLib)
            throw "Malformed prl file '" + prlFilePath + "'.";
        return [pluginLib].concat(otherLibs);
    } finally {
        prlFile.close();
    }
}

function typeRegistrarCommands(project, product, inputs, outputs, input, output,
                               explicitlyDependsOn)
{
    var versionParts = product.Qt.qml._importVersionParts;
    var args = [
        "--generate-qmltypes=" + outputs["qt.qml.types"][0].filePath,
        "--import-name=" + product.Qt.qml.importName,
        "--major-version=" + versionParts[0],
        "--minor-version=" + (versionParts.length > 1 ? versionParts[1] : "0")
    ];
    var foreignTypes = product.Qt.qml.extraMetaTypesFiles || [];
    var metaTypeArtifactsFromDeps = explicitlyDependsOn["qt.core.metatypes"] || [];
    var filePathFromArtifact = function(a) { return a.filePath; };
    foreignTypes = foreignTypes.concat(metaTypeArtifactsFromDeps.map(filePathFromArtifact));
    if (foreignTypes.length > 0)
        args.push("--foreign-types=" + foreignTypes.join(","));
    args.push("-o", outputs.cpp[0].filePath);
    args = args.concat(inputs["qt.core.metatypes"].map(filePathFromArtifact));
    var cmd = new Command(product.Qt.core.qmlLibExecPath + "/qmltyperegistrar", args);
    cmd.description = "running qmltyperegistrar";
    cmd.highlight = "codegen";
    return cmd;
}

function generatePluginImportOutputArtifacts(product, inputs)
{
    var list = [];
    if (inputs["qt.qml.qml"])
        list.push({ filePath: "qml_plugin_import.cpp", fileTags: ["cpp"] });
    list.push({
        filePath: product.Qt.core.qtBuildVariant === "debug"
                      ? product.Qt.qml.pluginListFilePathDebug
                      : product.Qt.qml.pluginListFilePathRelease,
        fileTags: ["qt.qml.pluginlist"]
    });
    return list;
}

function getCppLibsAndFrameworks(cpp)
{
    var libs = {"frameworks": {}, "libraries": {}};
    for (var i = 0; i < cpp.frameworks.length; ++i)
        libs.frameworks[cpp.frameworks[i]] = true;
    for (var i = 0; i < cpp.staticLibraries.length; ++i)
        libs.libraries[cpp.staticLibraries[i]] = true;
    for (var i = 0; i < cpp.dynamicLibraries.length; ++i)
        libs.libraries[cpp.dynamicLibraries[i]] = true;
    return libs;
}

function getUniqueLibs(libs, seen)
{
    var uniqueLibs = [];
    for (var i = 0; i < libs.length; ++i) {
        if (libs[i] == "-framework") {
            var frameworkName = libs[i + 1];
            if (frameworkName && !seen.frameworks[frameworkName]) {
                seen.frameworks[frameworkName] = true;
                uniqueLibs.push("-framework", frameworkName);
            }
            ++i; // Skip framework name
        } else {
            var libName;
            if (libs[i].startsWith("-l"))
                libName = libs[i].substr(2);
            else
                libName = libs[i];
            if (!seen.libraries[libName]) {
                seen.libraries[libName] = true;
                uniqueLibs.push(libs[i]);
            }
        }
    }

    return uniqueLibs;
}

function generatePluginImportCommands(project, product, inputs, outputs, input, output,
                                      explicitlyDependsOn)
{
    var cmd = new JavaScriptCommand();
    if (inputs["qt.qml.qml"])
        cmd.description = "creating " + outputs["cpp"][0].fileName;
    else
        cmd.silent = true;
    cmd.sourceCode = function() {
        var qmlInputs = inputs["qt.qml.qml"];
        if (!qmlInputs)
            qmlInputs = [];
        var scannerData = Qml.scannerData(product.Qt.qml.qmlImportScannerFilePath,
                qmlInputs.map(function(inp) { return inp.filePath; }),
                product.Qt.qml.qmlPath, Host.os());
        var cppFile;
        var listFile;
        try {
            if (qmlInputs.length > 0)
                cppFile = new TextFile(outputs["cpp"][0].filePath, TextFile.WriteOnly);
            listFile = new TextFile(outputs["qt.qml.pluginlist"][0].filePath,
                                    TextFile.WriteOnly);
            if (cppFile)
                cppFile.writeLine("#include <QtPlugin>");
            var plugins = { };
            var seen = Qml.getCppLibsAndFrameworks(product.cpp);
            var libsWithUniqueObjects = [];

            for (var p in scannerData) {
                var plugin = scannerData[p].plugin;
                if (!plugin || plugins[plugin])
                    continue;
                plugins[plugin] = true;
                var className = scannerData[p].classname;
                if (!className) {
                    throw "QML plugin '" + plugin + "' is missing a classname entry. " +
                          "Please add one to the qmldir file.";
                }
                if (cppFile)
                    cppFile.writeLine("Q_IMPORT_PLUGIN(" + className + ")");
                var libs = Qml.getLibsForPlugin(scannerData[p], product);
                var uniqueLibs = Qml.getUniqueLibs(libs, seen);
                libsWithUniqueObjects = libsWithUniqueObjects.concat(uniqueLibs);
            }
            listFile.write(libsWithUniqueObjects.join("\n"));
        } finally {
            if (cppFile)
                cppFile.close();
            if (listFile)
                listFile.close();
        };
    };
    return [cmd];
}
