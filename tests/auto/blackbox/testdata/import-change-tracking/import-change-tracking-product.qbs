import qbs
import "irrelevant.js" as Irrelevant
import "custom1prepare1.js" as Custom1Prepare
import "custom2prepare" as Custom2Prepare

Product {
    name: "customProduct"
    type: ["custom1", "custom2"]

    property string irrelevant: Irrelevant.irrelevant()

    Group {
        files: "input1.txt"
        fileTags: "input.custom1"
    }

    Group {
        files: "input2.txt"
        fileTags: "input.custom2"
    }

    Rule {
        inputs: "input.custom1"
        Artifact { filePath: "dummy.custom1"; fileTags: "custom1" }
        prepare: {
            return Custom1Prepare.prepare();
        }
    }

    Rule {
        inputs: "input.custom2"
        Artifact { filePath: "dummy.custom2"; fileTags: "custom2" }
        prepare: {
            return Custom2Prepare.prepare();
        }
    }
}
