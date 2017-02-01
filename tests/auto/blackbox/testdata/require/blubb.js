var TextFile = require("qbs.TextFile")
var zort = require("./zort.js")

function createCommands(filePaths) {
    var cmd = new JavaScriptCommand();
    cmd.description = "Write an empty file";
    cmd.filePath = filePaths[0];
    cmd.sourceCode = function() {
        var f = new TextFile(filePath, TextFile.WriteOnly);
        f.close();
    }
    return [cmd, zort.createCommand(filePaths)];
}
