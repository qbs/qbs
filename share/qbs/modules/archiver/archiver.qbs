import qbs
import qbs.File
import qbs.FileInfo

Module {
    property string type
    property string archiveBaseName: product.name
    property string workingDirectory
    property stringList flags: []
    property path outputDirectory: product.destinationDirectory
    property string archiveExtension: {
        if (type === "7zip")
            return "7z";
        if (type == "tar") {
            var extension = "tar";
            if (compressionType !== "none")
                extension += "." + compressionType;
            return extension;
        }
        return undefined;
    }
    property string command: {
        if (type === "7zip")
            return "7z";
        if (type === "tar")
            return "tar";
        return undefined;
    }

    property string compressionLevel
    property string compressionType: "gz"

    PropertyOptions {
        name: "type"
        description: "The type of archive to create."
        allowedValues: ["7zip", "tar"]
    }

    PropertyOptions {
        name: "compressionLevel"
        description: "How much effort to put into compression.\n"
            + "Higher numbers mean smaller archive files at the cost of taking more time.\n"
            + "This property is only used for the '7zip' type."
        allowedValues: [undefined, "0", "1", "3", "5", "7", "9"]
    }

    PropertyOptions {
        name: "compressionType"
        description: "The compression format to use. The respective tool needs to be present.\n"
            + "This property is only used for the 'tar' type."
        allowedValues: ["none", "gz", "bz2", "Z", "xz"]
    }

    Rule {
        inputs: ["archiver.input-list"]

        Artifact {
            filePath: FileInfo.joinPaths(product.moduleProperty("archiver", "outputDirectory"),
                              product.moduleProperty("archiver", "archiveBaseName") + '.'
                                         + product.moduleProperty("archiver", "archiveExtension"));
            fileTags: ["archiver.archive"]
        }

        prepare: {
            var binary = product.moduleProperty("archiver", "command");
            var args = [];
            var commands = [];
            var type = product.moduleProperty("archiver", "type");
            if (type === "7zip") {
                var rmCommand = new JavaScriptCommand();
                rmCommand.silent = true;
                rmCommand.sourceCode = function() {
                    if (File.exists(output.filePath))
                        File.remove(output.filePath);
                };
                commands.push(rmCommand);
                args.push("a", "-y", "-mmt=on");
                var compressionLevel = product.moduleProperty("archiver", "compressionLevel");
                if (compressionLevel)
                    args.push("-mx" + compressionLevel);
                args = args.concat(product.moduleProperty("archiver", "flags"));
                args.push(output.filePath);
                args.push("@" + input.filePath);
            } else if (type === "tar") {
                args.push("-c");
                var compression = product.moduleProperty("tar", "compressiontype");
                if (compression === "gz")
                    args.push("-z");
                else if (compression === "bz2")
                    args.push("-j");
                else if (compression === "Z")
                    args.push("-Z");
                else if (compression === "xz")
                    args.push("-J");
                args.push("-f", output.filePath, "-T", input.filePath);
                args = args.concat(product.moduleProperty("archiver", "flags"));
            }
            var archiverCommand = new Command(binary, args);
            archiverCommand.description = "Creating archive file " + output.fileName;
            archiverCommand.highlight = "linker";
            archiverCommand.workingDirectory
                    = product.moduleProperty("archiver", "workingDirectory");
            commands.push(archiverCommand);
            return commands;
        }
    }
}
