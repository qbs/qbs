Project {
    name: "project"
    qbsModuleProviders: ["provider_a", "provider_b"]
    Product {
        name: "p1"
        Depends { name: "qbsmetatestmodule" }
        Depends { name: "qbsothermodule"; required: false }
        property bool dummy: {
            console.info("p1.qbsmetatestmodule.prop: " + qbsmetatestmodule.prop);
            console.info("p1.qbsothermodule.prop: " + qbsothermodule.prop);
        }
    }
}
