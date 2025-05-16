import "InstalledApp.qbs" as InstalledApp

InstalledApp {
    files: "main.cpp"
    install: false
    Group {
        fileTagsFilter: product.type
        qbs.install: true
        qbs.installDir: "habicht"
    }
}
