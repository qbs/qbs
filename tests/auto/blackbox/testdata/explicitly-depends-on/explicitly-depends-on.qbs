import qbs.File
import qbs.TextFile

Project {
    // Cases:
    // step1 +             in-product final -> step2 -> step3 -> final => rule cycle
    // step1 +             dependency final -> step2 -> step3 -> final => ok
    // step1 + module filesAreTargets final -> step2 -> step3 -> final => ok

    name: "proj1"
    property bool useModule: false

    Product {
        name: "prod1"
        type: "final"
        property bool useExplicitlyDependsOn: false
        property bool useExplicitlyDependsOnFromDependencies: false

        Depends {
            condition: !project.useModule
            name: "prod2"
        }
        Depends {
            condition: project.useModule
            name: "module1"
        }

        Group {
            files: ["step1.txt"]
            fileTags: ["step1"]
        }

        Rule {
            inputs: ["step3"]
            outputFileTags: ["final"]
            Artifact {
                filePath: "final.txt"
                fileTags: ["final"]
            }
            prepare: {
                var cmd = new JavaScriptCommand();
                cmd.description = "step3 -> final";
                cmd.sourceCode = function() {
                    File.copy(input.filePath, output.filePath);
                };
                return cmd;
            }
        }

        Rule {
            inputs: ["step2"]
            outputFileTags: ["step3"]
            Artifact {
                filePath: "step3.txt"
                fileTags: ["step3"]
            }
            prepare: {
                var cmd = new JavaScriptCommand();
                cmd.description = "step2 -> step3";
                cmd.sourceCode = function() {
                    File.copy(input.filePath, output.filePath);
                };
                return cmd;
            }
        }

        Rule {
            inputs: ["step1"]
            outputFileTags: ["step2"]
            Artifact {
                filePath: "step2.txt"
                fileTags: ["step2"]
            }

            Properties {
                condition: useExplicitlyDependsOn
                explicitlyDependsOn: ["final"]
            }

            Properties {
                condition: useExplicitlyDependsOnFromDependencies
                explicitlyDependsOnFromDependencies: ["final"]
            }

            prepare: {
                var cmd = new JavaScriptCommand();
                cmd.description = "step1 -> step2";
                cmd.sourceCode = function() {
                    console.info("Using explicitlyDependsOnArtifact: "
                                 + explicitlyDependsOn["final"][0].fileName)
                    File.copy(input.filePath, output.filePath);
                };
                return cmd;
            }
        }
    }

    Product {
        name: "prod2"
        type: "final"
        condition: !project.useModule

        Rule {
            multiplex: true
            requiresInputs: false
            outputFileTags: ["final"]
            Artifact {
                filePath: "product-fish.txt"
                fileTags: ["final"]
            }
            prepare: {
                var cmd = new JavaScriptCommand();
                cmd.description = "creating 'product-fish.txt' tagged with 'final'";
                cmd.sourceCode = function() {
                    var file = new TextFile(output.filePath, TextFile.ReadWrite);
                    file.write("Lots of fish.");
                    file.close()
                };
                return cmd;
            }
        }
    }
}
