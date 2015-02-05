var TextFile = loadExtension("qbs.TextFile");

function sourceAndTargetFilePathsFromInfoFiles(inputs, inputTag)
{
    var sourceFilePaths = [];
    var targetFilePaths = [];
    var inputsLength = inputs[inputTag] ? inputs[inputTag].length : 0;
    for (var i = 0; i < inputsLength; ++i) {
        var infoFile = new TextFile(inputs[inputTag][i].filePath, TextFile.ReadOnly);
        var sourceFilePath = infoFile.readLine();
        var targetFilePath = infoFile.readLine();
        if (!targetFilePaths.contains(targetFilePath)) {
            sourceFilePaths.push(sourceFilePath);
            targetFilePaths.push(targetFilePath);
        }
        infoFile.close();
    }
    return { sourcePaths: sourceFilePaths, targetPaths: targetFilePaths };
}

function outputArtifactsFromInfoFiles(inputs, inputTag, outputTag)
{
    var pathSpecs = sourceAndTargetFilePathsFromInfoFiles(inputs, inputTag)
    var artifacts = [];
    for (i = 0; i < pathSpecs.targetPaths.length; ++i)
        artifacts.push({filePath: pathSpecs.targetPaths[i], fileTags: [outputTag]});
    return artifacts;
}
