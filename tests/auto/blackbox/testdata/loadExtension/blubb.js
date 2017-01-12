var TextFile = loadExtension("qbs.TextFile")

function func(filePath) {
    var cmd = new JavaScriptCommand();
    cmd.description = "Write an empty file";
    cmd.filePath = filePath;
    cmd.sourceCode = function() {
        var f = new TextFile(filePath, TextFile.WriteOnly);
        f.close();
    }
    return cmd;
}
