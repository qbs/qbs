import qbs.base
import qbs.File
import qbs.FileInfo

DynamicLibrary {
    name: "echoplugin"

    Depends { name: "Qt.core" }
    Depends { name: "cpp" }
    Properties {
        condition: qbs.targetOS.contains("darwin")
        bundle.isBundle: false
    }

    Group {
        condition: Qt.core.versionMajor >= 5
        files: [
            "echoplugin.h",
            "echoplugin.cpp",
        ]
    }
    Group {
        condition: Qt.core.versionMajor >= 5
        files: ["echoplugin.json.source"]
        fileTags: ["json_in"]
    }

    Group {
        condition: Qt.core.versionMajor < 5
        files: "echoplugin_dummy.cpp"
    }

    cpp.includePaths: buildDirectory

    Rule {
        condition: Qt.core.versionMajor >= 5
        inputs: ["json_in"]
        Artifact {
            filePath: "echoplugin.json"
            fileTags: ["qt_plugin_metadata"]
        }
        prepare: {
            var cmd = new JavaScriptCommand();
            cmd.description = "generating " + FileInfo.fileName(output.filePath);
            cmd.sourceCode = function() {
                File.copy(input.filePath, output.filePath);
            }
            return cmd;
        }
    }
}
