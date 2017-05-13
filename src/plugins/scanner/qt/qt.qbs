import qbs 1.0
import "../../qbsplugin.qbs" as QbsPlugin

QbsPlugin {
    name: "qbs_qt_scanner"
    files: [
        "../scanner.h",
        "qtscanner.cpp"
    ]
}

