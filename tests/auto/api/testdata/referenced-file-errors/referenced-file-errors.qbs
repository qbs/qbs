Project {
    references: [
        "ambiguousdir",
        "cycle.qbs",
        "emptydir",
        "nosuchfile.qbs",
        "okay.qbs",
        "wrongtype.qbs",
    ]

    SubProject {
        filePath: "cycle.qbs"
        Properties {
            productName: "p3"
        }
    }

    SubProject {
        filePath: "nosuchfile.qbs"
    }

    SubProject {
        filePath: "okay2.qbs"
    }

    Product {
        name: "p5"
        Depends { name: "brokenmodule" }
    }
}
