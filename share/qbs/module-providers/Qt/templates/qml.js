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
