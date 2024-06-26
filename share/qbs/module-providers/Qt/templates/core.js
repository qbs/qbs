var TextFile = require("qbs.TextFile");

function lreleaseCommands(project, product, inputs, outputs, input, output, explicitlyDependsOn)
{
    var inputFilePaths;
    if (product.Qt.core.lreleaseMultiplexMode)
        inputFilePaths = inputs["ts"].map(function(artifact) { return artifact.filePath; });
    else
        inputFilePaths = [input.filePath];
    var args = ['-silent', '-qm', output.filePath].concat(inputFilePaths);
    var cmd = new Command(product.Qt.core.binPath + '/'
                          + product.Qt.core.lreleaseName, args);
    cmd.description = 'creating ' + output.fileName;
    cmd.highlight = 'filegen';
    return cmd;
}

function qhelpGeneratorCommands(product, input, output)
{
    var args = [input.filePath];
    args = args.concat(product.Qt.core.helpGeneratorArgs);
    args.push("-o");
    args.push(output.filePath);
    var cmd = new Command(
        product.Qt.core.helpGeneratorLibExecPath + "/qhelpgenerator", args);
    cmd.description = 'qhelpgenerator ' + input.fileName;
    cmd.highlight = 'filegen';
    cmd.stdoutFilterFunction = function(output) { return ""; };
    return cmd;
}

function wasmQtHtmlCommands(product, output)
{
    var cmd = new JavaScriptCommand();
    cmd.description = "Generating Qt html for " + product.targetName;
    cmd.highlight = "filegen";
    cmd.sourceCode = function() {
        var platformsPath = product.Qt.core.pluginPath + "/platforms";
        var wasmShellFile = new TextFile(platformsPath + "/wasm_shell.html");
        var content = wasmShellFile.readAll();
        var appNameTemplate = "@APPNAME@";
        var finalContent = content.replaceAll(appNameTemplate, product.name);
        var preload = "@PRELOAD@";//TODO: figure out what it's for
        finalContent = finalContent.replace(preload, "");
        var finalHtmlFile = new TextFile(output.filePath, OpenMode = TextFile.WriteOnly);
        finalHtmlFile.write(finalContent);
    }
    return cmd;
}
