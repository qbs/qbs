var File = loadExtension("qbs.File")

function createCommand(filePaths) {
    var cmd = new JavaScriptCommand();
    cmd.description = "Create another empty file";
    cmd.filePaths = filePaths;
    cmd.sourceCode = function() {
        File.copy(filePaths[0], filePaths[1]);
    };
    return cmd;
}
