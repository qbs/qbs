Project {
    qbsModuleProviders: "provider_a"
    name: "project"
    Project {
        name: "innerProject"
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

    Product {
        name: "p2"
        Depends { name: "qbsmetatestmodule" }
        Depends { name: "qbsothermodule"; required: false }
        property bool dummy: {
            console.info("p2.qbsmetatestmodule.prop: " + qbsmetatestmodule.prop);
            console.info("p2.qbsothermodule.prop: " + qbsothermodule.prop);
        }
    }
}
