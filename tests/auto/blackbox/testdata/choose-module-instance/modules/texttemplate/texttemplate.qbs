import qbs.TextFile

Module {
    property var dict: ({})
    FileTagger {
        patterns: ["*.in"]
        fileTags: ["texttemplate.input"]
    }
    Rule {
        inputs: ["texttemplate.input"]
        Artifact {
            fileTags: ["text"]
            filePath: input.completeBaseName
        }
        prepare: {
            var cmd = new JavaScriptCommand();
            cmd.silent = true;
            cmd.sourceCode = function() {
                try {
                    var src = new TextFile(input.filePath, TextFile.ReadOnly);
                    var dst = new TextFile(output.filePath, TextFile.WriteOnly);
                    var rex = /\$([A-Z]+)/g;
                    while (!src.atEof()) {
                        rex.lastIndex = 0;
                        var line = src.readLine();
                        while (true) {
                            var result = rex.exec(line);
                            if (!result)
                                break;
                            var replacement = input.texttemplate.dict[result[1]];
                            if (replacement) {
                                line = line.substr(0, result.index)
                                        + replacement
                                        + line.substr(result.index + result[0].length);
                            }
                        }
                        dst.writeLine(line);
                    }
                } finally {
                    if (src)
                        src.close();
                    if (dst)
                        dst.close();
                }
            };
            return [cmd];
        }
    }
}
