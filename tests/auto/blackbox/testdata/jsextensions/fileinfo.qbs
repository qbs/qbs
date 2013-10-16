import qbs
import qbs.FileInfo
import qbs.TextFile

Product {
    type: {
        var output = new TextFile("output.txt", TextFile.WriteOnly);
        output.writeLine(FileInfo.baseName("/tmp/blubb.tar.gz"));
        output.writeLine(FileInfo.completeBaseName("/tmp/blubb.tar.gz"));
        output.writeLine(FileInfo.fileName("/tmp/blubb.tar.gz"));
        output.writeLine(FileInfo.fromWindowsSeparators("/tmp/blubb.tar.gz"));
        output.writeLine(FileInfo.fromWindowsSeparators("c:\\tmp\\blubb.tar.gz"));
        output.writeLine(FileInfo.isAbsolutePath("/tmp/blubb.tar.gz"));
        output.writeLine(FileInfo.isAbsolutePath("c:\\tmp\\blubb.tar.gz"));
        output.writeLine(FileInfo.isAbsolutePath("blubb.tar.gz"));
        output.writeLine(FileInfo.isAbsolutePath("../blubb.tar.gz"));
        output.writeLine(FileInfo.joinPaths("/", "tmp", "blubb.tar.gz"));
        output.writeLine(FileInfo.path("/tmp/blubb.tar.gz"));
        output.writeLine(FileInfo.path("/tmp/"));
        output.writeLine(FileInfo.path("/"));
        output.writeLine(FileInfo.path("d:/"));
        output.writeLine(FileInfo.relativePath("/tmp", "/tmp/blubb.tar.gz"));
        output.writeLine(FileInfo.relativePath("/", "/tmp/blubb.tar.gz"));
        output.writeLine(FileInfo.relativePath("/tmp", "/blubb.tar.gz"));
        output.writeLine(FileInfo.toWindowsSeparators("/tmp/blubb.tar.gz"));
        output.writeLine(FileInfo.toWindowsSeparators("c:\\tmp\\blubb.tar.gz"));
        output.close();
        return "wattweeßick";
    }
}
