import qbs
import qbs.TextFile

Project {
    Project {
        name: "subproject 1"
        Product {
            name: "multiplex-using-export"
            type: ["reversed-text"]
            files: ["foo.txt"]
            multiplexByQbsProperties: ["architectures"]
            qbs.architectures: ["TRS-80", "C64"]
            Depends { name: "multiplex-with-export" }
        }
        Product {
            name: "multiplex-without-aggregator-2-depend-on-non-multiplexed"
            type: ["reversed-text"]
            files: ["foo.txt"]
            multiplexByQbsProperties: ["architectures"]
            qbs.architectures: ["TRS-80", "C64"]
            Depends { name: "no-multiplexing" }
        }
        Product {
            name: "multiplex-with-aggregator-2"
            type: ["reversed-text"]
            files: ["foo.txt"]
            aggregate: true
            multiplexByQbsProperties: ["architectures"]
            qbs.architectures: ["TRS-80", "C64"]
            qbs.architecture: "Atari ST"
        }
        Product {
            name: "multiplex-with-aggregator-2-dependent"
            Depends { name: "multiplex-with-aggregator-2" }
            type: ["something"]
            files: ["foo.txt"]
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
            type: ["reversed-text"]
            files: ["foo.txt"]
        }
        Product {
            name: "multiplex-without-aggregator-2"
            type: ["reversed-text"]
            files: ["foo.txt"]
            multiplexByQbsProperties: ["architectures"]
            qbs.architectures: ["TRS-80", "C64"]
        }
        Product {
            name: "multiplex-with-export"
            type: ["reversed-text"]
            files: ["foo.txt"]
            multiplexByQbsProperties: ["architectures"]
            qbs.architectures: ["TRS-80", "C64"]
            Export { Depends { name: "multiplex-without-aggregator-2" } }
        }
        Product {
            name: "nonmultiplex-with-export"
            type: ["reversed-text"]
            files: ["foo.txt"]
            Export { Depends { name: "multiplex-without-aggregator-2" } }
        }
        Product {
            name: "nonmultiplex-exporting-aggregation"
            type: ["reversed-text"]
            files: ["foo.txt"]
            Export { Depends { name: "multiplex-with-aggregator-2" } }
        }
        Product {
            name: "multiplex-without-aggregator-4"
            type: ["reversed-text"]
            files: ["foo.txt"]
            multiplexByQbsProperties: ["architectures", "buildVariants"]
            qbs.architectures: ["TRS-80", "C64"]
            qbs.buildVariants: ["debug", "release"]
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
    FileTagger {
        patterns: ["*.txt"]
        fileTags: ["text"]
    }
    Rule {
        inputs: ["text"]
        Artifact {
            filePath: input.baseName + ".txet"
            fileTags: ["reversed-text"]
        }
        prepare: {
            var cmd = new JavaScriptCommand();
            cmd.description = "reversing text";
            cmd.sourceCode = function() {
                var tf = new TextFile(input.filePath, TextFile.ReadOnly);
                var content = tf.readAll();
                tf.close();
                tf = new TextFile(output.filePath, TextFile.WriteOnly);
                tf.write(content.split("").reverse().join(""));
                tf.close();
            };
            return cmd;
        }
    }
}
