var File = require("qbs.File");
var FileInfo = require("qbs.FileInfo");
var TextFile = require("qbs.TextFile");
var ModUtils = require("qbs.ModUtils");

function writeModule(outputBaseDir, name, prop, listProp, boolProp) {
    console.info("Running setup script for " + name);
    var moduleDir = FileInfo.joinPaths(outputBaseDir, "modules", name);
    File.makePath(moduleDir);
    var module = new TextFile(FileInfo.joinPaths(moduleDir, "module.qbs"), TextFile.WriteOnly);
    module.writeLine("Module {");
    module.writeLine("    property string prop: " + ModUtils.toJSLiteral(prop));
    if (listProp) {
        module.writeLine("    property stringList listProp: "
            + ModUtils.toJSLiteral(listProp));
    }
    if (boolProp) {
        module.writeLine("    property bool boolProp: "
            + ModUtils.toJSLiteral(boolProp));
    }
    module.writeLine("}");
    module.close();
}
