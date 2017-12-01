import qbs 1.0

Project {
    minimumQbsVersion: "1.6"
    qbsSearchPaths: ["qbs-resources"]
    property bool withCode: true
    property bool withDocker: true
    property bool withDocumentation: true
    property bool withExamples: false
    property bool withTests: withCode
    property stringList autotestArguments: []
    property stringList autotestWrapper: []

    references: [
        "share/share.qbs",
        "scripts/scripts.qbs",
    ]

    SubProject {
        filePath: "docker/docker.qbs"
        Properties {
            condition: parent.withDocker
        }
    }

    SubProject {
        filePath: "doc/doc.qbs"
        Properties {
            condition: parent.withDocumentation
        }
    }

    SubProject {
        filePath: "examples/examples.qbs"
        Properties {
            condition: parent.withExamples
        }
    }

    SubProject {
        filePath: "src/src.qbs"
        Properties {
            condition: parent.withCode
        }
    }

    SubProject {
        filePath: "tests/tests.qbs"
        Properties {
            condition: parent.withTests
        }
    }

    Product {
        name: "version"
        files: ["VERSION"]
    }

    Product {
        name: "qmake project files for qbs"
        files: ["**/*.pr[io]"]
    }
}
