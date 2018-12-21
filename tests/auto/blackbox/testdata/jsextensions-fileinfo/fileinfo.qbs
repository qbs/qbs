import qbs.FileInfo
import qbs.TextFile

Product {
    type: ["dummy"]
    property string messyPath: path + "/../" + FileInfo.fileName(path)
    Rule {
        multiplex: true
        outputFileTags: "dummy"
        prepare: {
            var cmd = new JavaScriptCommand();
            cmd.silent = true;
            cmd.sourceCode = function() {
                var output = new TextFile(FileInfo.joinPaths(product.sourceDirectory, "output.txt"),
                                          TextFile.WriteOnly);
                output.writeLine(FileInfo.baseName("/tmp/blubb.tar.gz"));
                output.writeLine(FileInfo.canonicalPath(product.messyPath));
                output.writeLine(FileInfo.cleanPath("/usr/local//../bin/"));
                output.writeLine(FileInfo.completeBaseName("/tmp/blubb.tar.gz"));
                output.writeLine(FileInfo.fileName("/tmp/blubb.tar.gz"));
                output.writeLine(FileInfo.fromWindowsSeparators("/tmp/blubb.tar.gz"));
                output.writeLine(FileInfo.fromWindowsSeparators("c:\\tmp\\blubb.tar.gz"));
                output.writeLine(FileInfo.isAbsolutePath("/tmp/blubb.tar.gz"));
                output.writeLine(FileInfo.isAbsolutePath("c:\\tmp\\blubb.tar.gz"));
                output.writeLine(FileInfo.isAbsolutePath("c:\\tmp\\blubb.tar.gz", ["unix"]));
                output.writeLine(FileInfo.isAbsolutePath("c:\\tmp\\blubb.tar.gz", ["windows"]));
                output.writeLine(FileInfo.isAbsolutePath("blubb.tar.gz"));
                output.writeLine(FileInfo.isAbsolutePath("../blubb.tar.gz"));
                output.writeLine(FileInfo.joinPaths("/", "tmp", "blubb.tar.gz"));
                output.writeLine(FileInfo.joinPaths("//", "/tmp/", "/blubb.tar.gz"));
                output.writeLine(FileInfo.path("/tmp/blubb.tar.gz"));
                output.writeLine(FileInfo.path("/tmp/"));
                output.writeLine(FileInfo.path("/"));
                output.writeLine(FileInfo.path("d:/"));
                output.writeLine(FileInfo.path("d:/", ["unix"]));
                output.writeLine(FileInfo.path("d:/", ["windows"]));
                output.writeLine(FileInfo.relativePath("/tmp", "/tmp/blubb.tar.gz"));
                output.writeLine(FileInfo.relativePath("/", "/tmp/blubb.tar.gz"));
                output.writeLine(FileInfo.relativePath("/tmp", "/blubb.tar.gz"));
                output.writeLine(FileInfo.toWindowsSeparators("/tmp/blubb.tar.gz"));
                output.writeLine(FileInfo.toWindowsSeparators("c:\\tmp\\blubb.tar.gz"));
                output.close();
            };
            return [cmd];
        }
    }
}
