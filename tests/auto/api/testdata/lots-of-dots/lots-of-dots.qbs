Project {
    QtGuiApplication {
        type: "application"
        consoleApplication: true
        name: "lots.of.dots"
        property bool dummy: { console.info("executable suffix: " + cpp.executableSuffix); }
        cpp.cxxLanguageVersion: "c++11"
        files : [
            "m.a.i.n.cpp",
            "object.narf.h",
            "object.narf.cpp",
            "polka.dots.qrc",
            "dotty.matrix.ui"
        ]
    }
}

