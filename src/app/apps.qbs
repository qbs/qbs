import qbs

Project {
    references: [
        "config/config.qbs",
        "config-ui/config-ui.qbs",
        "detect-toolchains/detect-toolchains.qbs",
        "qbs/qbs.qbs",
        "qbs-qmltypes/qbs-qmltypes.qbs",
        "qbs-setup-qt/qbs-setup-qt.qbs",
        "setupmaddeplatforms/setupmaddeplatforms.qbs"
    ]
}
