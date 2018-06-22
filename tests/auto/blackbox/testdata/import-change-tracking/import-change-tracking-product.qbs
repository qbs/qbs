import "irrelevant.js" as Irrelevant
import "custom1prepare1.js" as Custom1Prepare
import "custom2prepare" as Custom2Prepare
import "probe1.js" as ProbeFunc

Product {
    name: "customProduct"
    type: ["custom1", "custom2"]

    property string irrelevant: Irrelevant.irrelevant()
    property string dummy: probe1.result

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

    Probe {
        id: probe1
        property string input: Irrelevant.irrelevant()
        property string result
        configure: {
            found = true;
            console.info("running probe1");
            return ProbeFunc.probe1Func();
        }
    }

    Probe {
        id: probe2
        configure: {
            console.info("running probe2");
            found = true;
        }
    }
}
