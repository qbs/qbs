Project {
    property bool overrideFromModule
    property bool overrideFromExport
    property bool overrideFromProduct

    Product {
        name: "dep"
        Export {
            Depends {
                name: "higher";
                condition: project.overrideFromExport
                lower.param: "fromExportDepends"
            }
            Parameters { lower.param: "fromParameters" }
        }
    }
    Product {
        name: "main"

        Depends {
            name: "dep"
            condition: project.overrideFromProduct
            lower.param: "fromProductDepends"
        }
        Depends {
            name: "higher"
            condition: project.overrideFromProduct
            lower.param: "fromProductDepends"
        }
        Depends { name: "dep"; condition: !project.overrideFromProduct }
        Depends { name: "higher"; condition: !project.overrideFromProduct }
        Depends { name: "highest" }
        Depends { name: "broken"; required: false }
    }
}
