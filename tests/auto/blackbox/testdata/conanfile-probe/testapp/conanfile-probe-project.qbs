import qbs.Probes
import qbs.TextFile

Project {
    readonly property bool forceFailure: false

    Probes.ConanfileProbe {
        id: conan
        conanfilePath: path + "/conanfile.py"
        options: ({opt: "True", forceFailure: (project.forceFailure ? "True" : "False")})
        settings: ({os: "AIX"})
    }

    property var check: {
        var tf = new TextFile(buildDirectory + "/results.json", TextFile.WriteOnly);
        var o = {
            json: conan.json.deps_env_info["ENV_VAR"],
            dependencies: conan.dependencies["testlib"].libs,
            generatedFilesPath: conan.generatedFilesPath
        };
        tf.write(JSON.stringify(o));
    }
}
