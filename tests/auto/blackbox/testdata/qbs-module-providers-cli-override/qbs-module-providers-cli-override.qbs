Project {
    name: "project"
    Project {
        name: "innerProject"
        Product {
            name: "product"
            Depends { name: "qbsmetatestmodule"; required: false }
            property bool dummy: {
                console.info("qbsmetatestmodule.prop: " + qbsmetatestmodule.prop);
            }
        }
    }
}
