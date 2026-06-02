import qbs.File
import qbs.TextFile

Product {
    name: "scanner-properties"
    type: "out"

    Group {
        files: ["specs/a.spec", "specs/b.spec"]
        fileTags: "spec"
    }

    Scanner {
        scannerId: "theScanner"
        inputs: "spec"
        searchPaths: []
        scan: {
            if (input.qbsScanners !== undefined)
                throw new Error("qbsScanners must not be exposed in scan scripts");
            var file = new TextFile(filePath, TextFile.ReadOnly);
            var content = file.readAll().trim();
            return {
                scannerProperties: {
                    outputFileName: content
                }
            };
        }
    }

    Rule {
        inputs: "spec"
        outputFileTags: "out"
        outputArtifacts: {
            return [{
                filePath: input.qbsScanners.theScanner.outputFileName,
                fileTags: "out"
            }];
        }
        prepare: {
            if (input.qbsScanners === undefined)
                throw new Error("qbsScanners must be exposed in prepare scripts");
            if (input.qbsScanners.theScanner.outputFileName !== output.fileName) {
                throw new Error("qbsScanners data mismatch in prepare");
            }
            var cmd = new JavaScriptCommand();
            cmd.description = "copying " + input.fileName + " to " + output.fileName;
            cmd.sourceCode = function() {
                File.copy(input.filePath, output.filePath);
            };
            return cmd;
        }
    }
}
