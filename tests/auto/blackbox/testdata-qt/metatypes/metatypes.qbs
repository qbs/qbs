import qbs.Utilities

StaticLibrary {
    name: "mylib"

    Depends { name: "Qt.core" }

    qbs.installPrefix: "some-prefix"

    Probe {
        id: capabilitiesChecker
        property string version: Qt.core.version
        configure: {
            if (Utilities.versionCompare(version, "5.15") >= 0)
                console.info("can generate");
            else
                console.info("cannot generate");
            found = true;
        }
    }

    files: [
        "mocableclass1.cpp",
        "mocableclass1.h",
        "mocableclass2.cpp",
        "unmocableclass.cpp",
    ]
}
