import qbs
import qbs.File
import qbs.FileInfo
import qbs.TextFile

Product {
    type: ["dummy"]
    Transformer {
        Artifact {
            filePath: "dummy.txt"
            fileTags: ["dummy"]
        }
        prepare: {
            var cmd = new JavaScriptCommand();
            cmd.silent = true;
            cmd.sourceCode = function() {
                var origPath = FileInfo.joinPaths(product.sourceDirectory, "original.txt");
                var copyPath = FileInfo.joinPaths(product.sourceDirectory, "copy.txt");
                print("copy path: "+copyPath);
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

            };
            return [cmd];
        }
    }
}
