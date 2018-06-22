import qbs.FileInfo

Module {
    Depends { name: "cpp" }
    Group {
        name: "thetool binary"
        files: FileInfo.cleanPath(FileInfo.joinPaths(path, "..", "..",
                   "thetool" + (qbs.hostOS.contains("windows") ? ".exe" : "")));
        fileTags: ["thetool.thetool"]
        filesAreTargets: true
    }

    Rule {
        multiplex: true
        explicitlyDependsOnFromDependencies: ["thetool.thetool"]
        Artifact {
            filePath: "tool-output.txt"
            fileTags: ["thetool.output"]
        }
        prepare: {
            var cmd = new Command(explicitlyDependsOn["thetool.thetool"][0].filePath,
                                  output.filePath);
            cmd.description = "running the tool";
            return [cmd];
        }
    }
}
