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
