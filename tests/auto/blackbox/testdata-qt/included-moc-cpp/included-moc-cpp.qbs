import qbs.Utilities

QtApplication {
    condition: {
        if (Utilities.versionCompare(Qt.core.version, "5.0") < 0) {
            console.info("using qt4");
            return false;
        }
        return true;
    }
    files: [
        "main.cpp",
        "myobject.cpp",
        "myobject.h",
    ]
}
