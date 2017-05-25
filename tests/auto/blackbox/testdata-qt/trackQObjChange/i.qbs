import qbs 1.0

Project {
    Product {
        type: "application"
        consoleApplication: true
        name: "i"

        Depends {
            name: "Qt.core"
        }

        files: [
            "bla.cpp",
            "bla.h"
        ]
    }
}

