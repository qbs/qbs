import qbs 1.0
import 'blubb.js' as blubb

Product {
    type: ["schnusel"]
    Rule {
        multiplex: true
        Artifact {
            fileTags: ["schnusel"]
            filePath: "something"
        }
        prepare: {
            return blubb.func(output.filePath);
        }
    }
}

