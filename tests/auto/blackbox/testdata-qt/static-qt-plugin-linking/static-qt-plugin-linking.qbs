import qbs

StaticLibrary {
    name: "somelib"
    Probe {
        id: staticQtChecker
        property bool staticQt: Qt.core.staticBuild
        configure: {
            found = staticQt;
            if (found)
                console.info("Qt is static");
        }
    }

    Depends { name: "Qt.core" }
    Depends { name: "Qt.qminimal"; condition: Qt.core.staticBuild; }
}
