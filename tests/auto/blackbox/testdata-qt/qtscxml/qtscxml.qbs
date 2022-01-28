import qbs.Environment
import qbs.FileInfo
import qbs.Host
import qbs.Utilities

Project {
    QtApplication {
        name: "app"
        Depends { name: "Qt.scxml"; required: false }

        Properties {
            condition: Qt.scxml.present && Utilities.versionCompare(Qt.core.version, "5.13.0") != 0
            cpp.defines: ["HAS_QTSCXML"]
        }

        Qt.scxml.className: "QbsStateMachine"
        Qt.scxml.namespace: "QbsTest"
        Qt.scxml.generateStateMethods: true

        files: ["main.cpp"]
        Group {
            files: ["dummystatemachine.scxml"]
            fileTags: ["qt.scxml.compilable"]
        }
    }

    Product {
        condition: {
            var result = qbs.targetPlatform === Host.platform();
            if (!result)
                console.info("targetPlatform differs from hostPlatform");
            return result;
        }
        name: "runner"
        type: ["runner"]
        Depends { name: "app" }
        Rule {
            inputsFromDependencies: ["application"]
            outputFileTags: ["runner"]
            prepare: {
                var cmd = new Command(input.filePath);
                cmd.description = "running " + input.filePath;

                var envVars = {};
                if (Host.os().contains("windows")) {
                    envVars["PATH"] = FileInfo.toWindowsSeparators(input["Qt.core"].binPath);
                } else if (Host.os().contains("macos")) {
                    envVars["DYLD_LIBRARY_PATH"] = input["Qt.core"].libPath;
                    envVars["DYLD_FRAMEWORK_PATH"] = input["Qt.core"].libPath;
                } else {
                    envVars["LD_LIBRARY_PATH"] = input["Qt.core"].libPath;
                }
                for (var varName in envVars) {
                    var oldValue = Environment.getEnv(varName) || "";
                    var newValue = envVars[varName] + FileInfo.pathListSeparator() + oldValue;
                    cmd.environment.push(varName + '=' + newValue);
                }

                return [cmd];
            }
        }
    }
}
