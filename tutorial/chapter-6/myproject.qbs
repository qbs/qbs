//! [0]
Project {
    property string version: "1.0.0"
    property bool installDebugInformation: true
    property bool withTests: false
    property stringList autotestArguments: []
    property stringList autotestWrapper: []

    name: "My Project"
    minimumQbsVersion: "2.0"
    // ...
    //! [0]
    references: [
        "app/app.qbs",
        "lib/lib.qbs",
    ]
    qbsSearchPaths: "qbs"

    //! [1]
    SubProject {
        filePath: "test/test.qbs"
        Properties {
            condition: parent.withTests
        }
    }
    //! [1]

    //! [2]
    AutotestRunner {
        condition: parent.withTests
        arguments: parent.autotestArguments
        wrapper: parent.autotestWrapper
    }
    //! [2]
}
