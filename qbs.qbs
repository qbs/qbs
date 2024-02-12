Project {
    minimumQbsVersion: "1.6"
    qbsSearchPaths: ["qbs-resources"]
    property bool withCode: true
    property bool withDocumentation: true
    property bool withTests: withCode
    property stringList autotestArguments: []
    property stringList autotestWrapper: []

    references: [
        "docker/docker.qbs",
        "examples/examples.qbs",
        "share/share.qbs",
        "scripts/scripts.qbs",
        "tutorial/tutorial.qbs",
    ]

    SubProject {
        filePath: "doc/doc.qbs"
        Properties {
            condition: parent.withDocumentation
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
        name: "continuous integration files"
        files: [
            ".clang-tidy",
            "docker-compose.yml",
            ".github/**/*.yml",
        ]
    }
}
