Project {
    Product {
        name: "p1"
        Depends { name: "qbsmetatestmodule" }
        Depends { name: "qbsothermodule" }
        Depends { name: "nonexistentmodule"; required: false }
        property bool dummy: {
            console.info("p1.qbsmetatestmodule.prop: " + qbsmetatestmodule.prop);
            console.info("p1.qbsothermodule.prop: " + qbsothermodule.prop);
        }
        qbsModuleProviders: "provider_a"
    }
}
