Product {
    qbsModuleProviders: ["provider_a", "provider_b"]
    name: "p"
    Depends { name: "qbsmetatestmodule" }
    Depends { name: "qbsothermodule" }
    moduleProviders.provider_a.someProp: "someValue"
    property bool dummy: {
        console.info("p.qbsmetatestmodule.prop: " + qbsmetatestmodule.prop);
        console.info("p.qbsothermodule.prop: " + qbsothermodule.prop);
    }
}
