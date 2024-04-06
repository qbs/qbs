Project {
    property string version: "1.0.0"
    property bool withTests: true
    property bool installDebugInformation: true
    property stringList autotestArguments: []
    property stringList autotestWrapper: []

    name: "My Project"
    minimumQbsVersion: "2.0"
    //! [0]
    // myproject.qbs
    references: [
        "app/app.qbs",
        "lib/lib.qbs",
        "version-header/version-header.qbs",
    ]
    //! [0]
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
