import qbs.File
import qbs.FileInfo
import qbs.TextFile

Project
{
    Probe
    {
        id: projectBuilder
        property stringList refs: []
        property string sourceDir: sourceDirectory

        configure:
        {
            var tempDir = FileInfo.joinPaths(sourceDir, "temp");
            File.makePath(tempDir);
            var srcDir = FileInfo.joinPaths(sourceDir, "src");
            var projectDirs = File.directoryEntries(srcDir, File.Dirs | File.NoDotAndDotDot);
            var list = [];
            for (it in projectDirs) {
                var name = projectDirs[it];
                var productSrcDir = FileInfo.joinPaths(srcDir, name);
                var productFilePath = FileInfo.joinPaths(tempDir, name + ".qbs");
                var file = new TextFile(productFilePath, TextFile.WriteOnly);
                try {
                    file.writeLine("import qbs");
                    file.writeLine("CppApplication");
                    file.writeLine("{");
                    file.writeLine("\tfiles: [ \"" + productSrcDir + "/*.cpp\" ]");
                    file.writeLine("}");
                } finally {
                    file.close();
                }
                list.push(productFilePath);
            }
            found = true;
            refs = list;
        }
    }

    references: projectBuilder.refs
}
