Application {
    name: "kaputt"
    type: ["nutritious"]
    Rule {
        multiplex: true
        Artifact {
            filePath: "Stulle"
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

