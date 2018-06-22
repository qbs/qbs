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

    Product {
        name: "p1"
        condition: p2.present
        Depends { name: "p2"; required: false }
    }
    Product {
        name: "p2"
        condition: p3.present
        Depends { name: "p3"; required: false }
    }
    Product {
        name: "p3"
        condition: nosuchmodule.present
        Depends { name: "nosuchmodule"; required: false }
    }
}
