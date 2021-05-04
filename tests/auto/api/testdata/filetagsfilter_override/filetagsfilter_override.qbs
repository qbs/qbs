import "InstalledApp.qbs" as InstalledApp

InstalledApp {
    files: "main.cpp"
    config.install.install: false
    Group {
        fileTagsFilter: product.type
        qbs.install: true
        qbs.installDir: "habicht"
    }
}
