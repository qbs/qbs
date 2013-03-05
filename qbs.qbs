import qbs 1.0

Project {
    property string version: "0.3.0"
    property bool enableTests: false
    references: [
        "src/app/config/config.qbs",
        "src/app/config-ui/config-ui.qbs",
        "src/app/detect-toolchains/detect-toolchains.qbs",
        "src/app/qbs/qbs.qbs",
        "src/app/qbs-qmltypes/qbs-qmltypes.qbs",
        "src/app/qbs-setup-qt/qbs-setup-qt.qbs",
        "src/app/setupmaddeplatforms/setupmaddeplatforms.qbs",
        "src/lib/lib.qbs",
        "src/plugins/scanner/cpp/cpp.qbs",
        "src/plugins/scanner/qt/qt.qbs",
    ]

    Product {
        name: "share"
        Group {
            files: ["share/qbs"]
            qbs.install: true
            qbs.installDir: "shared"
        }
    }
}

