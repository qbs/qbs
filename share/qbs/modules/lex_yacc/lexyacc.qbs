import "lexyacc.js" as HelperFunctions

Module {
    Depends { name: "cpp" }

    property bool enableCompilerWarnings: false
    property string lexBinary: "lex"
    property string yaccBinary: "yacc"
    property string outputTag: "c"
    property bool uniqueSymbolPrefix: false
    property string lexOutputFilePath
    property string yaccOutputFilePath
    property stringList lexFlags: []
    property stringList yaccFlags: []

    readonly property string outputDir: product.buildDirectory + "/lexyacc"

    Rule {
        inputs: ["lex.input"]
        outputFileTags: [product.lex_yacc.outputTag]
        outputArtifacts: HelperFunctions.lexOutputArtifacts(product, input)
        prepare: HelperFunctions.lexCommands.apply(HelperFunctions, arguments)
    }

    Rule {
        inputs: ["yacc.input"]
        outputFileTags: [product.lex_yacc.outputTag, "hpp"]
        outputArtifacts: HelperFunctions.yaccOutputArtifacts(product, input)
        prepare: HelperFunctions.yaccCommands.apply(HelperFunctions, arguments)
    }

    FileTagger {
        patterns: ["*.l"]
        fileTags: ["lex.input"];
    }
    FileTagger {
        patterns: ["*.y"]
        fileTags: ["yacc.input"];
    }
}
