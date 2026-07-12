import qbs.File
import qbs.TextFile

Product {
    name: "scan-auxiliary-inputs"
    type: "gen_embedded"

    Group {
        files: "widget.hdr"
        fileTags: ["hdr"]
    }

    Group {
        files: "main.src"
        fileTags: ["src"]
    }

    Scanner {
        scannerId: "genScanner"
        inputs: ["hdr", "src"]
        searchPaths: []
        scan: {
            var file = new TextFile(filePath, TextFile.ReadOnly);
            var content = file.readAll();
            file.close();
            if (filePath.endsWith(".hdr")) {
                if (content.indexOf("# GENERATE") >= 0)
                    return { scannerProperties: { hasMacro: true } };
                return {};
            }
            if (filePath.endsWith(".src")) {
                var names = [];
                var pattern = /#include\s*<gen_(\w+)\.out>/g;
                var match;
                while ((match = pattern.exec(content)) !== null)
                    names.push(match[1]);
                if (names.length > 0)
                    return { scannerProperties: { includedGenBaseNames: names } };
                return {};
            }
            return {};
        }
    }

    Rule {
        inputs: "hdr"
        auxiliaryInputs: "src"
        outputFileTags: ["gen", "gen_embedded"]
        outputArtifacts: {
            var info = input.qbsScanners && input.qbsScanners.genScanner;
            if (!info || !info.hasMacro)
                return [];
            var included = [];
            var srcArtifacts = product.artifacts["src"] || [];
            for (var i = 0; i < srcArtifacts.length; ++i) {
                var srcInfo = srcArtifacts[i].qbsScanners
                        && srcArtifacts[i].qbsScanners.genScanner;
                if (!srcInfo || !srcInfo.includedGenBaseNames)
                    continue;
                for (var j = 0; j < srcInfo.includedGenBaseNames.length; ++j) {
                    var baseName = srcInfo.includedGenBaseNames[j];
                    if (included.indexOf(baseName) < 0)
                        included.push(baseName);
                }
            }
            var mustEmitStandalone = included.indexOf(input.completeBaseName) < 0;
            return [{
                filePath: "gen_" + input.completeBaseName + ".out",
                fileTags: mustEmitStandalone ? ["gen"] : ["gen_embedded"]
            }];
        }
        prepare: {
            var cmd = new JavaScriptCommand();
            cmd.description = (output.fileTags.indexOf("gen_embedded") >= 0 ? "embedded" : "standalone")
                    + " gen for " + input.fileName;
            cmd.sourceCode = function() {
                var file = new TextFile(output.filePath, TextFile.WriteOnly);
                file.write("generated");
                file.close();
            };
            return cmd;
        }
    }
}
