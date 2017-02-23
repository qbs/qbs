import qbs 1.0

Project {
    QtGuiApplication {
        type: "application"
        consoleApplication: true
        name: "lots.of.dots"

        files : [
            "m.a.i.n.cpp",
            "object.narf.h",
            "object.narf.cpp",
            "polka.dots.qrc",
            "dotty.matrix.ui"
        ]
    }
}

