import qbs 1.0

Project {
    Product {
        consoleApplication: true
        type: "application"
        name: "i"

        Depends {
            name: "Qt.core"
        }

        files: [
            "bla.cpp",
            "bla.qrc",
            "stuff.txt"
        ]
    }
}

