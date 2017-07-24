import qbs
import qbs.Environment
import qbs.FileInfo

Project {
    QtApplication {
        name: "app"
        Depends { name: "Qt.scxml"; required: false }

        Properties {
            condition: Qt.scxml.present
            cpp.defines: ["HAS_QTSCXML"]
        }

        Qt.scxml.className: "QbsStateMachine"
        Qt.scxml.namespace: "QbsTest"

        files: ["main.cpp"]
        Group {
            files: ["dummystatemachine.scxml"]
            fileTags: ["qt.scxml.compilable"]
        }
    }

    Product {
        name: "runner"
        type: ["runner"]
        Depends { name: "app" }
        Rule {
            inputsFromDependencies: ["application"]
            Artifact {
                filePath: "dummy"
                fileTags: ["runner"]
            }
            prepare: {
                var cmd = new Command(input.filePath);
                cmd.description = "running " + input.filePath;
                var pathVar;
                var pathValue;
                if (product.qbs.hostOS.contains("windows")) {
                    pathVar = "PATH";
                    pathValue = FileInfo.toWindowsSeparators(input["Qt.core"].binPath);
                } else {
                    pathVar = "LD_LIBRARY_PATH";
                    pathValue = input["Qt.core"].libPath;
                }
                var oldValue = Environment.getEnv(pathVar) || "";
                var newValue = pathValue + product.qbs.pathListSeparator + oldValue;
                cmd.environment = [pathVar + '=' + newValue];
                return [cmd];
            }
        }
    }
}
