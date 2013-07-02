import qbs

Project {
    name: "qbs plugins"
    references: [
        "scanner/cpp/cpp.qbs",
        "scanner/qt/qt.qbs"
    ]
}
