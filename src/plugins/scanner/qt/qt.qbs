import qbs 1.0
import "../scannerplugin.qbs" as ScannerPlugin

ScannerPlugin {
    name: "qbs_qt_scanner"
    files: [
        "../scanner.h",
        "qtscanner.cpp"
    ]
}

