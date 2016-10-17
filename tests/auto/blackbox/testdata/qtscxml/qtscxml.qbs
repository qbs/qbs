import qbs
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
                if (product.moduleProperty("qbs", "hostOS").contains("windows")) {
                    pathVar = "PATH";
                    pathValue = FileInfo.toWindowsSeparators(
                                input.moduleProperty("Qt.core", "binPath"));
                } else {
                    pathVar = "LD_LIBRARY_PATH";
                    pathValue = input.moduleProperty("Qt.core", "libPath");
                }
                cmd.environment = [pathVar + '=' + pathValue];
                return [cmd];
            }
        }
    }
}
