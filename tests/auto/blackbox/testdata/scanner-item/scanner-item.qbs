import qbs.File
import qbs.FileInfo

Product {
    type: "t"
    Depends { name: "m" }

    property bool productScanner
    property bool moduleScanner
    property bool enableGroup

    Group {
        files: ["subdir1/file1.in", "subdir2/file2.in"]
        fileTags: "i"
    }

    TheScanner { condition: productScanner }

    Rule {
        inputs: "i"
        Artifact {
            filePath: FileInfo.baseName(input.fileName) + ".out"
            fileTags: "t"
        }
        prepare: {
            var cmd = new JavaScriptCommand();
            cmd.description = "handling " + input.fileName;
            cmd.sourceCode = function() { File.copy(input.filePath, output.filePath); };
            return cmd;
        }
    }
}
