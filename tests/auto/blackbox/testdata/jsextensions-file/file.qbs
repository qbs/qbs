import qbs.File
import qbs.FileInfo
import qbs.TextFile

Product {
    type: ["dummy"]
    Rule {
        multiplex: true
        outputFileTags: "dummy"
        prepare: {
            var cmd = new JavaScriptCommand();
            cmd.silent = true;
            cmd.sourceCode = function() {
                var origPath = FileInfo.joinPaths(product.sourceDirectory, "original.txt");
                var copyPath = FileInfo.joinPaths(product.sourceDirectory, "copy.txt");
                console.info("copy path: "+copyPath);
                var original = new TextFile(origPath, TextFile.WriteOnly);
                original.close();
                File.copy(origPath, copyPath);
                var origTimestamp = File.lastModified(origPath);
                var copyTimestamp = File.lastModified(copyPath);
                if (origTimestamp > copyTimestamp)
                    throw new Error("Older file has newer timestamp.");
                File.remove(origPath);
                var copy = new TextFile(copyPath, TextFile.WriteOnly);
                copy.writeLine(File.exists(origPath));
                copy.writeLine(File.exists(copyPath));
                copy.close();
                var zePath = FileInfo.joinPaths(product.sourceDirectory, "zePath");
                if (File.exists(zePath))
                    throw new Error(zePath + " already exists.");
                var created = File.makePath(zePath);
                if (!created || !File.exists(zePath))
                    throw new Error("zePath was not created.");
                var entries = File.directoryEntries(product.sourceDirectory, File.AllEntries | File.NoDotAndDotDot);
                if (entries.length < 3 || !entries.contains("file.qbs"))
                    throw new Error("Directory did not contain file.qbs");
                entries = File.directoryEntries(product.sourceDirectory, File.Dirs | File.NoDotAndDotDot);
                if (entries.length < 1 || !entries.contains("zePath"))
                    throw new Error("Directory did not contain only zePath");
                var moveSource = FileInfo.joinPaths(product.sourceDirectory, "tomove.txt");
                var moveTarget = FileInfo.joinPaths(product.sourceDirectory, "moved.txt");
                File.move(moveSource, moveTarget);
                if (File.exists(moveSource))
                    throw new Error("Moved file still exists under old name");
                if (!File.exists(moveTarget))
                    throw new Error("Moved file does not exist under new name");
            };
            return [cmd];
        }
    }
}
