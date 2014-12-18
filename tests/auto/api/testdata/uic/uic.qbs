import qbs 1.0

Project {
    QtGuiApplication {
        type: "application"
        consoleApplication: true
        name: "ui"

        files: [
            "bla.cpp",
            "bla.h",
            "ui.ui",
            "ui.h"
        ]
    }
}
