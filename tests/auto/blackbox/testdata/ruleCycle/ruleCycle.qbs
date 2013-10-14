import qbs 1.0

Project {
    Product {
        name: "the cycle of life"
        type: "cow"
        Group {
            files: ["happy.grass"]
            fileTags: ["grass"]
        }
        Rule {
            inputs: ["grass"]
            Artifact {
                fileName: input.completeBaseName + ".cow"
                fileTags: ["cow"]
            }
            prepare: { print("The cow feeds on grass."); }
        }
        Rule {
            inputs: ["cow"]
            Artifact {
                fileName: input.completeBaseName + ".cow_pat"
                fileTags: ["cow_pat"]
            }
            prepare: { print("The cow pat falls out of the cow."); }
        }
        Rule {
            inputs: ["cow_pat"]
            Artifact {
                fileName: input.completeBaseName + ".fertilizer"
                fileTags: ["fertilizer"]
            }
            prepare: { print("The cow pat is used as fertilizer."); }
        }
        Rule {
            inputs: ["fertilizer"]
            Artifact {
                fileName: input.completeBaseName + ".grass"
                fileTags: ["grass"]
            }
            prepare: { print("The fertilizer lets the grass grow."); }
        }
    }
}

