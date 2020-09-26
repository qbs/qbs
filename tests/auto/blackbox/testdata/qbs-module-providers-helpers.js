var File = require("qbs.File");
var FileInfo = require("qbs.FileInfo");
var TextFile = require("qbs.TextFile");

function writeModule(outputBaseDir, name, prop) {
    console.info("Running setup script for " + name);
    var moduleDir = FileInfo.joinPaths(outputBaseDir, "modules", name);
    File.makePath(moduleDir);
    var module = new TextFile(FileInfo.joinPaths(moduleDir, "module.qbs"), TextFile.WriteOnly);
    module.writeLine("Module {");
    module.writeLine("    property string prop: '" + prop + "'");
    module.writeLine("}");
    module.close();
}
