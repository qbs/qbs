import "InstalledApp.qbs" as InstalledApp

InstalledApp {
    files: "main.cpp"
    Group {
        fileTagsFilter: product.type
        qbs.install: true
        qbs.installDir: "habicht"
    }
}
