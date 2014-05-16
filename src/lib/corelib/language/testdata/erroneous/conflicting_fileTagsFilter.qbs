import qbs 1.0

Application {
    Group {
        fileTagsFilter: "application"
        qbs.install: true
    }
    Group {
        fileTagsFilter: "application"
        qbs.install: false
    }
}

