import qbs.Host
import qbs.Utilities

QtApplication {
    condition: {
        if (Utilities.versionCompare(Qt.core.version, "5.0") < 0) {
            console.info("using qt4");
            return false;
        }
        var result = qbs.targetPlatform === Host.platform() && qbs.architecture === Host.architecture();
        if (!result)
            console.info("target platform/arch differ from host platform/arch");
        return result;
    }
    files: "main.cpp"
    Group {
        name: "QObject service provider"
        files: "createqtclass.h"
        fileTags: ["hpp", "unmocable"]
    }
    Group {
        name: "QObject service user"
        files: "myqtclass.h"
        fileTags: ["hpp", "mocable"]
    }
}
