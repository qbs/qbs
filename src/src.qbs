import qbs

Project {
    references: [
        "app/apps.qbs",
        "generators/generators.qbs",
        "lib/libs.qbs",
        "libexec/libexec.qbs",
        "plugins/plugins.qbs"
    ]
}
