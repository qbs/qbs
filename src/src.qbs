import qbs

Project {
    references: [
        "app/apps.qbs",
        "lib/libs.qbs",
        "libexec/libexec.qbs",
        "plugins/plugins.qbs"
    ]
}
