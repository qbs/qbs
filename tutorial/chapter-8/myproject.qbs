Project {
    property bool withTests: true
    property bool installDebugInformation: true
    property stringList autotestArguments: []
    property stringList autotestWrapper: []

    name: "My Project"
    minimumQbsVersion: "2.0"
    references: [
        "app/app.qbs",
        "lib/lib.qbs",
    ]
    qbsSearchPaths: "qbs"

    SubProject {
        filePath: "test/test.qbs"
        Properties {
            condition: parent.withTests
        }
    }

    AutotestRunner {
        condition: parent.withTests
        arguments: parent.autotestArguments
        wrapper: parent.autotestWrapper
    }
}
