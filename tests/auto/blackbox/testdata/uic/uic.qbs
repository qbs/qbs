import qbs 1.0

Project {
    Product {
        type: "application"
        name: "ui"

        Depends { name: "Qt.gui"}

        files: [
            "bla.cpp",
            "bla.h",
            "ui.ui",
            "ui.h"
        ]
    }
}
