import qbs.File;
import qbs.FileInfo;
import qbs.TextFile;

ModuleProvider {
    property string someDefines
    relativeSearchPaths: {
        console.info("Running setup script for " + name);
        var moduleDir = FileInfo.joinPaths(outputBaseDir, "modules", "othergenerator");
        File.makePath(moduleDir);
        var module = new TextFile(FileInfo.joinPaths(moduleDir, "module.qbs"), TextFile.WriteOnly);
        module.writeLine("Module {");
        module.writeLine("    Depends { name: 'cpp' }");
        module.writeLine("    cpp.defines: 'MY_DEFINE=\"" + someDefines + "\"'");
        module.writeLine("}");
        module.close();
        return "";
    }
}
