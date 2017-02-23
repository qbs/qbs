import qbs.File;
import qbs.FileInfo;
import qbs.TextFile;

ModuleProvider {
    property string chooseLettersFrom
    relativeSearchPaths: {
        console.info("Running setup script for " + name);
        var startAtBeginning = chooseLettersFrom === "beginning";
        var moduleBaseDir = FileInfo.joinPaths(outputBaseDir, "modules", "mygenerator");
        var module1Dir = FileInfo.joinPaths(moduleBaseDir, "module1");
        File.makePath(module1Dir);
        var module1 = new TextFile(FileInfo.joinPaths(module1Dir, "module1.qbs"), TextFile.WriteOnly);
        module1.writeLine("Module {");
        module1.writeLine("    Depends { name: 'cpp' }");
        module1.writeLine("    cpp.defines: 'LETTER1=" + (startAtBeginning ? "\\\'A\\\'" : "\\\'Z\\\'")
                          + "'");
        module1.writeLine("}");
        module1.close();
        var module2Dir = FileInfo.joinPaths(moduleBaseDir, "module2");
        File.makePath(module2Dir);
        var module2 = new TextFile(FileInfo.joinPaths(module2Dir, "module2.qbs"), TextFile.WriteOnly);
        module2.writeLine("Module {");
        module2.writeLine("    Depends { name: 'cpp' }");
        module2.writeLine("    cpp.defines: 'LETTER2=" + (startAtBeginning ? "\\\'B\\\'" : "\\\'Y\\\'")
                          + "'");
        module2.writeLine("}");
        module2.close();
        return "";
    }
}
