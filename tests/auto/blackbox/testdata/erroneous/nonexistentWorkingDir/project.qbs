import qbs

Application {
    name: "kaputt"
    Transformer {
        Artifact {
            fileName: "Stulle"
            fileTags: ["nutritious"]
        }
        prepare: {
            var cmd = new Command("ls");
            cmd.workingDirectory = "/does/not/exist";
            cmd.silent = true;
            return [cmd];
        }
    }
}

