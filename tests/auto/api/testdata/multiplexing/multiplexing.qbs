import qbs.TextFile

Project {
    Project {
        name: "subproject 1"
        Product {
            name: "multiplex-using-export"
            multiplexByQbsProperties: ["architectures"]
            qbs.architectures: ["TRS-80", "C64"]
            Depends { name: "multiplex-with-export" }
        }
        Product {
            name: "multiplex-without-aggregator-2-depend-on-non-multiplexed"
            multiplexByQbsProperties: ["architectures"]
            qbs.architectures: ["TRS-80", "C64"]
            Depends { name: "no-multiplexing" }
        }
        Product {
            name: "multiplex-with-aggregator-2"
            aggregate: true
            multiplexByQbsProperties: ["architectures"]
            qbs.architectures: ["TRS-80", "C64"]
            qbs.architecture: "Atari ST"
        }
        Product {
            name: "multiplex-with-aggregator-2-dependent"
            Depends { name: "multiplex-with-aggregator-2" }
        }
        Product {
            name: "non-multiplexed-with-dependencies-on-multiplexed"
            Depends { name: "multiplex-without-aggregator-2" }
        }
        Product {
            name: "non-multiplexed-with-dependencies-on-multiplexed-via-export1"
            Depends { name: "multiplex-with-export" }
        }
        Product {
            name: "non-multiplexed-with-dependencies-on-multiplexed-via-export2"
            Depends { name: "nonmultiplex-with-export" }
        }
        Product {
            name: "non-multiplexed-with-dependencies-on-aggregation-via-export"
            Depends { name: "nonmultiplex-exporting-aggregation" }
        }
    }

    Project {
        name: "subproject 2"
        Product {
            name: "no-multiplexing"
        }
        Product {
            name: "multiplex-without-aggregator-2"
            multiplexByQbsProperties: ["architectures"]
            qbs.architectures: ["TRS-80", "C64"]
        }
        Product {
            name: "multiplex-with-export"
            multiplexByQbsProperties: ["architectures"]
            qbs.architectures: ["TRS-80", "C64"]
            Export { Depends { name: "multiplex-without-aggregator-2" } }
        }
        Product {
            name: "nonmultiplex-with-export"
            Export { Depends { name: "multiplex-without-aggregator-2" } }
        }
        Product {
            name: "nonmultiplex-exporting-aggregation"
            Export { Depends { name: "multiplex-with-aggregator-2" } }
        }
        Product {
            name: "multiplex-without-aggregator-4"
            multiplexByQbsProperties: ["architectures", "buildVariants"]
            qbs.architectures: ["TRS-80", "C64"]
            qbs.buildVariants: ["debug", "release"]
        }
        Product {
            name: "multiplex-without-aggregator-4-depends-2"
            multiplexByQbsProperties: ["architectures", "buildVariants"]
            qbs.architectures: ["TRS-80", "C64"]
            qbs.buildVariants: ["debug", "release"]
            Depends { name: "multiplex-without-aggregator-2" }
        }
    }

    Product {
        name: "aggregate-with-dependencies-on-aggregation-via-export"
        Depends { name: "nonmultiplex-exporting-aggregation" }
        aggregate: true
        multiplexByQbsProperties: ["architectures"]
        qbs.architectures: ["TRS-80", "C64"]
        qbs.architecture: "Atari ST"
    }
}
