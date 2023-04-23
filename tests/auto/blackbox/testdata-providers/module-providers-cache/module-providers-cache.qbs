Project {
    qbsModuleProviders: ["provider_a"]
    name: "project"
    property string dummyProp

    Product {
        name: "p1"
        Depends { name: "qbsothermodule" }
        Depends { name: "qbsmetatestmodule" }
    }
}
