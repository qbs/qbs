Project {
    Product {
        name: "p1"
        Group {
            files: ["file1.txt"]
            qbs.install: true
        }
    }
    Product {
        name: "p2"
        Group {
            files: ["file1.txt"]
            qbs.install: true
        }
    }
    Product {
        name: "p3"
        Group {
            files: ["file2.txt"]
            qbs.install: true
        }
        multiplexByQbsProperties: ["buildVariants"]
        qbs.buildVariants: ["debug", "release"]
    }
    Product {
        name: "p4"
        Group {
            files: ["file2.txt"]
            qbs.install: true
        }
        aggregate: true
        multiplexByQbsProperties: ["buildVariants"]
        qbs.buildVariants: ["debug", "release"]
    }
}
