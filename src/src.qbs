import qbs

Project {
    references: [
        "app/apps.qbs",
        "lib/libs.qbs",
        "libexec/libexec.qbs",
        "packages/packages.qbs",
        "plugins/plugins.qbs"
    ]
}
