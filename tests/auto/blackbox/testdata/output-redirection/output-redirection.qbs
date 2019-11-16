import qbs.FileInfo

Product {
    name: "the-product"
    type: "output"
    Group {
        files: "input.bin"
        fileTags: "binary"
    }
    Group {
        files: "input.txt"
        fileTags: "text"
    }

    Rule {
        inputs: ["text", "binary"]
        Artifact {
            filePath: "output." + FileInfo.completeSuffix(input.filePath)
            fileTags: "output"
        }
        prepare: {
            var binary;
            var prefixArgs;
            if (product.qbs.hostOS.contains("windows")) {
                binary = product.qbs.windowsShellPath;
                prefixArgs = ["/c", "type"];
            } else {
                binary = "cat";
                prefixArgs = [];
            }
            var inputPath = FileInfo.toNativeSeparators(input.filePath);
            var cmd = new Command(binary, prefixArgs.concat([inputPath, inputPath]));
            cmd.stdoutFilePath = output.filePath;
            cmd.highlight = "filegen";
            return cmd;
        }
    }
}
