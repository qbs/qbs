import "../../qbsplugin.qbs" as QbsPlugin

QbsPlugin {
    Depends { name: "qbscore" }
    name: "qbs_qt_moc_scanner"
    files: [
        "../scanner.h",
        "qtmocscanner.cpp"
    ]
}
