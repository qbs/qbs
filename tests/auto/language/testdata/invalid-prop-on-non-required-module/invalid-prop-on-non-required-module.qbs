Project {
    property bool useExistingModule

    Product {
        name: "a"
        condition: project.useExistingModule
        Depends { name: "deploader" }
        Depends { name: "dep" }
        dep.nosuchprop: true
    }

    Product {
        name: "b"
        condition: !project.useExistingModule
        Depends { name: "deploader" }
        Depends { name: "random"; required: false }
        random.nosuchprop: true
    }
}
