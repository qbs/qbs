var Custom1Command = require("./custom1command.js");
var Irrelevant = require("./irrelevant.js");

function prepare() {
    console.info("running custom1 prepare script");
    var cmd = new JavaScriptCommand();
    cmd.description = "running custom1 command";
    cmd.sourceCode = function() { return Custom1Command.sourceCode(); };
    return cmd;
}
