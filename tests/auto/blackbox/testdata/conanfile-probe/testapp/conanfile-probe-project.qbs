import qbs.Probes
import qbs.TextFile

Project {

    Probes.ConanfileProbe {
        id: conan
        conanfilePath: path + "/conanfile.py"
        options: ({opt: "True"})
        settings: ({os: "AIX"})
    }

    property var check: {
        tf = new TextFile(buildDirectory + "/results.json", TextFile.WriteOnly);
        var o = {
            json: conan.json.deps_env_info["ENV_VAR"],
            dependencies: conan.dependencies["testlib"].libs,
            generatedFilesPath: conan.generatedFilesPath
        };
        tf.write(JSON.stringify(o));
    }
}
