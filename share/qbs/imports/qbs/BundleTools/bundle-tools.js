var FileInfo = loadExtension("qbs.FileInfo");
var DarwinTools = loadExtension("qbs.DarwinTools");
var PropertyList = loadExtension("qbs.PropertyList");

function destinationDirectoryForResource(product, input) {
    var path = product.destinationDirectory;
    var inputFilePath = FileInfo.joinPaths(input.baseDir, input.fileName);
    var key = DarwinTools.localizationKey(inputFilePath);
    if (key) {
        path = FileInfo.joinPaths(path, localizedResourcesFolderPath(product, key));
        var subPath = DarwinTools.relativeResourcePath(inputFilePath);
        if (subPath && subPath !== '.')
            path = FileInfo.joinPaths(path, subPath);
    } else {
        path = FileInfo.joinPaths(path, product.moduleProperty("bundle", "unlocalizedResourcesFolderPath"));
    }
    return path;
}

function localizedResourcesFolderPath(product, key) {
    return FileInfo.joinPaths(product.moduleProperty("bundle", "unlocalizedResourcesFolderPath"), key + product.moduleProperty("bundle", "localizedResourcesFolderSuffix"));
}

function frameworkSymlinkCreateCommands(bundlePath, targetName, frameworkVersion) {
    var cmd, commands = [];

    cmd = new JavaScriptCommand();
    cmd.description = "creating framework " + targetName;
    cmd.highlight = "codegen";
    cmd.sourceCode = function() { };
    commands.push(cmd);

    cmd = new Command("ln", ["-sfn", frameworkVersion, "Current"]);
    cmd.workingDirectory = FileInfo.joinPaths(bundlePath, "Versions");
    cmd.silent = true;
    commands.push(cmd);

    cmd = new Command("ln", ["-sfn", "Versions/Current/Headers", "Headers"]);
    cmd.workingDirectory = bundlePath;
    cmd.silent = true;
    commands.push(cmd);

    cmd = new Command("ln", ["-sfn", "Versions/Current/PrivateHeaders", "PrivateHeaders"]);
    cmd.workingDirectory = bundlePath;
    cmd.silent = true;
    commands.push(cmd);

    cmd = new Command("ln", ["-sfn", "Versions/Current/Resources", "Resources"]);
    cmd.workingDirectory = bundlePath;
    cmd.silent = true;
    commands.push(cmd);

    cmd = new Command("ln", ["-sf", FileInfo.joinPaths("Versions", "Current", targetName), targetName]);
    cmd.workingDirectory = bundlePath;
    cmd.silent = true;
    commands.push(cmd);

    return commands;
}

function infoPlistContents(infoPlistFilePath) {
    if (infoPlistFilePath === undefined)
        return undefined;

    var plist = new PropertyList();
    try {
        plist.readFromFile(infoPlistFilePath);
        return plist.toObject();
    } finally {
        plist.clear();
    }
}

function infoPlistFormat(infoPlistFilePath) {
    if (infoPlistFilePath === undefined)
        return undefined;

    var plist = new PropertyList();
    try {
        plist.readFromFile(infoPlistFilePath);
        return plist.format();
    } finally {
        plist.clear();
    }
}
