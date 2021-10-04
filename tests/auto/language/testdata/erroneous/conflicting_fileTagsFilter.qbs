Product {
    type: "app"
    Group {
        fileTagsFilter: "app"
        qbs.install: true
    }
    Group {
        fileTagsFilter: "app"
        qbs.install: false
    }
}

