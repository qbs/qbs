Project {
    name: "MyProject"
    property string projectName: name

    Product {
        name: "merge_lists"
        Depends { name: "dummyqt"; submodules: ["gui", "network"] }
        Depends { name: "dummy" }
        dummy.defines: ["THE_PRODUCT"]
    }
    Product {
        name: "merge_lists_and_values"
        Depends { name: "dummyqt"; submodules: ["network", "gui"] }
        Depends { name: "dummy" }
        dummy.defines: "THE_PRODUCT"
    }
    Product {
        name: "merge_lists_with_duplicates"
        Depends { name: "dummy" }
        dummy.cxxFlags: ["-foo", "BAR", "-foo", "BAZ"]
    }
    Product {
        name: "merge_lists_with_prototype_values"
        Depends { name: "dummyqt"; submodules: ["gui", "network"] }
        Depends { name: "dummy" }
    }

    Product {
        name: "list_property_that_references_product"
        type: ["blubb"]
        Depends { name: "dummy" }
        dummy.listProp: ["x"]
    }

    Product {
        name: "list_property_depending_on_overridden_property"
        Depends { name: "dummy" }
        dummy.listProp2: ["PRODUCT_STUFF"]
        dummy.controllingProp: true
    }

    Product {
        name: "overridden_list_property"
        Depends { name: "dummy" }
        Properties {
            condition: true
            overrideListProperties: true
            dummy.listProp: ["PRODUCT_STUFF"]
        }
    }

    Product {
        name: "shadowed-list-property"
        property string productName: name
        Depends { name: "dummy" }
        dummy.defines: [projectName, productName]
    }

    Product {
        name: "shadowed-scalar-property"
        property string productName: name
        Depends { name: "dummy" }
        dummy.someString: projectName + "_" + productName
    }
    Product {
        name: "merged-varlist"
        property string productName: name
        Depends { name: "dummy" }
        Depends { name: "dummyqt.core" }
        dummy.controllingProp: true
        dummy.varListProp: ({d: "product"})
    }
}
