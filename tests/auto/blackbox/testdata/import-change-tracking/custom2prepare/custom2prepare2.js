var Custom2Command = require("custom2command");

function prepare() {
    console.info("running custom2 prepare script");
    var cmd = new JavaScriptCommand();
    cmd.description = "running custom2 command";
    cmd.sourceCode = function() { return Custom2Command.sourceCode(); };
    return cmd;
}
