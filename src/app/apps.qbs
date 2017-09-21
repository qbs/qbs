import qbs

Project {
    references: [
        "config/config.qbs",
        "config-ui/config-ui.qbs",
        "qbs/qbs.qbs",
        "qbs-create-project/qbs-create-project.qbs",
        "qbs-setup-android/qbs-setup-android.qbs",
        "qbs-setup-qt/qbs-setup-qt.qbs",
        "qbs-setup-toolchains/qbs-setup-toolchains.qbs",
    ]
}
