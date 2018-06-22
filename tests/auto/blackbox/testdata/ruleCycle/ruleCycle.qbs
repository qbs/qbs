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
            outputFileTags: ["cow"]
            outputArtifacts: [{
                filePath: input.completeBaseName + ".cow",
                fileTags: ["cow"]
            }]
            prepare: { console.info("The cow feeds on grass."); }
        }
        Rule {
            inputs: ["cow"]
            Artifact {
                filePath: input.completeBaseName + ".cow_pat"
                fileTags: ["cow_pat"]
            }
            prepare: { console.info("The cow pat falls out of the cow."); }
        }
        Rule {
            inputs: ["cow_pat"]
            Artifact {
                filePath: input.completeBaseName + ".fertilizer"
                fileTags: ["fertilizer"]
            }
            prepare: { console.info("The cow pat is used as fertilizer."); }
        }
        Rule {
            inputs: ["fertilizer"]
            outputFileTags: ["grass"]
            outputArtifacts: [{
                filePath: input.completeBaseName + ".grass",
                fileTags: ["grass"]
            }]
            prepare: { console.info("The fertilizer lets the grass grow."); }
        }
    }
}

