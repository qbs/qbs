QtApplication {
    name: "app"
    files: "main.cpp"
    Group {
        fileTagsFilter: ["qthtml"]
        qbs.install: true
    }
}

