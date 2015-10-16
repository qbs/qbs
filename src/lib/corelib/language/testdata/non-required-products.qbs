import qbs

Project {
    Product {
        name: "depender"
        Depends { name: "dummy" }
        Depends { name: "dependee"; required: false }
        Properties {
            condition: dependee.present
            dummy.defines: ["WITH_DEPENDEE"]
        }
    }
    Project {
        name: "subproject"
        Product {
            name: "dependee"
        }
    }
}
