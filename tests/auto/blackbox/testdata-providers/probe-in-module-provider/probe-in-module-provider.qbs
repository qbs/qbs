Product {
    qbsModuleProviders: ["provider_a"]
    name: "p"
    Depends { name: "qbsmetatestmodule" }
    property bool dummy: {
        console.info("p.qbsmetatestmodule.boolProp: " + JSON.stringify(qbsmetatestmodule.boolProp));
    }
}
