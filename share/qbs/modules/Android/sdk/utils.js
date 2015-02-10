var FileInfo = loadExtension("qbs.FileInfo");
var TextFile = loadExtension("qbs.TextFile");

function sourceAndTargetFilePathsFromInfoFiles(inputs, product, inputTag)
{
    var sourceFilePaths = [];
    var targetFilePaths = [];
    var inputsLength = inputs[inputTag] ? inputs[inputTag].length : 0;
    for (var i = 0; i < inputsLength; ++i) {
        var infoFile = new TextFile(inputs[inputTag][i].filePath, TextFile.ReadOnly);
        var sourceFilePath = infoFile.readLine();
        var targetFilePath = FileInfo.joinPaths(product.buildDirectory, infoFile.readLine());
        if (!targetFilePaths.contains(targetFilePath)) {
            sourceFilePaths.push(sourceFilePath);
            targetFilePaths.push(targetFilePath);
        }
        infoFile.close();
    }
    return { sourcePaths: sourceFilePaths, targetPaths: targetFilePaths };
}

function outputArtifactsFromInfoFiles(inputs, product, inputTag, outputTag)
{
    var pathSpecs = sourceAndTargetFilePathsFromInfoFiles(inputs, product, inputTag)
    var artifacts = [];
    for (i = 0; i < pathSpecs.targetPaths.length; ++i)
        artifacts.push({filePath: pathSpecs.targetPaths[i], fileTags: [outputTag]});
    return artifacts;
}
