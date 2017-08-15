import qbs
import qbs.FileInfo
import qbs.Probes
import qbs.Utilities

Module {
    Probes.BinaryProbe {
        id: dockercli
        names: ["docker"]
        platformPaths: qbs.hostOS.contains("unix") ? ["/usr/bin", "/usr/local/bin"] : []
    }

    property string dockerFilePath: dockercli.filePath
    property string imageTag
    property stringList buildFlags

    FileTagger {
        patterns: ["Dockerfile"]
        fileTags: ["docker.dockerfile"]
    }

    Rule {
        inputs: ["docker.dockerfile"]

        Artifact {
            // Let Docker handle the dependency management
            filePath: FileInfo.joinPaths(product.buildDirectory,
                                         Utilities.getHash(input.filePath), ".docker-image-dummy")
            fileTags: ["docker.docker-image"]
        }

        prepare: {
            var args = ["build"];
            var tag = product.docker.imageTag;
            if (tag)
                args.push("-t", tag);
            Array.prototype.push.apply(args, product.docker.buildFlags);
            args.push(".");
            var cmd = new Command(product.docker.dockerFilePath, args);
            cmd.workingDirectory = FileInfo.path(input.filePath);
            cmd.description = "building docker image "
                    + FileInfo.fileName(cmd.workingDirectory) + (tag ? " (" + tag + ")" : "");
            return [cmd];
        }
    }

    validate: {
        if (!dockerFilePath)
            throw ModUtils.ModuleError("Could not find Docker.");
    }
}
