Product {
    name: "p"
    Probe {
        id: staticQtChecker
        property bool staticQt: Qt.core.staticBuild
        configure: {
            found = staticQt;
            if (found)
                console.info("Qt is static");
        }
    }

    Group {
        condition: type.contains("application")
        files: "main.cpp"
    }

    Depends { name: "Qt.core" }
    Depends { name: "Qt.gui" }
    Depends { name: "Qt.qminimal"; condition: Qt.core.staticBuild; }
}
