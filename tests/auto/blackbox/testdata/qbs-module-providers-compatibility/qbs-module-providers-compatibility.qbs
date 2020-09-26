Product {
    name: "p"
    Depends { name: "qbsmetatestmodule" }
    property bool dummy: {
        console.info("qbsmetatestmodule.prop: " + qbsmetatestmodule.prop);
    }
}
