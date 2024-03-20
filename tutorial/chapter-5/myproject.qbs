Project {
    name: "My Project"
    minimumQbsVersion: "2.0"
    // ![0]
    references: [
        "app/app.qbs",
        "lib/lib.qbs",
        "test/test.qbs",
    ]
    // ![0]
    qbsSearchPaths: "qbs"
    AutotestRunner {
        timeout: 60
    }
}
