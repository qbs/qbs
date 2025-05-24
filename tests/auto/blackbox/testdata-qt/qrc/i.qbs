import qbs.Host
import qbs.Utilities

Project {
    Product {
        condition: {
            if (Utilities.versionCompare(Qt.core.version, "5.0") < 0) {
                console.info("using qt4");
                return false;
            }
            var result = qbs.targetPlatform === Host.platform() && qbs.architecture === Host.architecture();
            if (!result)
                console.info("target platform/arch differ from host platform/arch");
            console.info("executable suffix: " + cpp.executableSuffix);
            return result;
        }
        consoleApplication: true
        type: "application"
        name: "i"

        Depends {
            name: "Qt.core"
        }

        files: [
            "bla.cpp",
            "bla.qrc",
            //"test.cpp",
        ]
    }
}

