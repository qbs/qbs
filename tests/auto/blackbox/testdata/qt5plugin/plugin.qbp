import qbs.base
import qbs.fileinfo as FileInfo

DynamicLibrary {
    name: "echoplugin"

    Depends { name: "Qt"; submodules: ["core", "widgets"] }
    Depends { name: "cpp" }

    Group {
        condition: qt.core.versionMajor >= 5
        files: [
            "echoplugin.h",
            "echoplugin.cpp",
            "echoplugin.json.source"
        ]
    }
    Group {
        condition: qt.core.versionMajor < 5
        files: "echoplugin_dummy.cpp"
    }

    cpp.includePaths: buildDirectory

    Transformer {
        condition: qt.core.versionMajor >= 5
        inputs: ["echoplugin.json.source"]
        Artifact {
            fileName: "echoplugin.json"
            fileTags: ["qt_plugin_metadata"]
        }
        prepare: {
            var cmd = new JavaScriptCommand();
            cmd.description = "generating " + FileInfo.fileName(output.fileName);
            cmd.sourceCode = function() {
                File.copy(input.fileName, output.fileName);
            }
            return cmd;
        }
    }
}
